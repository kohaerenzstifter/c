/*static void getScreenshot(char *url, int displayWidth,
		int displayHeight, int colorDepth, char *tempDir, err_t *e)
{
	mark();
	colorDepth = 8;
	int status = 0;
	char *stdoutBuf = NULL;
	char *stderrBuf = NULL;

	terror(status = runSyncWithOutput(e, &stdoutBuf, &stderrBuf,
			"xvfb-run --server-args=\"-screen 0, %dx%dx%d\" "
			"cutycapt --url=%s --out=%s/entire.jpg",
			displayWidth, displayHeight, colorDepth, url, tempDir))
	if (status != 0) {
		FILE *file = (errFile != NULL) ? errFile : stderr;
		fprintf(file, "stdoutBuf: %s\n",
				stdoutBuf != NULL ? stdoutBuf : "<NULL>");
		fprintf(file, "stderrBuf: %s\n",
				stderrBuf != NULL ? stderrBuf : "<NULL>");
	}
	terror(failIfFalse(status == 0))

finish:
	free(stdoutBuf);
	free(stderrBuf);
	return;
}*/

/*static void identify(char *tempDir, int *width, int *height, err_t *e)
{
	mark();

	int status = 0;
	char *stdoutBuf = NULL;
	char *stderrBuf = NULL;
	gchar **tokens = NULL;
	gchar **tokens2 = NULL;
	gchar **tokens3 = NULL;

	terror(status = runSyncWithOutput(e, &stdoutBuf, &stderrBuf,
			"identify -verbose %s/entire.jpg | grep Geometry", tempDir))
	terror(failIfFalse(status == 0))
	terror(failIfFalse(stderrBuf[0] == '\0'))

	tokens = g_strsplit(stdoutBuf, ":", 2);
	terror(failIfFalse(tokens[0] != NULL))
	terror(failIfFalse(tokens[1] != NULL))

	tokens2 = g_strsplit(tokens[1], "+", 2);
	terror(failIfFalse(tokens2[0] != NULL))
	terror(failIfFalse(tokens2[1] != NULL))

	tokens3 = g_strsplit(tokens2[0], "x", 2);
	terror(failIfFalse(tokens3[0] != NULL))
	terror(failIfFalse(tokens3[1] != NULL))

	terror(*width = parseInteger(tokens3[0], FALSE, e))
	terror(*height = parseInteger(tokens3[1], FALSE, e))

finish:
	if (tokens != NULL) {
		g_strfreev(tokens);
	}
	if (tokens2 != NULL) {
		g_strfreev(tokens2);
	}
	if (tokens3 != NULL) {
		g_strfreev(tokens3);
	}
	free(stdoutBuf);
	free(stderrBuf);
	return;
}*/

/*static char *getFragment(char *directory, uint32_t x,
		uint32_t width, uint32_t y, uint32_t height, err_t *e)
{
	mark();

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
}*/

/*static void fragment(char *tempDir, int width, int height, err_t *e)
{
	mark();

#define STANDARD_WIDTH 512
#define STANDARD_HEIGHT 512

	int status = 0;
	gboolean haveTransparent = FALSE;
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t widthNow = 0;
	uint32_t heightNow = 0;

	terror(status = runSync(e,
			"convert -size 512x512 xc:none %s/transparent.jpg", tempDir))
	terror(failIfFalse(status == 0))
	haveTransparent = TRUE;

	for (x = 0; x < width; x += STANDARD_WIDTH) {
		for (y = 0; y < height; y += STANDARD_HEIGHT) {
			widthNow = width - x;
			widthNow = widthNow > STANDARD_WIDTH ? STANDARD_WIDTH : widthNow;
			heightNow = height - y;
			heightNow = heightNow > STANDARD_HEIGHT ? STANDARD_HEIGHT : heightNow;
			terror(getFragment(tempDir, x, widthNow, y, heightNow, e))
			if ((widthNow < STANDARD_WIDTH)||(heightNow < STANDARD_HEIGHT)) {
				terror(status = runSync(e, "composite -geometry +0+0 "
						"%s/%ux%u_%u_%u.jpg %s/transparent.jpg %s/composite.jpg",
						tempDir, widthNow, heightNow, x, y, tempDir, tempDir))
				terror(failIfFalse(status == 0))
				terror(status = runSync(e, "rm -rf %s/%ux%u_%u_%u.jpg",
						tempDir, widthNow, heightNow, x, y))
				terror(failIfFalse(status == 0))
				terror(status =
						runSync(e, "mv %s/composite.jpg %s/%ux%u_%u_%u.jpg",
						tempDir, tempDir, widthNow, heightNow, x, y))
				terror(failIfFalse(status == 0))
			}
		}
	}

finish:
	if (haveTransparent) {
		runSync(NULL, "rm -rf %s/transparent.jpg", tempDir);
	}
}*/

