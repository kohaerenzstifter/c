#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <stdio.h>

#include "../docs.h"

char *editAliases[] = { "edit", NULL };



static char *tagAliases[] = { "tag", NULL };
static char *untagAliases[] = { "untag", NULL };
static char *backAliases[] = { "back", NULL };
static char *dateAliases[] = { "date", NULL };
static char *undateAliases[] = { "undate", NULL };

static char *document = NULL;

static gboolean cmdTag(context_t *context, GList *args);
static GList *getCompletionsTag(context_t *context, const char *line);
static gboolean validateArgsTag(GList *args, context_t *context);

static gboolean cmdUntag(context_t *context, GList *args);
static GList *getCompletionsUntag(context_t *context, const char *line);
static gboolean validateArgsUntag(GList *args, context_t *context);

static gboolean cmdBack(context_t *context, GList *args);
static GList *getCompletionsBack(context_t *context, const char *line);
static gboolean validateArgsBack(GList *args, context_t *context);

static gboolean cmdDate(context_t *context, GList *args);
static GList *getCompletionsDate(context_t *context, const char *line);
static gboolean validateArgsDate(GList *args, context_t *context);

static gboolean cmdUndate(context_t *context, GList *args);
static GList *getCompletionsUndate(context_t *context, const char *line);
static gboolean validateArgsUndate(GList *args, context_t *context);

static command_t commands[] = {
		{
				&getCompletionsTag,
				&validateArgsTag,
				&cmdTag,
				tagAliases
		},
		{
				&getCompletionsUntag,
				&validateArgsUntag,
				&cmdUntag,
				untagAliases
		},
		{
				&getCompletionsBack,
				&validateArgsBack,
				&cmdBack,
				backAliases
		},
		{
				&getCompletionsDate,
				&validateArgsDate,
				&cmdDate,
				dateAliases
		},
		{
				&getCompletionsUndate,
				&validateArgsUndate,
				&cmdUndate,
				undateAliases
		},
		{
				NULL
		}
};

static gboolean cmdBack(context_t *context, GList *args)
{
	return TRUE;
}

static GList *getCompletionsBack(context_t *context, const char *line)
{

	GList *result = NULL;

	result = getCompletionsGeneric(context, line, &getNextTokenNoTokens);

	return result;
}

static gboolean validateArgsBack(GList *args, context_t *context)
{
	return args == NULL;
}

static gboolean cmdTag(context_t *context, GList *args)
{
	err_t error;
	err_t *e = &error;
	GList *cur = NULL;
	char *tag = NULL;
	gboolean doesHaveTag = FALSE;

	initErr(e);

	for (cur = args; cur != NULL; cur = g_list_next(cur)) {
		tag = cur->data;
		terror(doesHaveTag = hasTag(context, document, tag, e))
		if (doesHaveTag) {
			continue;
		}
		terror(doTag(context, document, tag, e))
	}
finish:
	return FALSE;
}

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

		terror(haveTags = getTags(context, document, e))
		for (cur = tokens; cur != NULL; cur = g_list_next(cur)) {
			if (g_list_find_custom(haveTags, cur->data,
					(GCompareFunc) &strcmp) != NULL) {
				result = FALSE;
				*options = NULL;
				goto finish;
			}
			haveTags = g_list_append(haveTags, strdup(cur->data));
		}

		terror(allTags = getAllTags(context, e))
		for (cur = haveTags; cur != NULL; cur = g_list_next(cur)) {
			link = g_list_find_custom(allTags, cur->data,
					(GCompareFunc) &strcmp);
			if (link == NULL) {
				//this is a new tag
				continue;
			}
			free(link->data);
			allTags = g_list_delete_link(allTags, link);
		}

		result = (tokens != NULL);
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

