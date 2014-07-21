#include <glib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

#include "../docs.h"

gboolean cmdSubmit(context_t *context, GList *args);
char *submitAliases[] = { "submit", NULL };
char *submitting = NULL;

gboolean validateArgsSubmit(GList *args, context_t *context)
{
	gboolean result = FALSE;
	int length = 0;
	char *all = NULL;
	char *encryptedUnencrypted = NULL;
	char *one = NULL;
	char *by = NULL;

	if (args == NULL) {
		goto finish;
	}

	length = g_list_length(args);

	if (length < 2) {
		goto finish;
	}
	if (length == 2) {
		all = args->data;
		if (strcmp(all, "all") != 0) {
			goto finish;
		}
		args = g_list_next(args);
		encryptedUnencrypted = args->data;
		if ((strcmp(encryptedUnencrypted, "encrypted") != 0)&&
				(strcmp(encryptedUnencrypted, "unencrypted") != 0)) {
			goto finish;
		}
		result = TRUE;
		goto finish;
	}
	if (length > 3) {
		goto finish;
	}
	one = args->data;
	if (strcmp(one, "one") != 0) {
		goto finish;
	}
	args = g_list_next(args); by = args->data;
	if (strcmp(by, "by") != 0) {
		goto finish;
	}
	args = g_list_next(args); one = args->data;
	if (strcmp(one, "one") != 0) {
		goto finish;
	}
	result = TRUE;

	finish:
	return result;
}

GList *getCompletionsSubmit(context_t *context, const char *line)
{
	gboolean getNextToken(GList *tokens, GList **options,
			gboolean haveMore) {

		GList *getCommandToken() {
			char *commands[] = { "one", "all", NULL };
			GList *result = NULL;
			int i = -1;

			for (i = 0; commands[i] != NULL; i++) {
				result = g_list_append(result, strdup(commands[i]));
			}

			return result;
		}

		GList *getNextSpecificToken(GList *result, char *must) {
			result = g_list_append(result, strdup(must));
			return result;
		}

		gboolean getNextTokenOne(GList *tokens, GList **options) {
			gboolean result = FALSE;
			int length = 0;
			char *have = NULL;

			if (tokens != NULL) {
				length = g_list_length(tokens);
			}

			if (length < 1) {
				result = FALSE;
				*options = getNextSpecificToken(*options, "by");
				goto finish;
			}
			have = tokens->data;
			if (strcmp(have, "by") != 0) {
				result = FALSE;
				*options = NULL;
				goto finish;
			}
			tokens = g_list_next(tokens);
			if (length < 2) {
				result = FALSE;
				*options = getNextSpecificToken(*options, "one");
				goto finish;
			}
			have = tokens->data;
			if (strcmp(have, "one") != 0) {
				result = FALSE;
				*options = NULL;
				goto finish;
			}
			haveMore = haveMore || (g_list_next(tokens) != NULL);
			if (haveMore) {
				result = FALSE;
				*options = NULL;
				goto finish;
			}
			result = TRUE;
			*options = NULL;

			finish:
			return result;
		}

		gboolean getNextTokenAll(GList *tokens, GList **options) {
			gboolean result = FALSE;
			int length = 0;
			char *have = NULL;

			if (tokens != NULL) {
				length = g_list_length(tokens);
			}

			if (length < 1) {
				result = FALSE;
				*options = getNextSpecificToken(*options, "encrypted");
				*options = getNextSpecificToken(*options, "unencrypted");
				goto finish;
			}
			have = tokens->data;
			if ((strcmp(have, "encrypted") != 0)&&(strcmp(have,
					"unencrypted") != 0)) {
				result = FALSE;
				*options = NULL;
				goto finish;
			}
			haveMore = haveMore || (g_list_next(tokens) != NULL);
			if (haveMore) {
				result = FALSE;
				*options = NULL;
				goto finish;
			}
			result = TRUE;
			*options = NULL;

			finish:
			return result;
		}

		gboolean result = FALSE;
		int length = 0;
		char *commandToken = NULL;

		if (tokens != NULL) {
			length = g_list_length(tokens);
		}
		if (length < 1) {
			result = FALSE;
			*options = getCommandToken();
			goto finish;
		}
		commandToken = tokens->data;
		if (strcmp(commandToken, "one") == 0) {
			result = getNextTokenOne(g_list_next(tokens), options);
			goto finish;
		}
		if (strcmp(commandToken, "all") == 0) {
			result = getNextTokenAll(g_list_next(tokens), options);
			goto finish;
		}
		result = FALSE;
		*options = NULL;

		finish:
		return result;
	}

	GList *result = NULL;

	result = getCompletionsGeneric(context, line, &getNextToken);

	return result;
}

