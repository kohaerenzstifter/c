/*
 * httpd.h
 *
 *  Created on: Jun 18, 2013
 *      Author: sancho
 */

#ifndef HTTPD_H_
#define HTTPD_H_

#define mark() debug("");

#ifdef DEBUG
#define debug(fmt, ...) \
	{ \
		if (((!isChild)&&(LOG_PARENT))|| \
				((isChild)&&(LOG_CHILD))) { \
			FILE *file = (outFile != NULL) ? outFile : stdout; \
			fprintf(file, "[%s %d](FUNCTION: %s, FILE %s, LINE: %d): " fmt "\n", \
				isChild ? "CHILD" : "PARENT", getpid(), \
				__FUNCTION__, __FILE__, __LINE__, \
				##__VA_ARGS__); \
		} \
	}
#else
#define debug(fmt, ...)
#endif

typedef void (*handlerFunc_t)(char *url, int fromParent, int toParent,
		GList *requestHeaders, GList *requestParameters, gboolean *responseQueued,
		err_t *e);
typedef void (*authenticateFunc_t)(struct MHD_Connection *connection, err_t *e);
typedef void (*startRegistryFunc_t)(err_t *e);

char *getValue(GList *keyValuePairs, char *key, err_t *e);

void respondFromFile(int toParent, uint32_t status, char *path,
		gboolean *responseQueued, err_t *e);

void respondFromBuffer(int toParent, uint32_t status, void *buffer,
		uint32_t len, gboolean *responseQueued, err_t *e);

void registerPlugin(char *name,
		handlerFunc_t putHandler,
		handlerFunc_t postHandler,
		handlerFunc_t getHandler,
		handlerFunc_t deleteHandler,
		authenticateFunc_t authenticate,
		err_t *e);

#endif /* HTTPD_H_ */