/*static void createInfoTxt(char *tempDir, char *url,
		int width, int height, err_t *e)
{
	mark();

	char path[PATH_MAX];
	FILE *file = NULL;

	snprintf(path, sizeof(path), "%s/info.txt", tempDir);
	file = fopen(path, "w");
	fprintf(file, "%s\n", url);
	fprintf(file, "%d\n", width);
	fprintf(file, "%d\n", height);

	if (file != NULL) {
		fclose(file);
	}
}*/

/*static void zip(char *tempDir, char *zipDir, err_t *e)
{
	mark();

	int status = 0;

	terror(status = runSync(e, "rm -rf %s/entire.jpg", tempDir))
	terror(failIfFalse(status == 0))

	terror(status = runSync(e, "rm -rf %s/composite.jpg", tempDir))
	terror(failIfFalse(status == 0))

	terror(status = runSync(e, "rm -rf %s/transparent.jpg", tempDir))
	terror(failIfFalse(status == 0))
*/
//	terror(status = runSync(e, "zip -j %s/initial.zip %s/*",
/*			zipDir, tempDir))
	terror(failIfFalse(status == 0))

finish:
	return;
}*/

/*static void wwwidgetAuthenticate(struct MHD_Connection *connection, err_t *e)
{
	char *username = NULL;
	char *password = NULL;
	gboolean ok = FALSE;

	username = MHD_basic_auth_get_username_password(connection, &password);

	terror(failIfFalse(username != NULL))
	terror(failIfFalse(password != NULL))

	ok = (strcmp(username, "wwwidget") == 0);
	terror(failIfFalse(ok))
	ok = (strcmp(password, "ElJarabeTapatio") == 0);
	terror(failIfFalse(ok))

finish:
	free(username);
	free(password);
	return;
}

static void wwwidgetGet(char *url, int fromParent, int toParent,
		GList *requestHeaders, GList *requestParameters, gboolean *responseQueued,
		err_t *e)
{
	mark();

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

	terror(urlWanted = getValue(requestParameters, "url", e))

	terror(displayWidthString =
			getValue(requestParameters, "displayWidth", e))
	terror(displayHeightString =
			getValue(requestParameters, "displayHeight", e))

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
				getValue(requestParameters, "x", e))
		terror(yString =
				getValue(requestParameters, "y", e))
		terror(widthString =
				getValue(requestParameters, "width", e))
		terror(heightString =
				getValue(requestParameters, "height", e))

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
}*/

/*static void testGet(char *url, int fromParent, int toParent,
		GList *requestHeaders, GList *requestParameters, gboolean *responseQueued,
		err_t *e)
{
	mark();

	char page[1000];
	snprintf(page, sizeof(page), "<html><body>Hello %s!<"
			"/body></html>",
			url);
	terror(respondFromBuffer(toParent, MHD_HTTP_OK, page,
			strlen(page) + 1, responseQueued, e))
finish:
	return;
}*/
