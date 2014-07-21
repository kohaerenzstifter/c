#include <stdlib.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "../docs.h"

char *processAliases[] = { "process", NULL };

static gboolean doProcessMore = FALSE;
static char *document = NULL;



static gboolean cmdProcessMoreYes(context_t *context, GList *args);
static GList *getCompletionsProcessMoreYes(context_t *context, const char *line);
static gboolean validateArgsProcessMoreYes(GList *args, context_t *context);
static char *processMoreYesAliases[] = { "yes", NULL };

static gboolean cmdProcessMoreNo(context_t *context, GList *args);
static GList *getCompletionsProcessMoreNo(context_t *context, const char *line);
static gboolean validateArgsProcessMoreNo(GList *args, context_t *context);
static char *processMoreNoAliases[] = { "no", NULL };

static command_t processMoreCommands[] = {
		{
				&getCompletionsProcessMoreYes,
				&validateArgsProcessMoreYes,
				&cmdProcessMoreYes,
				processMoreYesAliases
		},
		{
				&getCompletionsProcessMoreNo,
				&validateArgsProcessMoreNo,
				&cmdProcessMoreNo,
				processMoreNoAliases
		},
		{
				NULL
		}
};

static gboolean cmdProcessMoreYes(context_t *context, GList *args)
{
	doProcessMore = TRUE;
	return TRUE;
}

static GList *getCompletionsProcessMoreYes(context_t *context, const char *line)
{
	GList *result = NULL;

	result = getCompletionsGeneric(context, line, &getNextTokenNoTokens);

	return result;
}

static gboolean validateArgsProcessMoreYes(GList *args, context_t *context)
{
	return args == NULL;
}

static gboolean cmdProcessMoreNo(context_t *context, GList *args)
{
	doProcessMore = FALSE;
	return TRUE;
}

static GList *getCompletionsProcessMoreNo(context_t *context, const char *line)
{
	GList *result = NULL;

	result = getCompletionsGeneric(context, line, &getNextTokenNoTokens);

	return result;
}

static gboolean validateArgsProcessMoreNo(GList *args, context_t *context)
{
	return args == NULL;
}



gboolean validateArgsProcess(GList *args, context_t *context)
{
	gboolean result = FALSE;
	char *endptr = NULL;

	result = args == NULL;
	if (result) {
		goto finish;
	}
	if (*((char *) args->data) == '\0') {
		goto finish;
	}
	if (strtoul(args->data, &endptr, 0) == 0) {
		result = FALSE;
		goto finish;
	}
	if (*endptr != '\0') {
		goto finish;
	}
	result = g_list_next(args) == NULL;

finish:
	return result;
}

GList *getCompletionsProcess(context_t *context, const char *line)
{
	gboolean getNextToken(GList *tokens, GList **options,
			gboolean haveMore) {
		gboolean result = FALSE;
		char *endptr = NULL;

		if (tokens == NULL) {
			result = !haveMore;
			goto finish;
		}
		if (*((char *) tokens->data) == '\0') {
			result = FALSE;
			goto finish;
		}
		if (strtoul(tokens->data, &endptr, 0) == 0) {
			result = FALSE;
			goto finish;
		}
		if (*endptr != '\0') {
			result = FALSE;
			goto finish;
		}
		if (g_list_next(tokens) != NULL) {
			result = FALSE;
			goto finish;
		}
		result = !haveMore;

finish:
		return result;
	}
	GList *result = NULL;

	result = getCompletionsGeneric(context, line, &getNextToken);

	return result;
}

static GList *getIntersection2(GList *one, GList *other, err_t *e)
{
	GList *result = NULL;
	GList *intersection = NULL;
	GList *cur = NULL;

	for (cur = one; cur != NULL; cur = g_list_next(cur)) {
		if (g_list_find_custom(other, cur->data,
				(GCompareFunc) &strcmp) == NULL) {
			continue;
		}
		intersection = g_list_append(intersection, strdup(cur->data));
	}

	result = intersection; intersection = NULL;

	freeSimpleList(intersection);

	return result;
}

static GList *getIntersection(GList *one, GList *other, err_t *e)
{
	GList *result = NULL;
	GList *intersection = NULL;

	terror(intersection = getIntersection2(one, other, e))
	terror(result = getIntersection2(intersection, one, e))

finish:
	freeSimpleList(intersection);
	return result;
}

