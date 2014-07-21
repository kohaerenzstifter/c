#include "kohaerenzstiftung.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

FILE *outFile = NULL;
FILE *errFile = NULL;

char *err2string(err_t *e)
{
	static char result[200];
	if (e->failed) {
		snprintf(result, sizeof(result), "Error in %s:%d (strerror: %s)", e->file, e->line, strerror(errno));
	} else {
		snprintf(result, sizeof(result), "No error");
	}
	return result;
}

char *strrstr(const char *haystack, const char *needle)
{
	int i = 0;
	char *result = NULL;
	int haystackLength = strlen(haystack);
	int needleLength = strlen(needle);
	int startIndex = 0;

	if (haystackLength < needleLength) {
		goto finish;
	}
	startIndex = haystackLength - needleLength;
	startIndex = (needleLength < 1) ? startIndex - 1 : startIndex;
	for (i = startIndex; i >= 0; i--) {
		result = strstr(&haystack[i], needle);
		if (result != NULL) {

			break;
		}
	}

finish:
	return result;
}

static void unblockSignals(gpointer user_data)
{
	sigset_t unblockedSignals;
	sigemptyset(&unblockedSignals);
	sigprocmask(SIG_SETMASK, &unblockedSignals, NULL);
}

static int doRunSyncWithOutput(char *command, gchar **stdoutBuf,
  gchar **stderrBuf, err_t *e)
{
	int result = 0;
	gchar *argv[] = {"/bin/sh", "-c", command, NULL};

	terror(failIfFalse(g_spawn_sync(NULL, argv, NULL, 0,
			&unblockSignals, NULL, stdoutBuf, stderrBuf, &result, NULL)))
finish:
	return result;
}

int runSyncWithOutput(err_t *e, gchar **stdoutBuf, gchar **stderrBuf,
  const char *format, ...)
{
	int result = 0;
	va_list vl;
	char buffer[500];

	va_start(vl, format);
	g_vsnprintf(buffer, sizeof(buffer), format, vl);
	va_end(vl);

	terror(result = doRunSyncWithOutput(buffer, stdoutBuf, stderrBuf, e))

finish:
	return result;
}

int runSync(err_t *e, const char *format, ...)
{
	defineError();

	int result = 0;
	va_list vl;
	char buffer[500];

	va_start(vl, format);
	g_vsnprintf(buffer, sizeof(buffer), format, vl);
	va_end(vl);

	terror(result = doRunSyncWithOutput(buffer, NULL, NULL, e))

finish:
	return result;
}

int parseInteger(char *parseMe, gboolean isSigned, err_t *e)
{
	char *endptr = NULL;
	int result = 0;

	terror(failIfFalse(parseMe[0] != '\0'))
	result = strtol(parseMe, &endptr, 0);
	terror(failIfFalse(*endptr == '\0'))
	if (!isSigned) {
		terror(failIfFalse(result >= 0))
	}

finish:
	return result;
}

void closeOutput()
{
	if (outFile != NULL) {
		fclose(outFile);
	}
	if (errFile != NULL) {
		fclose(errFile);
	}
}

void setOutput(char *out, char *error, err_t *e)
{
	FILE *o = NULL;
	FILE *er = NULL;

	o = fopen(out, "a");
	terror(failIfFalse(o != NULL))

	er = fopen(error, "a");
	terror(failIfFalse(er != NULL))

	outFile = o; o = NULL;
	errFile = er; er = NULL;

finish:
	if (o != NULL) {
		fclose(o);
	}
	if (er != NULL) {
		fclose(er);
	}
	return;
}

void daemonize(err_t *e)
{
	pid_t pid = 0;
	int fdIn = -1;
	int fdOut = -1;
	int fdErr = -1;

	terror(failIfFalse((((pid = fork()) >= 0))))
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	umask(0);
	terror(failIfFalse((setsid() >= 0)))
	terror(failIfFalse((chdir("/") == 0)))

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	terror(failIfFalse((fdIn =
		open("/dev/null", O_RDONLY)) >= 0))
		terror(failIfFalse(dup2(fdIn, STDIN_FILENO) == STDIN_FILENO))

	terror(failIfFalse((fdOut =
			open("/dev/null", O_WRONLY)) >= 0))
	terror(failIfFalse(dup2(fdOut, STDOUT_FILENO) == STDOUT_FILENO))

	terror(failIfFalse((fdErr =
			open("/dev/null", O_WRONLY)) >= 0))
	terror(failIfFalse(dup2(fdErr, STDERR_FILENO) == STDERR_FILENO))

finish:
	if (fdIn >= 0) {
		close(fdIn);
	}
	if (fdOut >= 0) {
		close(fdOut);
	}
	if (fdErr >= 0) {
		close(fdErr);
	}
	return;
}
