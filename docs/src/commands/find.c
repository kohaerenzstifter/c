#include <stdlib.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "../docs.h"

char *findAliases[] = { "find", NULL };

static char *tagAliases[] = { "tag", NULL };

static gboolean cmdTag(context_t *context, GList *args);
static GList *getCompletionsTag(context_t *context, const char *line);
static gboolean validateArgsTag(GList *args, context_t *context);

static command_t commands[] = {
		{
				&getCompletionsTag,
				&validateArgsTag,
				&cmdTag,
				tagAliases
		},
		{
				NULL
		}
};

static GList *tags = NULL;
static date_t *date = NULL;

static GList *getCompletionsTag(context_t *context, const char *line)
{
	gboolean getNextToken(GList *tokens, GList **options,
			gboolean haveMore) {
		err_t error;
		err_t *e = &error;
		GList *haveTags = NULL;
		GList *cur = NULL;
		GList *allTags = NULL;
		gboolean result = FALSE;
		GList *link = NULL;

		initErr(e);

		for (cur = tokens; cur != NULL; cur = g_list_next(cur)) {
			if (g_list_find_custom(haveTags, cur->data,
					(GCompareFunc) &strcmp) != NULL) {
				result = FALSE;
				*options = NULL;
				goto finish;
			}
			haveTags = g_list_append(haveTags, strdup(cur->data));
		}

		terror(allTags = getTagSuggestions(context, haveTags, date, e))
		for (cur = haveTags; cur != NULL; cur = g_list_next(cur)) {
			link = g_list_find_custom(allTags, cur->data,
					(GCompareFunc) &strcmp);
			if (link == NULL) {
				//tag doesn't exist!
				result = FALSE;
				*options = NULL;
				goto finish;
			}
			free(link->data);
			allTags = g_list_delete_link(allTags, link);
		}

		result = ((tokens != NULL)&&(!haveMore));
		*options = allTags; allTags = NULL;

finish:
		if (allTags != NULL) {
			freeSimpleList(allTags);
		}
		if (haveTags != NULL) {
			freeSimpleList(haveTags);
		}
		return result;
	}

	GList *result = NULL;

	result = getCompletionsGeneric(context, line, &getNextToken);

	return result;
}

static gboolean cmdTag(context_t *context, GList *args)
{
	err_t error;
	err_t *e = &error;
	GList *cur = NULL;

	initErr(e);

	for (cur = args; cur != NULL; cur = g_list_next(cur)) {
		tags = g_list_append(tags, strdup(cur->data));
	}

	return TRUE;
}

static gboolean validateArgsTag(GList *args, context_t *context)
{
	err_t error;
	err_t *e = &error;
	gboolean result = FALSE;
	GList *haveTags = NULL;
	GList *cur = NULL;

	initErr(e);

	for (cur = args; cur != NULL; cur = g_list_next(cur)) {
		if (g_list_find_custom(haveTags, cur->data,
				(GCompareFunc) &strcmp) != NULL) {
			goto finish;
		}
		if (!tagExists(context, cur->data)) {
			goto finish;
		}
		haveTags = g_list_append(haveTags, strdup(cur->data));
	}
	result = TRUE;

finish:
	if (haveTags != NULL) {
		freeSimpleList(haveTags);
	}
	return result;
}

gboolean validateArgsFind(GList *args, context_t *context)
{
	return args == NULL;
}

GList *getCompletionsFind(context_t *context, const char *line)
{
	GList *result = NULL;

	result = getCompletionsGeneric(context, line, &getNextTokenNoTokens);

	return result;
}

gboolean cmdFind(context_t *context, GList *args)
{

	gboolean ignoreTagFunc(context_t *context, char *tag, err_t *e) {
		gboolean result = FALSE;
		return result;
	}

	void tagYesFunc(context_t *context, char *tag, err_t *e) {
		tags = g_list_append(tags, strdup(tag));
		return;
	}

	err_t error;
	err_t *e = &error;
	char *input = NULL;
	GList *docs = NULL;
	GList *cur = NULL;
	char *docName = NULL;
	char *baseDir = NULL;
	char *path = NULL;
	char *prompt = NULL;
	gboolean doAccept = FALSE;

	initErr(e);

	baseDir = getBaseDir(context);

	terror(input = getPlainInput(context, "enter date$ ", e))
	if (input[0] != '\0') {
		terror(date = parseDate(input, e))
	}

	//terror(tagLoop(context, ignoreTagFunc, tagYesFunc, e))

	terror(commandLoop(context, "find $ ", commands, e))
	terror(docs = findDocs(context, tags, date, e))

	for (cur = docs; cur != NULL; cur = g_list_next(cur)) {
		docName = cur->data;
		free(path); path = NULL;
		path = g_strdup_printf("%s/result/%s", baseDir, docName);
		if (g_file_test(path, G_FILE_TEST_EXISTS)) {
			continue;
		}

		terror(view(context, docName, e))

		free(path); path = NULL;
		prompt = g_strdup_printf("Add %s to result set [y/N]?", docName);

		doAccept = FALSE;
		for(;;) {
			free(input); input = NULL;
			terror(input = getPlainInput(context, prompt, e))
			if (strcmp(input, "") == 0) {
				break;
			}
			if (strcasecmp(input, "y") == 0) {
				doAccept = TRUE;
				break;
			}
		}

		if (!doAccept) {
			continue;
		}

		terror(runSync(e, "ln -s ../unencrypted/%s %s/result/%s",
				docName, baseDir, docName))
	}

finish:
	free(prompt);
	free(path);
	free((date)); date = NULL;
	free(input);
	freeSimpleList(tags); tags = NULL;
	freeSimpleList(docs);
	return FALSE;
}
