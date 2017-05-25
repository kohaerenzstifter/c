/*
 ============================================================================
 Name        : httpd.c
 Author      : Martin Knappe (martin.knappe@gmail.com)
 Version     : 1.0.1
 Copyright   : Your copyright notice
 Description : Simple preforking http(s) server made with libmycrohttpd
 ============================================================================
 */

#include <signal.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <microhttpd.h>
#include <unistd.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <openssl/rsa.h>
#include <linux/limits.h>
#include <dlfcn.h>

#include <kohaerenzstiftung.h>

//#define DEBUG_PARENT 1
//#define DEBUG_CHILD 1

#if defined DEBUG_PARENT
#define DEBUG 1
#elif defined DEBUG_CHILD
#define DEBUG 1
#endif

#ifdef DEBUG_PARENT
#define LOG_PARENT TRUE
#else
#define LOG_PARENT FALSE
#endif

#ifdef DEBUG_CHILD
#define LOG_CHILD TRUE
#else
#define LOG_CHILD FALSE
#endif

#include "httpd.h"

#define STATE_DEAD 0
#define STATE_IDLE 1
#define STATE_BUSY 2

#define isIdle(handler) \
		(((handler)->state == STATE_IDLE)&&((handler)->sigTermAt == -1))


#define PIPE_READ 0
#define PIPE_WRITE 1

#define TIME_TO_FINISH 2

struct _connection;

typedef struct _handler {
	int sigTermAt; //UNIX timestamp (when did we send SIGTERM to this child?)
	int toHandler;
	int fromHandler;
	pid_t pid;
	int state;
	uint32_t requestsHandled;
	struct _connection *connection;
} handler_t;

static handler_t *handlers = NULL;
static char *configFile = NULL;

static GList *responseHeaders = NULL;

typedef void (*authenticateFunc_t)(struct MHD_Connection *connection, err_t *e);

typedef struct _service {
	setupFunc_t setup;
	teardownFunc_t teardown;
	char *name;
	handlerFunc_t putHandler;
	handlerFunc_t postHandler;
	handlerFunc_t getHandler;
	handlerFunc_t deleteHandler;
	authenticateFunc_t authenticate;
	gboolean isSetup;
} service_t;

typedef struct _connection {
	handler_t *handler;
	handlerFunc_t func;
	struct MHD_Connection *con;
	char *url;
	char *method;
	char *version;
	struct MHD_PostProcessor *postProcessor;
	struct {
		gboolean queued;
		struct MHD_Response *value;
		uint32_t status;
	} response;
} connection_t;

static volatile gboolean sigAlrm = FALSE;
static volatile gboolean sigTerm = FALSE;
static gboolean isChild = FALSE;
static gboolean doDaemonize = FALSE;
static gboolean stopReforking = FALSE;
static int32_t minHandlers = -1;
static int32_t maxHandlers = -1;
static char *logDirectory = NULL;
static char *pluginDirectory = NULL;
static time_t reloadPluginsAt = 0;
//static sigset_t signalsBlocked;
//static sigset_t signalsAllowed;



FUNCTION(writeBytes, void, PUBLIC, (int fd, void *buffer, uint32_t len,
  err_t *e),
	uint32_t bytesPending = len;
	void *cur = buffer;
	int32_t justWritten = 0;

	while (bytesPending > 0) {
		justWritten = write(fd, cur, bytesPending);
		if (justWritten < 0) {
			terror(failIfFalse((errno == EINTR)))
					continue;
		}
		bytesPending -= justWritten;
		cur += justWritten;
	}
finish:
	return;
)

FUNCTION(writeStatus, void, PRIVATE, (int toParent, uint32_t status, err_t *e),
	terror(writeBytes(toParent, &status, sizeof(status), e))
finish:
	return;
)

 
FUNCTION(writeKeyValuePair, void, PRIVATE, (int toParent,
  keyValuePair_t *keyValuePair, err_t *e),
	uint32_t keySize = 0;
	uint32_t valueSize = 0;

	keySize = strlen(keyValuePair->key) + 1;
	valueSize = strlen(keyValuePair->value) + 1;
	terror(writeBytes(toParent, &keySize, sizeof(keySize), e))
	terror(writeBytes(toParent, &valueSize, sizeof(valueSize), e))
	terror(writeBytes(toParent, keyValuePair->key, keySize, e))
	terror(writeBytes(toParent, keyValuePair->value, valueSize, e))
finish:
	return;
)

  
FUNCTION(writeKeyValuePairs, void, PRIVATE, (GList *keyValuePairs,
  int toParent, err_t *e),
	uint32_t count = 0;
	GList *cur = NULL;
	keyValuePair_t *keyValuePair = NULL;

	if (keyValuePairs != NULL) {
		count = g_list_length(keyValuePairs);
	}
	terror(writeBytes(toParent, &count, sizeof(count), e))
	for (cur = keyValuePairs; cur != NULL; cur = g_list_next(cur)) {
		keyValuePair = (keyValuePair_t *) (cur->data);
		terror(writeKeyValuePair(toParent, keyValuePair, e))
	}
finish:
	return;
)

 
FUNCTION(respondFromFile, void, PUBLIC, (int toParent, uint32_t status,
  char *path, gboolean *responseQueued, err_t *e),
	size_t justRead = -1;
	int fd = -1;
	char buffer[4096];

	terror(failIfFalse(((fd = open(path, O_RDONLY)) >= 0)))

	*responseQueued = TRUE;
	terror(writeStatus(toParent, status, e))
	terror(writeKeyValuePairs(responseHeaders, toParent, e))

	for(;;) {
		terror(failIfFalse((justRead = read(fd, buffer, sizeof(buffer))) >= 0))
				if (justRead < 1) {
					if (justRead < 0) {
						terror(failIfFalse(errno == EINTR))
						continue;
					}
					break;
				}
		terror(writeBytes(toParent, buffer, justRead, e))
	}
finish:
	if (fd >= 0) {
		close(fd);
	}
	return;
)

FUNCTION(respondFromBuffer, void, PUBLIC, (int toParent, uint32_t status,
  void *buffer, uint32_t len, gboolean *responseQueued, err_t *e),

	*responseQueued = TRUE;

	terror(writeStatus(toParent, status, e))
	terror(writeKeyValuePairs(responseHeaders, toParent, e))
	terror(writeBytes(toParent, buffer, len, e))

finish:
	return;
)

