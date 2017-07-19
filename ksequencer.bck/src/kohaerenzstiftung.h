#ifndef KOHAERENZSTIFTUNG_H_
#define KOHAERENZSTIFTUNG_H_

#include <glib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>

#ifdef __cplusplus
	#include <string>
extern "C" {
#endif


typedef struct _err {
	gboolean failed;
#ifdef __cplusplus
	std::string file;
#else
	char *file;
#endif
	int line;
} err_t;

#define failIfFalse(c) do { \
	if (!(c)) { \
		e->failed = TRUE; \
		e->file = __FILE__; \
		e->line = __LINE__; \
	} \
} while(FALSE);

#define defineError() \
	err_t error; \
	if (e == NULL) { \
		e = &error; \
		initErr(e); \
	}

#define initErr(e) e->failed = FALSE;

#define hasFailed(e) (e->failed)

#define terror(c) do { \
		c; \
		if (hasFailed(e)) { \
			FILE *file = getErrFile(); \
			fprintf(file, "code %s in %s:%d failed: %s\n", #c, \
					__FILE__, __LINE__, err2string(e)); \
			goto finish; \
		} \
} while(FALSE);

#define PIPE_RD 0
#define PIPE_WR 1

char *err2string(err_t *e);

char *strrstr(const char *haystack, const char *needle);

int runSync(err_t *e, const char *format, ...)
	__attribute__ ((format (printf, 2, 3)));

int runSyncWithOutput(err_t *e, gchar **stdout, gchar **stderr,
	const char *format, ...) __attribute__ ((format (printf, 4, 5)));

int parseInteger(char *parseMe, gboolean isSigned, err_t *e);

void daemonize(err_t *e);

void setOutput(const char *out, const char *error, err_t *e);

char *readFile(char *path, err_t *e);

FILE *getErrFile(void);
FILE *getOutFile(void);

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#ifdef __cplusplus
};
#endif

#endif /* KOHAERENZSTIFTUNG_H_ */