static GList *getPotentialTags3(context_t *context, char *docName,
		GList *documents, GList *haveAlready, GList *areEligible, err_t *e)
{
	GList *cur = NULL;
	char *document;
	GList *tags = NULL;
	GList *result = NULL;
	GList *eligible = NULL;
	GList *oldEligible = NULL;
	gboolean doAppend = FALSE;

	if (documents == NULL) {
		goto finish;
	}

	eligible = cloneStringList(areEligible);

	for (cur = documents; cur != NULL; cur = g_list_next(cur)) {
		document = cur->data;
		if (strcmp(document, docName) == 0) {
			continue;
		}
		doAppend = TRUE;
		freeSimpleList(tags); tags = NULL;
		terror(tags = getTags(context, document, e))
		oldEligible = eligible;

		terror(eligible = getIntersection(tags, eligible, e))

		if (oldEligible != NULL) {
			freeSimpleList(oldEligible); oldEligible = NULL;
		}
	}

	if (!doAppend) {
		goto finish;
	}

	for (cur = eligible; cur != NULL; cur = g_list_next(cur)) {
		if (g_list_find_custom(haveAlready, cur->data,
				(GCompareFunc) &strcmp) != NULL) {
			continue;
		}
		result = g_list_append(result, strdup(cur->data));
	}

finish:
	freeSimpleList(eligible); eligible = NULL;
	freeSimpleList(oldEligible); oldEligible = NULL;
	freeSimpleList(tags);
	return result;
}

static GList *combinePotentials(GList *one, GList *other)
{
	GList *result = NULL;
	GList *cur = NULL;

	for (cur = one; cur != NULL; cur = g_list_next(cur)) {
		if (g_list_find_custom(result, cur->data,
				(GCompareFunc) &strcmp) != NULL) {
			continue;
		}
		result = g_list_append(result, strdup(cur->data));
	}

	for (cur = other; cur != NULL; cur = g_list_next(cur)) {
		if (g_list_find_custom(result, cur->data,
				(GCompareFunc) &strcmp) != NULL) {
			continue;
		}
		result = g_list_append(result, strdup(cur->data));
	}

	return result;
}

static GList *getPotentialTags2(context_t *context,
		char *document, GList *haveAlready,
		GList *areEligible, err_t *e)
{
	GList *cur = NULL;
	GList *documents = NULL;
	GList *result = NULL;
	GList *combinedPotentials = NULL;
	GList *oldCombinedPotentials = NULL;
	GList *potentials = NULL;

	for (cur = haveAlready; cur != NULL; cur = g_list_next(cur)) {
		freeSimpleList(documents); documents = NULL;
		terror(documents = getDocuments(context, cur->data, e))

		freeSimpleList(potentials); potentials = NULL;
		terror(potentials = getPotentialTags3(context, document, documents,
				haveAlready, areEligible, e))

		oldCombinedPotentials = combinedPotentials;
		combinedPotentials = combinePotentials(combinedPotentials, potentials);
		freeSimpleList(oldCombinedPotentials); oldCombinedPotentials = NULL;
	}

	result = combinedPotentials;

finish:
	freeSimpleList(oldCombinedPotentials);
	freeSimpleList(potentials);
	freeSimpleList(documents);
	return result;
}

static GList *getPotentialTags(context_t *context,
		char *document, err_t *e)
{
	GList *result = NULL;
	GList *haveAlready = NULL;
	GList *areEligible;
	GList *cur = NULL;
	GList *found = NULL;

	terror(areEligible = getAllTags(context, e))
	terror(haveAlready = getTags(context, document, e))

	for (cur = haveAlready; cur != NULL; cur = g_list_next(cur)) {
		found = g_list_find_custom(areEligible, cur->data, (GCompareFunc) &strcmp);
		terror(failIfFalse(found != NULL))
		free(found->data);
		areEligible = g_list_delete_link(areEligible, found);
	}

	terror(result = getPotentialTags2(context, document,
			haveAlready, areEligible, e))

finish:
	freeSimpleList(haveAlready);
	freeSimpleList(areEligible);
	return result;
}