static void doLink(context_t *context, char *document, err_t *e)
{
	char *baseDir = NULL;

	baseDir = getBaseDir(context);

	terror(runSync(e, "ln -s ../encrypted/%s %s/unencrypted/%s",
			document, baseDir, document))
finish:
	return;
}

static void doSubmit(context_t *context, char *sourceName,
		gboolean encrypted, char *targetName, int highest, err_t *e)
{
	char *baseDir = NULL;
	char *targetDir = NULL;
	char *sourceDir = NULL;

	baseDir = getBaseDir(context);
	targetDir = encrypted ? g_strdup_printf("%s/encrypted",
			baseDir) : g_strdup_printf("%s/unencrypted",
			baseDir);
	sourceDir = g_strdup_printf("%s/submit", baseDir);

	terror(runSync(e, "mv %s/%s %s/%s",
			sourceDir, sourceName,
			targetDir, targetName))
	setHighest(context, highest);
	addDoc(context, targetName, encrypted);
	if (encrypted) {
		terror(doLink(context, targetName, e))
	}
finish:
	free(sourceDir);
	free(targetDir);
	return;
}

static void cmdSubmitAllTargetDir(context_t *context, gboolean encrypted)
{
	err_t error;
	err_t *e = &error;
	DIR *dir = NULL;
	struct dirent *dirent = NULL;
	char *nextName = NULL;
	int highest = -1;
	char *extension = NULL;
	char *submitDirectory = NULL;
	char *baseDir = NULL;

	initErr(e);

	baseDir = getBaseDir(context);
	submitDirectory = g_strdup_printf("%s/submit", baseDir);

	terror(highest = getHighest(context, e))
	dir = opendir(submitDirectory);
	terror(failIfFalse(dir != NULL))
	while ((dirent = readdir(dir)) != NULL) {
		free(nextName); nextName = NULL;
		if (dirent->d_name[0] == '.') {
			//won't show hidden files either...
			continue;
		}
		if (strchr(dirent->d_name, ' ') != NULL) {
			//won't handle these...
			continue;
		}
		extension = strchr(dirent->d_name, '.');
		highest++;
		nextName = extension != NULL ?
				g_strdup_printf("%d%s", highest, extension) :
				g_strdup_printf("%d", highest);
		terror(doSubmit(context, dirent->d_name, encrypted,
				nextName, highest, e));
	}
finish:
	free(submitDirectory);
	free(nextName);
	if (dir != NULL) {
		closedir(dir);
	}
	return;
}

static void cmdSubmitAllUnencrypted(context_t *context)
{
	cmdSubmitAllTargetDir(context, FALSE);
}

static void cmdSubmitAllEncrypted(context_t *context, GList *args)
{
	cmdSubmitAllTargetDir(context, TRUE);
}

static void cmdSubmitAll(context_t *context, GList *args)
{
	char *encryptedUnencrypted = NULL;
	encryptedUnencrypted = args->data;
	if (strcmp(encryptedUnencrypted, "encrypted") == 0) {
		cmdSubmitAllEncrypted(context, g_list_next(args));
	} else {
		cmdSubmitAllUnencrypted(context);
	}
}

static GList *getCompletionsSubmitView(context_t *context,
		const char *line)
{

	GList *result = NULL;

	result = getCompletionsGeneric(context, line, &getNextTokenNoTokens);

	return result;
}

static gboolean validateArgsSubmitView(GList *args, context_t *context)
{
	return args == NULL;
}

static gboolean cmdSubmitView(context_t *context, GList *args)
{
	err_t error;
	err_t *e = &error;

	initErr(e);

	terror(viewUnsubmitted(context, submitting, e))

finish:
	return FALSE;
}

static char *submitViewAliases[] = { "view", NULL };

static GList *getCompletionsSubmitEncrypted(context_t *context,
		const char *line)
{

	GList *result = NULL;

	result = getCompletionsGeneric(context, line, &getNextTokenNoTokens);

	return result;
}