static gboolean validateArgsTag(GList *args, context_t *context)
{
	err_t error;
	err_t *e = &error;
	gboolean result = FALSE;
	GList *haveTags = NULL;
	GList *cur = NULL;

	initErr(e);

	terror(haveTags = getTags(context, document, e));
	for (cur = args; cur != NULL; cur = g_list_next(cur)) {
		if (g_list_find_custom(haveTags, cur->data,
				(GCompareFunc) &strcmp) != NULL) {
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

static gboolean cmdUntag(context_t *context, GList *args)
{
	err_t error;
	err_t *e = &error;
	GList *cur = NULL;
	char *tag = NULL;
	gboolean doesHaveTag = FALSE;

	initErr(e);

	for (cur = args; cur != NULL; cur = g_list_next(cur)) {
		tag = cur->data;
		terror(doesHaveTag = hasTag(context, document, tag, e))
		if (!doesHaveTag) {
			continue;
		}
		terror(doUntag(context, document, tag, e))
	}
finish:
	return FALSE;
}

static GList *getCompletionsUntag(context_t *context, const char *line)
{
	gboolean getNextToken(GList *tokens, GList **options,
			gboolean haveMore) {
		err_t error;
		err_t *e = &error;
		gboolean result = FALSE;
		GList *haveTags = NULL;
		GList *cur = NULL;
		GList *link = NULL;

		initErr(e);

		terror(haveTags = getTags(context, document, e))
		for (cur = tokens; cur != NULL; cur = g_list_next(cur)) {
			link = g_list_find_custom(haveTags, cur->data,
					(GCompareFunc) &strcmp);
			if (link == NULL) {
				result = FALSE;
				*options = NULL;
				goto finish;
			}
			free(link->data);
			haveTags = g_list_delete_link(haveTags, link);
		}
		*options = haveTags; haveTags = NULL;
		result = tokens != NULL;
finish:
		if (haveTags != NULL) {
			freeSimpleList(haveTags);
		}
		return result;
	}

	GList *result = NULL;

	result = getCompletionsGeneric(context, line, &getNextToken);

	return result;
}

static gboolean validateArgsUntag(GList *args, context_t *context)
{
	err_t error;
	err_t *e = &error;
	gboolean result = FALSE;
	GList *haveTags = NULL;
	GList *cur = NULL;

	initErr(e);

	terror(haveTags = getTags(context, document, e));
	for (cur = args; cur != NULL; cur = g_list_next(cur)) {
		if (g_list_find_custom(haveTags, cur->data,
				(GCompareFunc) &strcmp) == NULL) {
			goto finish;
		}
	}
	result = TRUE;

finish:
	if (haveTags != NULL) {
		freeSimpleList(haveTags);
	}
	return result;
}

gboolean validateArgsEdit(GList *args, context_t *context)
{
	err_t error;
	err_t *e = &error;
	gboolean result = FALSE;
	gboolean documentDoesExist = FALSE;

	initErr(e);

	if (args == NULL) {
		//edit what??
		goto finish;
	}
	if (g_list_length(args) > 1) {
		goto finish;
	}

	terror(documentDoesExist = documentExists(context, args->data, e))
	if (!documentDoesExist) {
		goto finish;
	}
	result = TRUE;

	finish:
	return result;
}

GList *getCompletionsEdit(context_t *context, const char *line)
{
	gboolean getNextToken(GList *tokens, GList **options,
			gboolean haveMore) {
		GList *docNames = NULL;
		gboolean result = FALSE;
		int length = 0;
		GList *cur = NULL;
		gboolean fine = FALSE;

		if (tokens == NULL) {
			result = FALSE;
			*options = getDocNames(context);
			goto finish;
		}
		length = g_list_length(tokens);
		if ((length == 1)&&(!haveMore)) {
			docNames = getDocNames(context);
			for (cur = g_list_first(docNames); cur != NULL;
					cur = g_list_next(cur)) {

				if (strcmp(tokens->data, cur->data) == 0) {
					fine = TRUE;
				}
			}
			if (fine) {
				result = TRUE;
				*options = NULL;
			} else {
				result = FALSE;
				*options = NULL;
			}
			goto finish;
		}

		result = FALSE;
		*options = NULL;

		finish:
		if (docNames != NULL) {
			freeSimpleList(docNames);
		}
		return result;
	}

	GList *result = NULL;

	result = getCompletionsGeneric(context, line, &getNextToken);

	return result;
}

void doEditDocument(context_t *context, char *docName)
{
	err_t error;
	err_t *e = &error;
	char *prompt = NULL;

	initErr(e);

	document = docName;
	prompt = g_strdup_printf("edit %s$ ", document);

	terror(commandLoop(context, prompt, commands, e))

finish:
	free(prompt);
}

gboolean cmdEdit(context_t *context, GList *args)
{
	gboolean result = FALSE;

	editDocument(context, args->data);

	return result;
}

static gboolean cmdDate(context_t *context, GList *args)
{
	err_t error;
	err_t *e = &error;
	GList *cur = NULL;
	gboolean result = FALSE;
	date_t *date = NULL;

	initErr(e);

	for (cur = args; cur != NULL; cur = g_list_next(cur)) {
		free(date); date = NULL;
		date = parseDate(cur->data, e);
		terror(doDate(context, document, date, e))
	}

finish:
	free(date);
	return result;
}

static GList *getCompletionsDate(context_t *context, const char *line)
{
	gboolean getNextToken(GList *tokens, GList **options,
			gboolean haveMore) {
		err_t error;
		err_t *e = &error;
		gboolean result = FALSE;
		GList *cur = NULL;
		date_t *date = NULL;
		gboolean maybeFine = FALSE;

		initErr(e);

		if (haveMore) {
			//not able to tell whether it makes sense
			goto finish;
		}
		for (cur = tokens; cur != NULL; cur = g_list_next(cur)) {
			maybeFine = TRUE;
			free(date); date = NULL;
			terror(date = parseDate(cur->data, e))
			if (hasDate(context, document, date, FALSE)) {
				goto finish;
			}
		}
		result = maybeFine;

finish:
		free(date);
		return result;
	}

	return getCompletionsGeneric(context, line, &getNextToken);
}
static gboolean validateArgsDate(GList *args, context_t *context)
{
	err_t error;
	err_t *e = &error;
	gboolean result = FALSE;
	GList *cur = NULL;
	date_t *date = NULL;
	gboolean maybeFine = FALSE;

	initErr(e);

	for (cur = args; cur != NULL; cur = g_list_next(cur)) {
		maybeFine = TRUE;
		free(date); date = NULL;
		terror(date = parseDate(cur->data, e))
		if (hasDate(context, document, date, FALSE)) {
			goto finish;
		}
	}
	result = maybeFine;

finish:
	free(date);
	return result;
}

static gboolean cmdUndate(context_t *context, GList *args)
{
	err_t error;
	err_t *e = &error;
	GList *cur = NULL;
	gboolean result = FALSE;
	date_t *date = NULL;

	initErr(e);

	for (cur = args; cur != NULL; cur = g_list_next(cur)) {
		free(date); date = NULL;
		date = parseDate(cur->data, e);
		terror(doUndate(context, document, date, e))
	}

finish:
	free(date);
	return result;
}

static char *date2String(date_t *date)
{
	char *result = NULL;

	if (date->type == YEAR) {
		result = g_strdup_printf("%d", date->year);
	} else 	if (date->type == MONTH) {
		result = g_strdup_printf("%d:%d", date->year, date->month);
	} else {
		result = g_strdup_printf("%d:%d:%d", date->year, date->month,
				date->day);
	}

	return result;
}

static GList *getCompletionsUndate(context_t *context, const char *line)
{
	gboolean getNextToken(GList *tokens, GList **options,
			gboolean haveMore) {

		gint compare(gconstpointer a, gconstpointer b) {
			const date_t *dateA = a;
			const date_t *dateB = b;
			gboolean equal = FALSE;

			if (dateA->type != dateB->type) {
				goto finish;
			}
			equal = (dateA->year == dateB->year);
			if ((!equal)||(dateA->type == YEAR)) {
				goto finish;
			}
			equal = (dateA->month == dateB->month);
			if ((!equal)||(dateA->type == MONTH)) {
				goto finish;
			}
			equal = (dateA->day == dateB->day);
finish:
			return equal ? 0 : 1;
		}

		err_t error;
		err_t *e = &error;
		gboolean result = FALSE;
		GList *cur = NULL;
		date_t *date = NULL;
		gboolean maybeFine = FALSE;
		GList *dates = NULL;
		GList *link = NULL;

		initErr(e);

		terror(dates = getAllDates(context, document, e))

		for (cur = tokens; cur != NULL; cur = g_list_next(cur)) {
			maybeFine = TRUE;
			free(date); date = NULL;
			terror(date = parseDate(cur->data, e))
			if (!hasDate(context, document, date, TRUE)) {
				goto finish;
			}
			link = g_list_find_custom(dates, date, &compare);
			if (link == NULL) {
				continue;
			}
			free(link->data);
			dates = g_list_delete_link(dates, link);
		}
		result = maybeFine;
		for (cur = dates; cur != NULL; cur = g_list_next(cur)) {
			*options = g_list_append(*options, date2String(cur->data));
		}

finish:
		if (dates != NULL) {
			for (cur = dates; cur != NULL; cur = g_list_next(cur)) {
				free(cur->data);
			}
			g_list_free(dates);
		}
		free(date);
		return result;
	}

	return getCompletionsGeneric(context, line, &getNextToken);
}

static gboolean validateArgsUndate(GList *args, context_t *context)
{
	err_t error;
	err_t *e = &error;
	gboolean result = FALSE;
	GList *cur = NULL;
	date_t *date = NULL;
	gboolean maybeFine = FALSE;

	initErr(e);

	for (cur = args; cur != NULL; cur = g_list_next(cur)) {
		maybeFine = TRUE;
		free(date); date = NULL;
		terror(date = parseDate(cur->data, e))
		if (!hasDate(context, document, date, TRUE)) {
			goto finish;
		}
	}
	result = maybeFine;

finish:
	free(date);
	return result;
}