static gboolean processDocument(context_t *context, char *docName)
{
	gboolean suggestTags(err_t *e) {
		gboolean result = FALSE;

		gboolean ignoreTagFunc(context_t *context, char *tag, err_t *e) {
			gboolean result = FALSE;

			terror(result = hasTag(context, document, tag, e))
finish:
			return result;
		}

		void tagYesFunc(context_t *context, char *tag, err_t *e) {
			terror(doTag(context, document, tag, e))
			result = TRUE;
finish:
			return;
		}

		GList *documentTags = NULL;
		GList *potentialTags = NULL;
		GList *potentialTags1 = NULL;
		GList *potentialTags2 = NULL;
		GList *cur = NULL;
		GList *link = NULL;

		terror(documentTags = getTags(context, document, e))
		terror(potentialTags1 = getPotentialTags(context, document, e))
		terror(potentialTags2 = getTagSuggestions(context, documentTags, NULL, e))
		for (cur = potentialTags1; cur != NULL; cur = g_list_next(cur)) {
			if (g_list_find_custom(potentialTags, cur->data,
					(GCompareFunc) &strcmp) != NULL) {
				continue;
			}
			potentialTags = g_list_append(potentialTags, strdup(cur->data));
		}
		for (cur = potentialTags2; cur != NULL; cur = g_list_next(cur)) {
			if (g_list_find_custom(potentialTags, cur->data,
					(GCompareFunc) &strcmp) != NULL) {
				continue;
			}
			potentialTags = g_list_append(potentialTags, strdup(cur->data));
		}

		for (cur = documentTags; cur != NULL; cur = g_list_next(cur)) {
			link = g_list_find_custom(potentialTags, cur->data, (GCompareFunc) &strcmp);
			if (link == NULL) {
				continue;
			}
			free(link->data);
			potentialTags = g_list_delete_link(potentialTags, link);
		}

		terror(tagLoop(context, &ignoreTagFunc, &tagYesFunc, potentialTags, FALSE, e))

		if (g_list_length(potentialTags) > 0) {
			editDocument(context, docName);
		}

finish:
		freeSimpleList(potentialTags1);
		freeSimpleList(potentialTags2);
		freeSimpleList(potentialTags);
		freeSimpleList(documentTags);
		return result;
	}

	err_t error;
	err_t *e = &error;
	gboolean result = FALSE;
	char *prompt = NULL;
	char *input = NULL;
	GList *argList = NULL;
	date_t *date = NULL;
	gboolean ok = FALSE;
	GList *cur = NULL;
	gboolean suggested = FALSE;

	initErr(e);

	terror(view(context, docName, e))
	document = docName;

	for(;;) {
		free(input); input = NULL;
		terror(input = getPlainInput(context, "Enter date(s) or \"done\"$ ", e))
		if (strcmp(input, "done") == 0) {
			break;
		}
		freeSimpleList(argList); argList = NULL;
		argList = getArgList(input);
		ok = FALSE;
		for (cur = argList; cur != NULL; cur = g_list_next(cur)) {
			free(date); date = NULL;
			if ((date = parseDate(cur->data, NULL)) == NULL) {
				ok = FALSE;
				break;
			}
			ok = TRUE;
		}
		if (!ok) {
			printf("Invalid date found!\n");
			continue;
		}
		for (cur = argList; cur != NULL; cur = g_list_next(cur)) {
			free(date); date = NULL;
			date = parseDate(cur->data, NULL);
			terror(doDate(context, docName, date, e))
		}
		break;
	}

	editDocument(context, docName);

	do {
		terror(suggested = suggestTags(e))
	} while (suggested);

	prompt = g_strdup_printf("process more?$ ");
	terror(commandLoop(context, prompt, processMoreCommands, e))
	result = !doProcessMore;

	terror(setProcessed(context, docName, e))

finish:
	if (argList != NULL) {
		freeSimpleList(argList);
	}
	free(input); input = NULL;
	free(prompt);
	return result;
}

static gboolean doProcess(context_t *context)
{
	gboolean result = FALSE;
	char *unprocessed = NULL;

	unprocessed = getUnprocessed(context);
	if (unprocessed == NULL) {
		result = TRUE;
		goto finish;
	}
	result = processDocument(context, unprocessed);

finish:
	free(unprocessed);
	return result;
}

gboolean cmdProcess(context_t *context, GList *args)
{
	char *endptr = NULL;
	uint number = 0;
	uint i = 0;
	gboolean stop = FALSE;

	if (args != NULL) {
		if (*((char *) args->data) == '\0') {
			goto finish;
		}
		number = strtoul(args->data, &endptr, 0);
		if (*endptr != '\0') {
			goto finish;
		}
	}
	while ((number == 0)||(i < number)) {
		stop = doProcess(context);
		if (stop) {
			break;
		}
		i++;
	}

finish:
	return FALSE;
}