FUNCTION(respondFromError, void, PRIVATE, (int toParent, err_t *err),
	err_t error;
	err_t *e = &error;
	char *errString = NULL;

	initErr(e);
	terror(writeStatus(toParent, hasFailed(err) ?
			MHD_HTTP_INTERNAL_SERVER_ERROR : MHD_HTTP_OK, e));
	terror(writeKeyValuePairs(responseHeaders, toParent, e));
	errString = err2string(err);
	terror(writeBytes(toParent, errString, strlen(errString) + 1, e))
finish:
	return;
)

FUNCTION(getValue, char *, PUBLIC, (GList *keyValuePairs, char *key, err_t *e),
	GList *cur = NULL;
	keyValuePair_t *keyValuePair = NULL;
	keyValuePair_t *theKeyValuePair = NULL;
	char *result = NULL;

	for (cur = g_list_first(keyValuePairs);
			cur != NULL; cur = g_list_next(cur)) {
		keyValuePair = (keyValuePair_t *) cur->data;

		if (strcmp(keyValuePair->key, key) == 0) {
			theKeyValuePair = keyValuePair;
			break;
		}
	}

	terror(failIfFalse(theKeyValuePair != NULL))
	result = theKeyValuePair->value;

finish:
	return result;
)

FUNCTION(readBytes, void, PRIVATE, (int fd, void *buffer,
  uint32_t len, err_t *e),
	uint32_t bytesPending = len;
	void *cur = buffer;
	int32_t justRead = 0;

	while (bytesPending > 0) {
		justRead = read(fd, cur, bytesPending);
		if (justRead < 1) {
			terror(failIfFalse(((justRead == 0)||
				(errno == EINTR)||(errno == EAGAIN))))
					continue;
		}
		bytesPending -= justRead;
		cur += justRead;
	}
finish:
	return;
)

FUNCTION(freeKeyValuePair, void, PRIVATE, (keyValuePair_t *keyValuePair),
	free(keyValuePair->key);
	free(keyValuePair->value);
	free(keyValuePair);
)

FUNCTION(freeKeyValuePairs, void, PRIVATE, (GList *headers),
	GList *cur = NULL;
	keyValuePair_t *keyValuePair = NULL;

	for (cur = headers; cur != NULL; cur = g_list_next(cur)) {
		keyValuePair = (keyValuePair_t *) cur->data;
		freeKeyValuePair(keyValuePair);
	}
	if (headers != NULL) {
		g_list_free(headers);
	}
)

FUNCTION(isUrlEncoded, gboolean, PRIVATE, (GList *requestHeaders, err_t *e),
	char *contentType = NULL;
	gboolean result = FALSE;

	terror(contentType =
			getValue(requestHeaders, "Content-Type", e))
	result = (strcmp(contentType,
			"application/x-www-form-urlencoded") == 0);
finish:
	return result;
)

FUNCTION(readKeyValuePair, keyValuePair_t *, PRIVATE, (int fd, err_t *e),
	keyValuePair_t *_result = NULL;
	keyValuePair_t *result = NULL;
	uint32_t keyLength = 0;
	uint32_t valueLength = 0;

	terror(readBytes(fd, &keyLength, sizeof(keyLength), e))
	terror(readBytes(fd, &valueLength, sizeof(valueLength), e))
	_result = calloc(1, sizeof(keyValuePair_t));
	_result->key = calloc(1, keyLength);
	_result->value = calloc(1, valueLength);
	terror(readBytes(fd, _result->key, keyLength, e))
	terror(readBytes(fd, _result->value, valueLength, e))
	result = _result; _result = NULL;
finish:
	if (_result != NULL) {
		freeKeyValuePair(_result);
	}
	return result;
)

FUNCTION(readKeyValuePairs, GList *, PRIVATE, (int fd, err_t *e),
	GList *result = NULL;
	GList *_result = NULL;
	uint32_t count = 0;
	keyValuePair_t *keyValuePair = NULL;
	uint32_t i = 0;

	terror(readBytes(fd, &count, sizeof(count), e))

	for (i = 0; i < count; i++) {
		terror(keyValuePair = readKeyValuePair(fd, e))
				_result = g_list_append(_result, keyValuePair);
	}
	result = _result; _result = NULL;
finish:
	if (_result != NULL) {
		freeKeyValuePairs(_result);
	}
	return result;
)

static GList *libraries = NULL;
static GList *tmpServices = NULL;
static GList *services = NULL;


FUNCTION(processSignal, void, PRIVATE, (int signum),
	if (signum == SIGALRM) {
		sigAlrm = TRUE;
	} else {
		sigTerm = TRUE;
	}
)

FUNCTION(doSignals, void, PRIVATE, (void),
	sigset_t signalsBlocked;
	struct sigaction action;

	sigfillset(&signalsBlocked);

	action.sa_handler = &processSignal;
	action.sa_mask = signalsBlocked;
	action.sa_flags = 0;

	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGALRM, &action, NULL);

	action.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &action, NULL);
)

FUNCTION(validateRequest, handlerFunc_t, PRIVATE, (struct MHD_Connection *con,
  char *url, char **sUrl, const char *method, const char *version, err_t *e),
	handlerFunc_t result = NULL;
	service_t *service = NULL;
	service_t *theService = NULL;
	char *serviceUrl = NULL;
	gchar **split = NULL;
	GList *cur = NULL;

	split = g_strsplit(url, "/", 3);
	serviceUrl = (split[1] != NULL) ? strdup(split[1]) : strdup("");

	for (cur = services; cur != NULL; cur = g_list_next(cur)) {
		service = cur->data;
		if (strcmp(service->name, serviceUrl) == 0) {
			terror(failIfFalse(service->isSetup))
			theService = service;
			break;
		}
	}

	terror(failIfFalse(theService != NULL))

	if (theService->authenticate != NULL) {
		terror(theService->authenticate(con, e))
	}

	if (strcmp(method, MHD_HTTP_METHOD_PUT) == 0) {
		result = theService->putHandler;
	} else if (strcmp(method, MHD_HTTP_METHOD_POST) == 0) {
		result = theService->postHandler;
	} else if (strcmp(method, MHD_HTTP_METHOD_GET) == 0) {
		result = theService->getHandler;
	} else if (strcmp(method, MHD_HTTP_METHOD_DELETE) == 0) {
		result = theService->deleteHandler;
	}

	terror(failIfFalse(result != NULL))

	*sUrl = (split[1] == NULL) ? strdup("") : (split[2] == NULL) ?
			strdup("") : strdup(split[2]);
finish:
	free(serviceUrl);
	g_strfreev(split);
	return result;
)