static gboolean validateArgsSubmitEncrypted(GList *args,
		context_t *context)
{
	return args == NULL;
}

static gboolean cmdSubmitEncrypted(context_t *context, GList *args)
{
	err_t error;
	err_t *e = &error;
	char *period = NULL;
	char *nextName = NULL;
	int highest = -1;

	initErr(e);

	terror(highest = getHighest(context, e))
	highest++;
	period = strchr(submitting, '.');
	nextName = (period != NULL) ?
			g_strdup_printf("%d%s", highest, period) :
			g_strdup_printf("%d", highest);

	terror(doSubmit(context, submitting, TRUE, nextName, highest, e));

finish:
	free(nextName);
	return TRUE;
}

static char *submitEncryptedAliases[] = { "encrypted", NULL };

static GList *getCompletionsSubmitUnencrypted(context_t *context,
		const char *line)
{

	GList *result = NULL;

	result = getCompletionsGeneric(context, line, &getNextTokenNoTokens);

	return result;
}

static gboolean validateArgsSubmitUnencrypted(GList *args,
		context_t *context)
{
	return args == NULL;
}

static gboolean cmdSubmitUnencrypted(context_t *context, GList *args)
{
	err_t error;
	err_t *e = &error;
	char *period = NULL;
	char *nextName = NULL;
	int highest = -1;

	initErr(e);

	terror(highest = getHighest(context, e))
	highest++;
	period = strchr(submitting, '.');
	nextName = (period != NULL) ?
			g_strdup_printf("%d%s", highest, period) :
			g_strdup_printf("%d", highest);

	terror(doSubmit(context, submitting, FALSE,
			nextName, highest, e));

finish:
	free(nextName);
	return TRUE;
}

static char *submitUnencryptedAliases[] = { "unencrypted", NULL };

static void submitSingle(context_t *context, char *submitMe)
{
	command_t commands[] = {
			{
					&getCompletionsSubmitView,
					&validateArgsSubmitView,
					&cmdSubmitView,
					submitViewAliases
			},
			{
					&getCompletionsSubmitEncrypted,
					&validateArgsSubmitEncrypted,
					&cmdSubmitEncrypted,
					submitEncryptedAliases
			},
			{
					&getCompletionsSubmitUnencrypted,
					&validateArgsSubmitUnencrypted,
					&cmdSubmitUnencrypted,
					submitUnencryptedAliases
			},
			{
					NULL
			}
	};
	err_t error;
	err_t *e = &error;
	char *prompt = NULL;

	initErr(e);

	submitting = submitMe;

	prompt = g_strdup_printf("submit %s$ ", submitMe);
	terror(commandLoop(context, prompt, commands, e))

finish:
	free(prompt);
	return;
}

static void cmdSubmitOneByOne(context_t *context)
{
	err_t error;
	err_t *e = &error;
	DIR *dir = NULL;
	char *baseDir = NULL;
	char *submitDir = NULL;
	struct dirent *dirent = NULL;
	GList *list = NULL;
	GList *cur = NULL;

	initErr(e);

	baseDir = getBaseDir(context);
	submitDir = g_strdup_printf("%s/submit", baseDir);

	dir = opendir(submitDir);
	terror(failIfFalse(dir != NULL))
	while ((dirent = readdir(dir)) != NULL) {
		if (dirent->d_name[0] == '.') {
			//won't show hidden files either...
			continue;
		}
		if (strchr(dirent->d_name, ' ') != NULL) {
			//won't handle these...
			continue;
		}
		list = g_list_append(list, strdup(dirent->d_name));
	}
	for (cur = list; cur != NULL; cur = g_list_next(cur)) {
		submitSingle(context, cur->data);
	}

finish:
	free(submitDir);
	if (dir != NULL) {
		closedir(dir);
	}
	if (list != NULL) {
		freeSimpleList(list);
	}
	return;
}

gboolean cmdSubmit(context_t *context, GList *args)
{
	char *allOne = NULL;

	allOne = args->data;
	if (strcmp(allOne, "all") == 0) {
		cmdSubmitAll(context, g_list_next(args));
	} else {
		cmdSubmitOneByOne(context);
	}
	return FALSE;
}
