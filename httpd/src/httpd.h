#ifndef HTTPD_H_
#define HTTPD_H_


#define ENTER_FUNCTION debug("ENTER %s", __FUNCTION__);
#define LEAVE_FUNCTION debug("LEAVE %s", __FUNCTION__);

#define PUBLIC
#define PRIVATE static

#define FUNCTION(name, type, scope, args, body) \
  scope type name args \
  { \
    ENTER_FUNCTION \
    body \
    LEAVE_FUNCTION \
  }

#define _COMMA ,

#ifdef DEBUG

#define DEBUG_WRAP(code) \
		if (((!isChild)&&(LOG_PARENT))|| \
				((isChild)&&(LOG_CHILD))) { \
			code; \
		}

#define debug(fmt, ...) \
	{ \
		DEBUG_WRAP( \
			fprintf(getOutFile(), "[%s %d](FUNCTION: %s, FILE %s, LINE: %d): " fmt "\n", \
				isChild ? "CHILD" : "PARENT", getpid(), \
				__FUNCTION__, __FILE__, __LINE__, \
				##__VA_ARGS__); \
		) \
	}
#else
#define DEBUG_WRAP(code)
#define debug(fmt, ...)
#endif

typedef struct _keyValuePair {
	char *key;
	char *value;
} keyValuePair_t;

typedef void (*teardownFunc_t)();
typedef void (*setupFunc_t)(GList *keyValuePairs, err_t *e);
typedef void (*handlerFunc_t)(char *url, int fromParent, int toParent,
		GList *requestHeaders, GList *requestParameters, gboolean *responseQueued,
		err_t *e);
typedef void (*authenticateFunc_t)(struct MHD_Connection *connection, err_t *e);
typedef void (*startRegistryFunc_t)(err_t *e);





char *getValue(GList *keyValuePairs, char *key, err_t *e);
void respondFromFile(int toParent, uint32_t status, char *path,
		gboolean *responseQueued, err_t *e);
void writeBytes(int fd, void *buffer, uint32_t len, err_t *e);

void respondFromBuffer(int toParent, uint32_t status, void *buffer,
		uint32_t len, gboolean *responseQueued, err_t *e);
void registerPlugin(char *name,
		setupFunc_t setup,
		teardownFunc_t teardown,
		handlerFunc_t putHandler,
		handlerFunc_t postHandler,
		handlerFunc_t getHandler,
		handlerFunc_t deleteHandler,
		authenticateFunc_t authenticate,
		err_t *e);

#endif /* HTTPD_H_ */