FUNCTION(postDataIterator, int, PRIVATE, (void *cls, enum MHD_ValueKind kind,
  const char *key, const char *filename, const char *content_type,
  const char *transfer_encoding, const char *data, uint64_t off, size_t size),
	connection_t *connection = (connection_t *) cls;
	GList *keyValuePairs = NULL;
	handler_t *handler = NULL;
	int result = MHD_NO;
	keyValuePair_t keyValuePair;
	err_t err;
	err_t *e = &err;

	initErr(e);

	handler = connection->handler;
	if (handler == NULL) {
		goto finish;
	}

	keyValuePair.key = (char *) key;
	keyValuePair.value = (char *) data;

	keyValuePairs = g_list_append(NULL, &keyValuePair);

	terror(writeKeyValuePairs(keyValuePairs, handler->toHandler, e))

	result = MHD_YES;
finish:
	if (keyValuePairs != NULL) {
		g_list_free(keyValuePairs);
	}
	return result;
)

FUNCTION(handleNewRequest, int, PRIVATE, (struct MHD_Connection *con,
  const char *url, const char *method, const char *version,
  void **con_cls, err_t *e),
	int result = MHD_NO;
	handlerFunc_t func;
	char *shortUrl = NULL;
	connection_t *connection = NULL;

	terror(func =
	  validateRequest(con, (char *) url, &shortUrl, method, version, e))

	connection = calloc(1, sizeof(connection_t));
	connection->handler = NULL;
	connection->func = func;
	connection->con = con;
	connection->url = shortUrl; shortUrl = NULL;
	connection->method = strdup(method);
	connection->version = strdup(version);
	connection->postProcessor = NULL;
	connection->response.queued = FALSE;
	connection->response.value = NULL;
	connection->response.status = 0;

	*con_cls = connection;

	result = MHD_YES;
finish:
	free(shortUrl);
	return result;
)

FUNCTION(initHandler, void, PRIVATE, (handler_t *handler),
	handler->sigTermAt = -1;
	if (handler->toHandler >= 0) {
		close(handler->toHandler); handler->toHandler = -1;
	}
	if (handler->fromHandler >= 0) {
		close(handler->fromHandler); handler->fromHandler = -1;
	}
	handler->pid = -1;
	handler->state = STATE_DEAD;
	handler->requestsHandled = 0;
	handler->connection = NULL;

	return;
)

FUNCTION(queueResponse, void, PRIVATE, (connection_t *connection,
  uint32_t status, void* data, uint32_t size),
	struct MHD_Response *response = NULL;

	if (connection->response.value != NULL) {
		goto finish;
	}
	response = MHD_create_response_from_data(size, data, FALSE, TRUE);

	connection->response.value = response;
	connection->response.status = status;

finish:
	return;
)

FUNCTION(waitForHandler, gboolean, PRIVATE, (handler_t *handler,
  gboolean hang),
	gboolean result = FALSE;
	int status = 0;
	int options = hang ? 0 : WNOHANG;
	connection_t *connection = NULL;
	err_t error;
	err_t *e = &error;
	char *errString = NULL;

	if (handler->pid < 1) {
		result = TRUE;
		goto finish;
	}

	if (waitpid(handler->pid, &status, options) == handler->pid) {
		result = TRUE;
	}
	if (!result) {
		goto finish;
	}

	connection = handler->connection;
	if (connection == NULL) {
		goto finish;
	}
	handler->connection = NULL;
	connection->handler = NULL;
	if (connection->response.value != NULL) {
		goto finish;
	}
	initErr(e);
	failIfFalse(FALSE);
	errString = err2string(e);
	queueResponse(connection, MHD_HTTP_INTERNAL_SERVER_ERROR,
			errString, strlen(errString));


finish:
	if (result) {
		initHandler(handler);
	}
	return result;
)

FUNCTION(sendSignal, void, PRIVATE, (handler_t *handler, gboolean sigKill),
	if (!sigKill) {
		if (handler->sigTermAt == -1) {
			kill(handler->pid, SIGTERM);
			handler->sigTermAt = time(NULL);
			if (handler->toHandler != -1) {
				close(handler->toHandler); handler->toHandler = -1;
			}
		}
	} else {
		kill(handler->pid, SIGKILL);
	}
)

FUNCTION(writeUrl, void, PRIVATE, (char *url, int fd, err_t *e),
	uint32_t len = strlen(url) + 1;
	terror(writeBytes(fd, &len, sizeof(len), e))
	terror(writeBytes(fd, url, len, e))
finish:
	return;
)

FUNCTION(writeFunc, void, PRIVATE, (handlerFunc_t func, int fd, err_t *e),
	terror(writeBytes(fd, &func, sizeof(func), e))
finish:
	return;
)

FUNCTION(notifyHandler, void, PRIVATE, (handler_t *handler, handlerFunc_t func,
  char *url, GList *requestHeaders, GList *requestParameters, err_t *e),
	terror(writeFunc(func, handler->toHandler, e))
	terror(writeUrl(url, handler->toHandler, e))
	terror(writeKeyValuePairs(requestHeaders, handler->toHandler, e))
	terror(writeKeyValuePairs(requestParameters, handler->toHandler, e))

finish:
	if (hasFailed(e)) {
		sendSignal(handler, FALSE);
	}
)

FUNCTION(readUrl, char *, PRIVATE, (int fd, err_t *e),
	char *result = NULL;
	char *_result = NULL;
	uint32_t length = 0;

	terror(readBytes(fd, &length, sizeof(length), e))
	_result = calloc(1, length);
	terror(readBytes(fd, _result, length, e))
	result = _result; _result = NULL;

finish:
	free(_result);
	return result;
)

FUNCTION(readFunc, handlerFunc_t, PRIVATE, (int fd, err_t *e),
	handlerFunc_t result = NULL;

	terror(readBytes(fd, &result, sizeof(result), e));
finish:
	return result;
)

