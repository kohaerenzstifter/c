/*
 ============================================================================
 Name        : wwwidget.c
 Author      : Martin Knappe (martin.knappe@gmail.com)
 Version     : 1.0
 Copyright   : Your copyright notice
 Description : Plugin for my "httpd" http(s) server
 
 Note        : The following commands must exist (and work!):
 
 xvfb-run, convert (imagemagick) => See code below
 
 
 Test with the following command:
 
 curl -k -u <USER>:<PASSWORD> -G -d "url=<URL>"
 -d "displayWidth=100" -d "displayHeight=100" 
 https://<SERVER>:8080/wwwidget/initial
 
 ============================================================================
 */

#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <glib.h>
#include <microhttpd.h>

#include <kohaerenzstiftung.h>
#include <httpd.h>

static char *mustPassword = NULL;

FUNCTION(getScreenshot, void, PRIVATE, (char *url, int displayWidth, int displayHeight, int colorDepth, char *tempDir, err_t *e),
	colorDepth = 8;
	int status = 0;
	char *stdoutBuf = NULL;
	char *stderrBuf = NULL;

	terror(status = runSyncWithOutput(e, &stdoutBuf, &stderrBuf,
			"xvfb-run --auto-servernum --server-args=\"-screen 0, 640x480x8\" "
			"cutycapt --url=%s --out=%s/entire.jpg",
			url, tempDir))
	if (status != 0) {
		fprintf(getErrFile(), "stdoutBuf: %s\n",
				stdoutBuf != NULL ? stdoutBuf : "<NULL>");
		fprintf(getErrFile(), "stderrBuf: %s\n",
				stderrBuf != NULL ? stderrBuf : "<NULL>");
	}

finish:
	free(stdoutBuf);
	free(stderrBuf);
	return;
)

FUNCTION(getFragment, char *, PRIVATE, (char *directory, uint32_t x, uint32_t width, uint32_t y, uint32_t height, err_t *e),
	int status = 0;
	static char result[PATH_MAX];

	terror(status = runSync(e, "convert %s/entire.jpg "
			"-crop %ux%u+%u+%u %s/%ux%u_%u_%u.jpg",
			directory, width, height,
			x, y, directory, width, height, x, y))
	terror(failIfFalse(status == 0))

	snprintf(result, sizeof(result), "%s/%ux%u_%u_%u.jpg",
			directory, width, height, x, y);

finish:
	return result;
)

FUNCTION(wwwidgetGet, void, PRIVATE, (char *url, int fromParent, int toParent, GList *requestHeaders, GList *requestParameters, gboolean *responseQueued, err_t *e),
	char *urlWanted = NULL;
	char tempDir[] = "/tmp/tempXXXXXX";
	gboolean haveTempDir = FALSE;
	char screenshotPath[PATH_MAX];
	char *displayWidthString = NULL;
	char *displayHeightString = NULL;
	int displayWidth = -1;
	int displayHeight = -1;
	char *resultFile = NULL;
	int colorDepth = 24;
	int width = -1;
	int height = -1;
	int x = -1;
	int y = -1;
	char *widthString = NULL;
	char *heightString = NULL;
	char *xString = NULL;
	char *yString = NULL;

	terror(failIfFalse((strcmp(url, "initial") == 0)
			||(strcmp(url, "periodic") == 0)))

	terror(failIfFalse((mkdtemp(tempDir)) != NULL))
	haveTempDir = TRUE;

	terror(urlWanted = getValue(requestHeaders, "url", e))

	terror(displayWidthString =
			getValue(requestHeaders, "displayWidth", e))
	terror(displayHeightString =
			getValue(requestHeaders, "displayHeight", e))

	terror(displayWidth = parseInteger(displayWidthString, FALSE, e))
	terror(displayHeight = parseInteger(displayHeightString, FALSE, e))

	if (strcmp(url, "initial") == 0) {
		colorDepth = 8;
	}

	terror(getScreenshot(urlWanted, displayWidth, displayHeight,
			colorDepth, tempDir, e))

	snprintf(screenshotPath, sizeof(screenshotPath),
			"%s/entire.jpg", tempDir);

	if (strcmp(url, "initial") == 0) {
		resultFile = screenshotPath;
	} else {
		terror(xString =
				getValue(requestHeaders, "x", e))
		terror(yString =
				getValue(requestHeaders, "y", e))
		terror(widthString =
				getValue(requestHeaders, "width", e))
		terror(heightString =
				getValue(requestHeaders, "height", e))

		terror(x = parseInteger(xString, FALSE, e))
		terror(y = parseInteger(yString, FALSE, e))
		terror(width = parseInteger(widthString, FALSE, e))
		terror(height = parseInteger(heightString, FALSE, e))

		terror(resultFile = getFragment(tempDir, x, width,
				y, height, e))
	}

	terror(respondFromFile(toParent, MHD_HTTP_OK, resultFile,
			responseQueued, e))

finish:
	if (haveTempDir) {
		runSync(NULL, "rm -rf %s", tempDir);
	}
	return;
)

FUNCTION(wwwidgetAuthenticate, void, PRIVATE, (struct MHD_Connection *connection, err_t *e),
	char *username = NULL;
	char *password = NULL;
	gboolean ok = FALSE;

	username = MHD_basic_auth_get_username_password(connection, &password);

	terror(failIfFalse(username != NULL))
	terror(failIfFalse(password != NULL))

	ok = (strcmp(username, "wwwidget") == 0);
	terror(failIfFalse(ok))
	ok = (strcmp(password, mustPassword) == 0);
	terror(failIfFalse(ok))

finish:
	free(username);
	free(password);
	return;
)

FUNCTION(teardown, void, PRIVATE, (void),
	free(mustPassword);
)

FUNCTION(setup, void, PRIVATE, (GList *keyValuePairs, err_t *e),
	char *password = NULL;

	terror(password = getValue(keyValuePairs, "password", e))

	mustPassword = strdup(password);

finish:
	return;
)

FUNCTION(startRegistry, void, PUBLIC, (err_t *e),
	terror(registerPlugin("wwwidget", setup, teardown, NULL, NULL,
			&wwwidgetGet, NULL, &wwwidgetAuthenticate, e))
finish:
	return;
)