FUNCTION(execRequest, void, PRIVATE, (handlerFunc_t func, char *url,
  int fromParent,int toParent, GList *requestHeaders,
  GList *requestParameters),
	gboolean responseQueued = FALSE;
	err_t error;
	err_t *e = &error;

	initErr(e);

	func(url, fromParent, toParent, requestHeaders,
			requestParameters, &responseQueued, e);
	if (!responseQueued) {
		respondFromError(toParent, e);
	}
)

FUNCTION(handlerFunc, void, PRIVATE, (int fromParent, int toParent),
	handlerFunc_t func = NULL;
	char *url = NULL;
	err_t error;
	err_t *e = &error;
	GList *requestHeaders = NULL;
	GList *requestParameters = NULL;
	gboolean alarmSet = FALSE;

	initErr(e);
	while (!sigAlrm) {
		if (sigTerm) {
			sigTerm = FALSE;
			if (!alarmSet) {
				alarm(2);
				alarmSet = TRUE;
			}
		}
		free(url); url = NULL;
		if (requestHeaders != NULL) {
			freeKeyValuePairs(requestHeaders); requestHeaders = NULL;
		}
		if (responseHeaders != NULL) {
			freeKeyValuePairs(responseHeaders); responseHeaders = NULL;
		}
		terror(func = readFunc(fromParent, e))
		terror(url = readUrl(fromParent, e))
		terror(requestHeaders = readKeyValuePairs(fromParent, e))
		terror(requestParameters = readKeyValuePairs(fromParent, e))
		//TODO: what about post processing?
		execRequest(func, url, fromParent, toParent, requestHeaders,
		  requestParameters);
	}
finish:
	if (requestHeaders != NULL) {
		freeKeyValuePairs(requestHeaders);
	}
	if (responseHeaders != NULL) {
		freeKeyValuePairs(responseHeaders);
	}
	free(url);
	return;
)

FUNCTION(forkHandler, void, PRIVATE, (handler_t *handler, err_t *e),
	pid_t pid = -1;
	int pipe1[2] = {-1 _COMMA -1};
	int pipe2[2] = {-1 _COMMA -1};
	int fromParent = -1;
	int toParent = -1;

	if (stopReforking) {
		goto finish;
	}
	terror(failIfFalse(pipe(pipe1) == 0))
	terror(failIfFalse(pipe(pipe2) == 0))
	terror(failIfFalse((pid = fork()) >= 0))
	if (pid > 0) {
		//parent process
		close(pipe2[PIPE_READ]);
		close(pipe1[PIPE_WRITE]);

		handler->connection = NULL;
		handler->pid = pid;
		handler->sigTermAt = -1;
		handler->state = STATE_IDLE;

		handler->fromHandler = pipe1[PIPE_READ];
		pipe1[PIPE_READ] = -1;

		handler->toHandler = pipe2[PIPE_WRITE];
		pipe2[PIPE_WRITE] = -1;

		fcntl(handler->fromHandler, F_SETFL, O_NONBLOCK);
		fcntl(handler->toHandler, F_SETFL, O_NONBLOCK);
	} else {
		//child process
		isChild = TRUE;
		close(pipe1[PIPE_READ]);
		close(pipe2[PIPE_WRITE]);

		fromParent = pipe2[PIPE_READ];
		pipe2[PIPE_READ] = -1;

		toParent = pipe1[PIPE_WRITE];
		pipe1[PIPE_WRITE] = -1;

		handlerFunc(fromParent, toParent);
		exit(0);
	}

finish:
	close(pipe1[PIPE_READ]);
	close(pipe1[PIPE_WRITE]);
	close(pipe2[PIPE_READ]);
	close(pipe2[PIPE_WRITE]);
	return;
)

FUNCTION(forkHandlers, void, PRIVATE, (uint32_t howMany, err_t *e),
	int i = 0;
	int forked = 0;

	for (i = 0; i < maxHandlers; i++) {
		if (forked >= howMany) {
			break;
		}
		if (handlers[i].state == STATE_DEAD) {
			terror(forkHandler(&handlers[i], e));
			forked++;
		}
	}
finish:
	return;
)

FUNCTION(addKeyValuePair, int, PRIVATE, (void *cls, enum MHD_ValueKind kind,
  const char *key, const char *value),
	GList **keyValuePairs = (GList **) cls;
	keyValuePair_t *keyValuePair = calloc(1, sizeof(keyValuePair_t));

	keyValuePair->key = strdup(key);
	keyValuePair->value = strdup(value);

	*keyValuePairs = g_list_append(*keyValuePairs, keyValuePair);

	return MHD_YES;
)

FUNCTION(getRequestHeaders, GList *, PRIVATE, (struct MHD_Connection *con),
	GList *result = NULL;

	MHD_get_connection_values(con, MHD_HEADER_KIND,
			&addKeyValuePair, &result);

	return result;
)

FUNCTION(getRequestParameters, GList *, PRIVATE, (struct MHD_Connection *con),
	GList *result = NULL;

	MHD_get_connection_values(con, MHD_GET_ARGUMENT_KIND,
			&addKeyValuePair, &result);

	return result;
)

FUNCTION(assignHandler, gboolean, PRIVATE, (connection_t *connection,
  gboolean mayFork, err_t *e),
	gboolean result = FALSE;
	int i = 0;
	gboolean foundHandler = FALSE;
	uint32_t dead = 0;
	GList *requestHeaders = NULL;
	GList *requestParameters = NULL;

	for (i = 0; i < maxHandlers; i++) {
		if (isIdle(&handlers[i])) {
			connection->handler = &handlers[i];
			connection->handler->connection = connection;
			requestHeaders = getRequestHeaders(connection->con);
			requestParameters = getRequestParameters(connection->con);
			terror(notifyHandler(connection->handler, connection->func,
					connection->url, requestHeaders, requestParameters, e))
			connection->handler->state = STATE_BUSY;
			foundHandler = TRUE;
			break;
		} else if (handlers[i].state == STATE_DEAD) {
			dead++;
		}
	}

	if (!foundHandler) {
		terror(failIfFalse(mayFork))
		terror(failIfFalse(dead > 0))
		terror(forkHandlers(1, e))
		terror(result = assignHandler(connection, FALSE, e))
		goto finish;
	}

	if (strcmp(connection->method, MHD_HTTP_METHOD_POST) == 0) {
		terror(result = isUrlEncoded(requestHeaders, e))
	}

finish:
	if (requestHeaders != NULL) {
		freeKeyValuePairs(requestHeaders);
	}
	if (requestParameters != NULL) {
		freeKeyValuePairs(requestParameters);
	}
	return result;
)

FUNCTION(feedDataToChild, void, PRIVATE, (connection_t *connection,
  const char *upload_data, size_t *upload_data_size, err_t *e),
	char *errString = NULL;

	terror(writeBytes(connection->handler->toHandler, (char *) upload_data,
			*upload_data_size, e))


finish:
	if (hasFailed(e)) {
		errString = err2string(e);
		queueResponse(connection, MHD_HTTP_INTERNAL_SERVER_ERROR,
				errString, strlen(errString));
	} else {
		*upload_data_size = 0;
	}
	return;
)

FUNCTION(handleExistingRequest, int, PRIVATE, (connection_t *connection,
  const char *upload_data, size_t *upload_data_size, err_t *e),
	handler_t *handler = NULL;
	int result = MHD_NO;
	gboolean doCreatePostProcessor = FALSE;

	if (connection->response.value == NULL) {
		if (connection->handler == NULL) {
			terror(doCreatePostProcessor =
			  assignHandler(connection, TRUE, e))
			if (doCreatePostProcessor) {
				connection->postProcessor =
				  MHD_create_post_processor(connection->con, 65536,
				  &postDataIterator, connection);
			}
		} else if ((*upload_data_size) > 0) {
			if (connection->postProcessor == NULL) {
				terror(feedDataToChild(connection, upload_data,
						upload_data_size, e))
			} else {
				MHD_post_process(connection->postProcessor,
						upload_data, *upload_data_size);
				*upload_data_size = 0;
			}
		} else {
			if (connection->postProcessor != NULL) {
				handler = connection->handler;
				if (handler != NULL) {
					terror(writeKeyValuePairs(NULL,
							handler->toHandler, e))
				}
			} else {
				//TODO: how to tell child there's no more data?
			}
		}
	} else {
		if (!connection->response.queued) {
			MHD_queue_response(connection->con,
					connection->response.status,
					connection->response.value);
			connection->response.queued = TRUE;
		}
	}

	result = MHD_YES;
finish:
	return result;
)

FUNCTION(handleRequest, int, PRIVATE, (void *cls, struct MHD_Connection *con,
  const char *url, const char *method, const char *version,
  const char *upload_data, size_t *upload_data_size, void **con_cls),
	int result = MHD_NO;
	connection_t *connection = NULL;
	err_t error;
	err_t *e = &error;

	initErr(e);
	if (*con_cls == NULL) {
		terror(result = handleNewRequest(con, url, method, version, con_cls, e))
	} else {
		connection = (connection_t *) *con_cls;
		terror(result = handleExistingRequest(connection, upload_data,
		  upload_data_size, e))
	}

finish:
	return result;
)

FUNCTION(finishConnection, void, PRIVATE, (connection_t *connection),
	handler_t *handler = connection->handler;
	if (handler != NULL) {
		handler->connection = NULL;
	}
	connection->handler = NULL;
	free(connection->url);
	free(connection->method);
	free(connection->version);
	if (connection->postProcessor != NULL) {
		MHD_destroy_post_processor(connection->postProcessor);
	}
	if (connection->response.value != NULL) {
		MHD_destroy_response(connection->response.value);
	}
	free(connection);
)

FUNCTION(killChild, void, PRIVATE, (gboolean idleOnly),
	int i = 0;
	uint32_t requestsHandledMax = 0;

	for (i = 0; i < maxHandlers; i++) {
		if (idleOnly&&!isIdle(&handlers[i])) {
			continue;
		}
		if (handlers[i].state == STATE_DEAD) {
			continue;
		}
		if (handlers[i].sigTermAt != -1) {
			continue;
		}
		if (requestsHandledMax < handlers[i].requestsHandled) {
			requestsHandledMax = handlers[i].requestsHandled;
		}
	}

	for (i = 0; i < maxHandlers; i++) {
		if (idleOnly&&!isIdle(&handlers[i])) {
			continue;
		}
		if (handlers[i].state == STATE_DEAD) {
			continue;
		}
		if (handlers[i].sigTermAt != -1) {
			continue;
		}
		if (handlers[i].requestsHandled == requestsHandledMax) {
			sendSignal(&handlers[i], FALSE);
			break;
		}
	}
)

FUNCTION(killChildren, void, PRIVATE, (uint32_t soMany,
  gboolean idleOnly, err_t *e),
	uint32_t i = 0;

	for (i = 0; i < soMany; i++) {
		killChild(idleOnly);
	}

)

FUNCTION(minMaxChildren, void, PRIVATE, (err_t *e),
	defineError();

	uint32_t idleChildren = 0;
	uint32_t forkedChildren = 0;
	uint32_t dyingChildren = 0;
	uint32_t deadChildren = 0;
	uint32_t soMany = 0;
	int i = 0;

	for (i = 0; i < maxHandlers; i++) {
		if (isIdle(&handlers[i])) {
			idleChildren++;
			forkedChildren++;
		} else if (handlers[i].state == STATE_DEAD) {
			deadChildren++;
		} else if (handlers[i].sigTermAt != -1) {
			dyingChildren++;
		} else {
			forkedChildren++;
		}
	}

	if (forkedChildren < minHandlers) {
		soMany = minHandlers - forkedChildren;
		terror(forkHandlers(soMany, e))
	} else if ((idleChildren > 1)&&(forkedChildren > minHandlers)) {
		soMany = forkedChildren - minHandlers;
		soMany = (idleChildren >= soMany) ? soMany : idleChildren;
		//let's not kill handlers just because
//		terror(killChildren(soMany, TRUE, e))
	}
finish:
	return;
)

FUNCTION(onRequestDone, void, PRIVATE, (void *cls, struct MHD_Connection *con,
  void **con_cls, enum MHD_RequestTerminationCode toe),
	connection_t *connection = (connection_t *) *con_cls;
	handler_t *handler = NULL;

	if (connection == NULL) {
		goto finish;
	}

	handler = connection->handler;
	if (handler != NULL) {
		//this is an undesirable situation: the request is over
		//for whatever reason, but the handler isn't yet done
		//sending a reponse
		sendSignal(handler, FALSE);
	}

	finishConnection(connection);

finish:
	return;
)

FUNCTION(daemonLogger, void, PRIVATE, (void *cls, const char *fmt, va_list va),
	DEBUG_WRAP(
		vfprintf(getOutFile(), fmt, va);
	)
)

FUNCTION(startDaemon, struct MHD_Daemon *, PRIVATE, (uint16_t port,
  char *caCert, char *serverCert, char *serverKey, err_t *e),
	struct MHD_Daemon *result = NULL;
#ifdef DEBUG
	uint32_t flags = MHD_USE_DEBUG;
#else
	uint32_t flags = 0;
#endif
	initErr(e);

	if (caCert != NULL) {
		flags |= MHD_USE_SSL;
		result = MHD_start_daemon(flags,
				port,
				NULL,
				NULL,
				&handleRequest,
				NULL,
				MHD_OPTION_NOTIFY_COMPLETED, &onRequestDone, NULL,
				MHD_OPTION_HTTPS_MEM_CERT, serverCert,
				MHD_OPTION_HTTPS_MEM_KEY, serverKey,
				MHD_OPTION_HTTPS_MEM_TRUST, caCert,
				MHD_OPTION_EXTERNAL_LOGGER, &daemonLogger, NULL,
				MHD_OPTION_END);
	} else {
		result = MHD_start_daemon(flags,
				port,
				NULL,
				NULL,
				&handleRequest,
				NULL,
				MHD_OPTION_NOTIFY_COMPLETED, &onRequestDone, NULL,
				MHD_OPTION_EXTERNAL_LOGGER, &daemonLogger, NULL,
				MHD_OPTION_END);
	}

	terror(failIfFalse(result != NULL))
finish:
	return result;
)

FUNCTION(resetHandler, void, PRIVATE, (handler_t *handler),
	handler->state = STATE_IDLE;
	if (handler->connection != NULL) {
		handler->connection->handler = NULL;
		handler->connection = NULL;
	}
)

FUNCTION(contentReaderCallback, ssize_t, PRIVATE, (void *cls, uint64_t pos,
  char *buf, size_t max),
	int result = 0;
	connection_t *connection = cls;
	int32_t justRead = 0;
	struct timeval timeval;
	fd_set rs;
	fd_set ws;
	int nfds = -1;
	handler_t *handler = (connection == NULL) ? NULL :
			connection->handler;

	if (handler == NULL) {
		result = -2; //end with error
		goto finish;
	}
	FD_ZERO(&rs);
	FD_ZERO(&ws);
	FD_SET(handler->toHandler, &ws);
	FD_SET(handler->fromHandler, &rs);
	timeval.tv_sec = 0;
	timeval.tv_usec = 1000;
	nfds = handler->toHandler > handler->fromHandler ?
			handler->toHandler : handler->fromHandler;
	nfds++;
	select(nfds, &rs, &ws, NULL, &timeval);
	if (FD_ISSET(handler->fromHandler, &rs)) {
		justRead = read(handler->fromHandler, buf, max);
		if (justRead >= 0) {
			result = justRead;
		} else {
			if (errno == EINTR) {
				result = 0;
			} else {
				result = -1;
			}
		}
	} else if (FD_ISSET(handler->toHandler, &ws)) {
		result = -1; //end of stream
		resetHandler(handler);
		handler->requestsHandled++;
	}
finish:
	return result;
)

FUNCTION(readStatus, uint32_t, PRIVATE, (int fd, err_t *e),
	uint32_t result = 0;
	terror(readBytes(fd, &result, sizeof(result), e))
finish:
	return result;
)

FUNCTION(queueResponseForHandler, void, PRIVATE, (handler_t *handler),
	GList *responseHeaders = NULL;
	err_t error;
	err_t *e = &error;
	struct MHD_Response *response = NULL;
	char *errString = NULL;
	uint32_t status = 0;
	GList *cur = NULL;

	initErr(e);

	if (handler->connection->response.value != NULL) {
		goto finish;
	}

	terror(status = readStatus(handler->fromHandler, e))
	terror(responseHeaders = readKeyValuePairs(handler->fromHandler, e))

	response =
			MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, 1024,
					&contentReaderCallback, handler->connection, NULL);

	for (cur = responseHeaders; cur != NULL; cur = g_list_next(cur)) {
		keyValuePair_t *keyValuePair = cur->data;
		MHD_add_response_header(response,
				keyValuePair->key, keyValuePair->value);
	}

	handler->connection->response.value = response; response = NULL;
	handler->connection->response.status = status;

finish:
	if (responseHeaders != NULL) {
		freeKeyValuePairs(responseHeaders);
	}
	if (hasFailed(e)) {
		errString = err2string(e);
		queueResponse(handler->connection, MHD_HTTP_INTERNAL_SERVER_ERROR,
				errString, strlen(errString));
		sendSignal(handler, FALSE);
	}
	if (response != NULL) {
		MHD_destroy_response(response);
	}
)

FUNCTION(reapChildren, gboolean, PRIVATE, (void),
	gboolean childrenLeft = FALSE;
	int i = 0;
	gboolean mustDieNow = FALSE;

	for (i = 0; i < maxHandlers; i++) {
		if (handlers[i].state == STATE_DEAD) {
			continue;
		}
		mustDieNow = ((handlers[i].sigTermAt != -1)&&
				((time(NULL) - handlers[i].sigTermAt) >= TIME_TO_FINISH));

		if (!mustDieNow) {
			waitForHandler(&handlers[i], FALSE);
		} else {
			sendSignal(&(handlers[i]), TRUE);
			while (!waitForHandler(&(handlers[i]), TRUE));
		}
		if (handlers[i].state != STATE_DEAD) {
			childrenLeft = TRUE;
		}
	}

	return childrenLeft;
)

FUNCTION(freeService, void, PRIVATE, (service_t *service),
	free(service->name);
	free(service);
)

FUNCTION(freeServices, void, PRIVATE, (GList *services),
	GList *cur = NULL;

	for (cur = services; cur != NULL; cur = g_list_next(cur)) {
		service_t *service = cur->data;
		if (service->teardown != NULL) {
			service->teardown();
		}
		freeService(cur->data);
	}
	g_list_free(services);
)

FUNCTION(freeLibraries, void, PRIVATE, (GList *services),
	GList *cur = NULL;

	for (cur = services; cur != NULL; cur = g_list_next(cur)) {
		dlclose(cur->data);
	}
	g_list_free(services);
)

FUNCTION(unloadPlugins, void, PRIVATE, (void),
	freeServices(services);
	freeLibraries(libraries);
)

FUNCTION(getKeyValuePairs, GList *, PRIVATE, (GKeyFile *keyFile, char *name, err_t *e),
	defineError();

	GList *_result = NULL;
	GList *result = NULL;
	gchar **keys = NULL;
	gsize length = 0;
	int i = 0;
	gchar *value = NULL;

	keys = g_key_file_get_keys(keyFile, name, &length, NULL);

	if (keys != NULL) {
		for (i = 0; i < length; i++) {
			keyValuePair_t *keyValuePair = NULL;

			free(value); value = NULL;
			terror(failIfFalse((value = g_key_file_get_value(keyFile,
					name, keys[i], NULL)) != NULL))
			keyValuePair = calloc(1, sizeof(keyValuePair_t));
			keyValuePair->key = strdup(keys[i]);
			keyValuePair->value = strdup(value);

			_result = g_list_append(_result, keyValuePair);
		}
	}

	result = _result; _result = NULL;
finish:
	if (value != NULL) {
		free(value);
	}
	if (_result != NULL) {
		freeKeyValuePairs(_result);
	}
	if (keys != NULL) {
		g_strfreev(keys);
	}
	return result;
)

FUNCTION(setupService, void, PRIVATE, (service_t *service, GKeyFile *keyFile, err_t *e),
	GList *keyValuePairs = NULL;

	terror(keyValuePairs = getKeyValuePairs(keyFile, service->name, NULL))
	if (keyValuePairs == NULL) {
		goto finish;
	}

	terror(service->setup(keyValuePairs, e))
	service->isSetup = TRUE;

finish:
	if (keyValuePairs != NULL) {
		freeKeyValuePairs(keyValuePairs);
	}
)

FUNCTION(reloadPlugins, gboolean, PRIVATE, (GKeyFile *keyFile, err_t *e),
	defineError();

	gboolean result = FALSE;
	DIR *directory = NULL;
	struct dirent *dirent = NULL;
	size_t length = -1;
	char path[PATH_MAX];
	void *library = NULL;
	startRegistryFunc_t startRegistryFunc = NULL;
	GList *startRegistryFuncs = NULL;
	GList *tmpLibraries = NULL;
	GList *cur = NULL;

	directory = opendir(pluginDirectory);
	terror(failIfFalse(directory != NULL))
	for (dirent = readdir(directory); dirent != NULL;
			dirent = readdir(directory)) {
		length = strlen(dirent->d_name);
		if (length < 3) {
			continue;
		}
		if (strcmp(".so", &(dirent->d_name[length - 3])) != 0) {
			continue;
		}
		snprintf(path, sizeof(path), "%s/%s", pluginDirectory, dirent->d_name);
		library = dlopen(path, RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
		terror(failIfFalse(library != NULL))
		tmpLibraries = g_list_append(tmpLibraries, library);
		startRegistryFunc = dlsym(library,  "startRegistry");
		terror(failIfFalse(startRegistryFunc != NULL))
		startRegistryFuncs = g_list_append(startRegistryFuncs, startRegistryFunc);
	}
	for (cur = startRegistryFuncs; cur != NULL; cur = g_list_next(cur)) {
		startRegistryFunc = cur->data;
		terror(startRegistryFunc(e))
	}
	unloadPlugins();

	for (cur = tmpServices; cur != NULL; cur = g_list_next(cur)) {
		service_t *service = cur->data;
		if (service->setup != NULL) {
			terror(setupService(service, keyFile, e))
		}
	}

	libraries = tmpLibraries; tmpLibraries = NULL;
	services = tmpServices; tmpServices = NULL;
	result = TRUE;
finish:
	freeServices(tmpServices); tmpServices = NULL;
	freeLibraries(tmpLibraries);
	g_list_free(tmpLibraries);
	g_list_free(startRegistryFuncs);
	reloadPluginsAt = time(NULL) + 3600;
	if (directory != NULL) {
		closedir(directory);
	}
	return result;
)

 
FUNCTION(registerPlugin, void, PUBLIC, (char *name, setupFunc_t setup, teardownFunc_t teardown, handlerFunc_t putHandler, handlerFunc_t postHandler, handlerFunc_t getHandler, handlerFunc_t deleteHandler, authenticateFunc_t authenticate, err_t *e),
	GList *cur = NULL;
	service_t *service = NULL;

	terror(failIfFalse(strcmp(name, "main") != 0))

	for (cur = tmpServices; cur != NULL; cur = g_list_next(cur)) {
		service = cur->data;
		terror(failIfFalse((strcmp(service->name, name) != 0)))
	}
	service = calloc(1, sizeof(service_t));
	service->setup = setup;
	service->teardown = teardown;
	service->authenticate = authenticate;
	service->deleteHandler = deleteHandler;
	service->getHandler = getHandler;
	service->name = strdup(name);
	service->postHandler = postHandler;
	service->putHandler = putHandler;
	service->isSetup = FALSE;

	tmpServices = g_list_append(tmpServices, service);

finish:
	return;
)

FUNCTION(main, int, PUBLIC, (int argc, char *argv[]),
	struct timeval tv;
	int nfds = 0;
	struct MHD_Daemon *d = NULL;
	fd_set rs;
	fd_set ws;
	fd_set es;
	int i = 0;
	err_t error;
	err_t *e = &error;
	GOptionContext *optionContext = NULL;
	GError *gerror = NULL;
	char outPath[PATH_MAX];
	char errPath[PATH_MAX];
	char *progName = NULL;
	char *lastSlash = NULL;
	char *caCert = NULL;
	char *serverCert = NULL;
	char *serverKey = NULL;
	char *caCertPath = NULL;
	char *serverCertPath = NULL;
	char *serverKeyPath = NULL;
	int32_t port = 0;
	gboolean childrenLeft = FALSE;
	gboolean shuttingDown = FALSE;
	GKeyFile *keyFile = NULL;
	GKeyFile *tmpKeyFile = NULL;
	GList *keyValuePairs = NULL;
	char *value = NULL;

	GOptionEntry optionEntries[] = {
			{
					"configFile" _COMMA
					'c' _COMMA
					0 _COMMA
					G_OPTION_ARG_STRING _COMMA
					&configFile _COMMA
					"configFile" _COMMA
					NULL
			} _COMMA
			{
					"daemonize" _COMMA
					'd' _COMMA
					0 _COMMA
					G_OPTION_ARG_NONE _COMMA
					&doDaemonize _COMMA
					"daemonize" _COMMA
					NULL
			} _COMMA
			{
					"logDirectory" _COMMA
					'l' _COMMA
					0 _COMMA
					G_OPTION_ARG_STRING _COMMA
					&logDirectory _COMMA
					"logDirectory" _COMMA
					NULL
			} _COMMA
			{
					NULL
			}
	};

	initErr(e);

	lastSlash = strrchr(argv[0], '/');
	progName = (lastSlash == NULL) ? argv[0] : (lastSlash + 1);

	optionContext = g_option_context_new("");
	g_option_context_add_main_entries(optionContext,
			optionEntries, NULL);
	terror(failIfFalse(g_option_context_parse(optionContext,
			&argc, &argv, &gerror)))

	terror(failIfFalse(configFile != NULL))

	keyFile = g_key_file_new();
	terror(failIfFalse(g_key_file_load_from_file(keyFile,
			configFile, G_KEY_FILE_NONE, NULL)))
	terror(keyValuePairs = getKeyValuePairs(keyFile, "main", e))

	terror(value = getValue(keyValuePairs, "minHandlers", e))
	terror(minHandlers = parseInteger(value, TRUE, e))
	terror(value = getValue(keyValuePairs, "maxHandlers", e))
	terror(maxHandlers = parseInteger(value, TRUE, e))
	terror(value = getValue(keyValuePairs, "port", e))
	terror(port = parseInteger(value, TRUE, e))
	terror(value = getValue(keyValuePairs, "pluginDirectory", e))
	pluginDirectory = strdup(value);
	terror(value = getValue(keyValuePairs, "secure", e))
	if (strcmp(value, "yes") == 0) {
		terror(value = getValue(keyValuePairs, "caCertPath", e))
		caCertPath = strdup(value);
		terror(value = getValue(keyValuePairs, "serverCertPath", e))
		serverCertPath = strdup(value);
		terror(value = getValue(keyValuePairs, "serverKeyPath", e))
		serverKeyPath = strdup(value);
	}

	terror(failIfFalse(minHandlers >= 0))
	terror(failIfFalse(maxHandlers >= minHandlers))
	terror(failIfFalse(pluginDirectory != NULL))

	if (caCertPath != NULL) {
		terror(failIfFalse(serverCertPath != NULL))
				terror(failIfFalse(serverKeyPath != NULL))

				terror(caCert = readFile(caCertPath, e))
				terror(serverCert = readFile(serverCertPath, e))
				terror(serverKey = readFile(serverKeyPath, e))
	} else {
		terror(failIfFalse(serverCertPath == NULL))
				terror(failIfFalse(serverKeyPath == NULL))
	}

	if (doDaemonize) {
		if (logDirectory == NULL) {
			logDirectory = strdup("/var/log");
		}
		snprintf(outPath, sizeof(outPath), "%s/%s.out",
				logDirectory, progName);
		snprintf(errPath, sizeof(errPath), "%s/%s.err",
				logDirectory, progName);
		terror(setOutput(outPath, errPath, e))
		terror(daemonize(e))
	} else {
		terror(failIfFalse(logDirectory == NULL))
	}

	doSignals();

	terror(failIfFalse((handlers =
			calloc(maxHandlers, sizeof(handler_t))) != NULL))
	for(i = 0; i < maxHandlers; i++) {
		handlers[i].toHandler = -1;
		handlers[i].fromHandler = -1;
		initHandler(&handlers[i]);
	}

	terror(d = startDaemon(port, caCert, serverCert, serverKey, e));

	reloadPluginsAt = time(NULL);
	while (!(shuttingDown&&(!childrenLeft))) {
		if (sigTerm) {
			stopReforking = TRUE;
			sigTerm = FALSE;
			if (!shuttingDown) {
				killChildren(maxHandlers, FALSE, NULL);
				alarm(2);
				shuttingDown = TRUE;
			}
		}
		if (time(NULL) >= reloadPluginsAt) {
			tmpKeyFile = g_key_file_new();
			if (g_key_file_load_from_file(tmpKeyFile,
				configFile, G_KEY_FILE_NONE, NULL)) {
				//we need to kill all child processes
				//when we reload the plugins because we need to
				//be sure that the children have the correct plugins
				//loaded
				terror(killChildren(maxHandlers, FALSE, e))
				if (reloadPlugins(tmpKeyFile, NULL)) {
					g_key_file_free(keyFile);
					keyFile = tmpKeyFile;
				}
			}
		}
		childrenLeft = reapChildren();
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		nfds = 0;
		FD_ZERO(&rs);
		FD_ZERO(&ws);
		FD_ZERO(&es);
		if (MHD_get_fdset(d, &rs, &ws, &es, &nfds) != MHD_YES) {
			fprintf(getErrFile(), "unable to MHD_get_fdset()\n");
			sleep(5);
			continue;
		}
		for (i = 0; i < maxHandlers; i++) {
			if ((handlers[i].state == STATE_BUSY)&&
					(handlers[i].connection != NULL)) {
				nfds = (nfds < handlers[i].fromHandler) ?
						handlers[i].fromHandler : nfds;
				FD_SET(handlers[i].fromHandler, &rs);
			}
		}
		select(nfds + 1, &rs, &ws, &es, &tv);
		for (i = 0; i < maxHandlers; i++) {
			if ((handlers[i].state == STATE_BUSY)&&
					(handlers[i].connection != NULL)&&
					(handlers[i].connection->response.value == NULL)) {
				if (FD_ISSET(handlers[i].fromHandler, &rs)) {
					queueResponseForHandler(&handlers[i]);
				}
			}
		}
		terror(minMaxChildren(e))
		MHD_run(d);
	}
finish:
	MHD_stop_daemon(d);
	if (keyValuePairs != NULL) {
		freeKeyValuePairs(keyValuePairs);
	}
	if (keyFile != NULL) {
		g_key_file_free(keyFile);
	}
	if (gerror != NULL) {
		g_error_free(gerror);
	}
	if (optionContext != NULL) {
		g_option_context_free(optionContext);
	}
	unloadPlugins();
	free(pluginDirectory);
	free(caCertPath);
	free(serverCertPath);
	free(serverKeyPath);
	free(configFile);
	free(handlers);
	free(logDirectory);
	free(caCert);
	free(serverCert);
	free(serverKey);
	return EXIT_SUCCESS;
)
