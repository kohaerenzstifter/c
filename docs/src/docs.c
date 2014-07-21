/*
 ============================================================================
 Name        : docs.c
 Author      : Martin Knappe
 Version     :
 Copyright   : 
 Description : Command Line Document Management Tool
 ============================================================================
 */

//xdg-open

#include <stdio.h>
#include <locale.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include <libtecla.h>
#include <glib.h>

#include <kohaerenzstiftung.h>

#include "docs.h"

#include "commands/help.h"
#include "commands/submit.h"
#include "commands/quit.h"
#include "commands/edit.h"
#include "commands/find.h"
#include "commands/view.h"
#include "commands/process.h"

typedef struct _document {
	char *name;
	gboolean encrypted;
	GList *allTags; //list of tag_t
	GList *allDates; //list of yearMonthDay_t
} document_t;

typedef struct _tag {
	char *name;
	GList *allDocuments; //list of document_t
} tag_t;

typedef struct _yearMonthDay {
	int type;
	GList *documents; //list of document_t
	struct _yearMonthDay *parent;
	union {
		struct {
			int value;
			GList *months; //list of yearMonthDay_t (type DAY)
		} year;
		struct {
			int value;
			GList *days; //list of yearMonthDay_t (type DAY)
		} month;
		struct {
			int value;
		} day;
	};
} yearMonthDay_t;

typedef struct _commandCompletion {
	int word_start;
	char *suffix;
	char *type_suffix;
	char *cont_suffix;
} commandCompletion_t;

typedef int (*completionFunc_t)(context_t *context, WordCompletion *cpl,
		const char *line, int word_end, err_t *e);

struct _context {
	completionFunc_t completionFunc;
	GetLine *gl;
	command_t *commands;

	char *viewCommand;
	char *baseDir;

	GHashTable *allDocuments; //hash table of document_t
	GList *unprocessedDocuments; //list of document_t
	GList *allTags; //ordered list of tag_t
	//GList *years; //list of year_t
	GList *yearMonthDays;
	int highest;
};

//TODO: commands may NOT contain spaces!
//TODO: command aliases must be UNIQUE!
static command_t commands[] = {
		{
				&getCompletionsHelp,
				&validateArgsHelp,
				&cmdHelp,
				helpAliases
		},
		{
				&getCompletionsQuit,
				&validateArgsQuit,
				&cmdQuit,
				quitAliases
		},
		{
				&getCompletionsSubmit,
				&validateArgsSubmit,
				&cmdSubmit,
				submitAliases
		},
		{
				&getCompletionsEdit,
				&validateArgsEdit,
				&cmdEdit,
				editAliases
		},
		{
				&getCompletionsFind,
				&validateArgsFind,
				&cmdFind,
				findAliases
		},
		{
				&getCompletionsView,
				&validateArgsView,
				&cmdView,
				viewAliases
		},
		{
				&getCompletionsProcess,
				&validateArgsProcess,
				&cmdProcess,
				processAliases
		},
		{
				NULL
		}
};

#define WONT_MAKE_SENSE "won't make sense...\n"
#define COMMAND_COMPLETE "Command complete (may hit Enter)!\n"

#define completerPrintf(gl, ...) \
  gl_normal_io(gl); \
  printf(__VA_ARGS__); \
  gl_raw_io(gl)

static char *configViewCommand = NULL;
static char *configBaseDir = NULL;

date_t *parseDate(char *parseMe, err_t *e)
{
	defineError();

	date_t *result = NULL;
	int year = -1;
	int month = -1;
	int day = -1;
	char *endptr = NULL;
	gchar **dateTokens = NULL;

	dateTokens = g_strsplit(parseMe, ":", 3);
	terror(failIfFalse(*dateTokens[0] != '\0'))
	year = strtoul(dateTokens[0], &endptr, 0);
	terror(failIfFalse((*endptr) == '\0'))
	if (dateTokens[1] != NULL) {
		terror(failIfFalse(*dateTokens[1] != '\0'))
		month = strtoul(dateTokens[1], &endptr, 0);
		terror(failIfFalse(*endptr == '\0'))
		terror(failIfFalse((month > 0)&&(month < 13)))
		if (dateTokens[2] != NULL) {
			terror(failIfFalse(*dateTokens[2] != '\0'))
			day = strtoul(dateTokens[2], &endptr, 0);
			terror(failIfFalse(*endptr == '\0'))
			terror(failIfFalse((day > 0)&&(day < 32)))
		}
	}

	result = calloc(1, sizeof(date_t));

	if (month < 0) {
		result->type = YEAR;
		result->year = year;
	} else {
		if (day < 0) {
			result->type = MONTH;
			result->year = year;
			result->month = month;
		} else {
			result->type = DAY;
			result->year = year;
			result->month = month;
			result->day = day;
		}
	}

finish:
	if (dateTokens != NULL) {
		g_strfreev(dateTokens);
	}
	return result;
}

char *getUnprocessed(context_t *context)
{
	char *result = NULL;
	document_t *document = NULL;

	if (context->unprocessedDocuments == NULL) {
		goto finish;
	}
	document = context->unprocessedDocuments->data;
	result = strdup(document->name);
finish:
	return result;
}

GList *cloneStringList(GList *cloneMe)
{
	GList *result = NULL;
	GList *cur = NULL;

	for (cur = cloneMe; cur != NULL; cur = g_list_next(cur)) {
		result = g_list_append(result, strdup(cur->data));
	}
	return result;
}

static GList *getDateList(GList *cloneMe)
{
	GList *result = NULL;
	GList *cur = NULL;
	date_t *newDate = NULL;
	yearMonthDay_t *yearMonthDay = NULL;

	for (cur = cloneMe; cur != NULL; cur = g_list_next(cur)) {
		yearMonthDay = cur->data;
		newDate = calloc(1, sizeof(date_t));
		newDate->type = yearMonthDay->type;
		if (yearMonthDay->type == DAY) {
			newDate->day = yearMonthDay->day.value;
			yearMonthDay = yearMonthDay->parent;
		}
		if (yearMonthDay->type == MONTH) {
			newDate->month = yearMonthDay->month.value;
			yearMonthDay = yearMonthDay->parent;
		}
		if (yearMonthDay->type == YEAR) {
			newDate->year = yearMonthDay->year.value;
			yearMonthDay = yearMonthDay->parent;
		}
		result = g_list_append(result, newDate);
	}
	return result;
}

gboolean hasDate(context_t *context, char *docName,
		date_t *date, gboolean exact)
{
	err_t error;
	err_t *e = &error;
	gboolean result = FALSE;
	document_t *document = NULL;
	GList *cur = NULL;
	yearMonthDay_t *yearMonthDay = NULL;

	initErr(e);

	document = g_hash_table_lookup(context->allDocuments, docName);
	terror(failIfFalse(document != NULL))
	for (cur = document->allDates; cur != NULL; cur = g_list_next(cur)) {
		yearMonthDay = cur->data;
		if (yearMonthDay->type < date->type) {
			//not exact enough...
			continue;
		}
		if ((exact)&&(yearMonthDay->type != date->type)) {
			continue;
		}
		while (yearMonthDay->type > date->type) {
			yearMonthDay = yearMonthDay->parent;
		}
		if (date->type >= DAY) {
			if (date->day != yearMonthDay->day.value) {
				continue;
			}
			yearMonthDay = yearMonthDay->parent;
		}
		if (date->type >= MONTH) {
			if (date->month != yearMonthDay->month.value) {
				continue;
			}
			yearMonthDay = yearMonthDay->parent;
		}
		if (date->type >= YEAR) {
			if (date->year != yearMonthDay->year.value) {
				continue;
			}
			yearMonthDay = yearMonthDay->parent;
		}
		result = TRUE;
		break;
	}
finish:
	return result;
}

GList *getAllDates(context_t *context, char *docName, err_t *e)
{
	document_t *document = NULL;
	GList *result = NULL;

	document = g_hash_table_lookup(context->allDocuments, docName);
	terror(failIfFalse(document != NULL))
	result = getDateList(document->allDates);
finish:
	return result;
}

void setProcessed(context_t *context, char *docName, err_t *e)
{
	GList *link = NULL;
	char *baseDir = NULL;
	GList *cur = NULL;
	document_t *document = NULL;

	baseDir = getBaseDir(context);

	for (cur = context->unprocessedDocuments; cur != NULL;
			cur = g_list_next(cur)) {
		document = cur->data;
		if (strcmp(document->name, docName) == 0) {
			link = cur;
			break;
		}
	}

	failIfFalse(link != NULL);
	context->unprocessedDocuments =
			g_list_delete_link(context->unprocessedDocuments, link);
	terror(runSync(e, "ln -s ../unencrypted/%s %s/processed/%s",
			docName, baseDir, docName))
finish:
	return;
}

gboolean tagExists(context_t *context, char *name)
{
	gboolean result = FALSE;
	GList *cur = NULL;
	tag_t *tag = NULL;

	for (cur = context->allTags; cur != NULL; cur = g_list_next(cur)) {
		tag = cur->data;
		if (strcmp(tag->name, name) == 0) {
			result = TRUE;
			break;
		}
	}

	return result;
}

GList *findDocs(context_t *context, GList *tags,
		date_t *date, err_t *e)
{
	GList *result = NULL;

	gboolean tagWanted(tag_t *tag, GList *tags) {
		GList *cur = NULL;
		gboolean result = FALSE;

		for (cur = tags; cur != NULL; cur = g_list_next(cur)) {
			if (strcmp(tag->name, cur->data) == 0) {
				result = TRUE;
				break;
			}
		}
		return result;
	}

	void addAllDocs(tag_t *tag) {
		GList *cur = NULL;
		document_t *document = NULL;

		for (cur = tag->allDocuments; cur != NULL; cur = g_list_next(cur)) {
			document = cur->data;
			result = g_list_append(result, strdup(document->name));
		}
	}

	GList *keepAllDocs(tag_t *tag) {
		GList *result2 = NULL;
		GList *cur = NULL;
		gboolean doesHaveTag = FALSE;

		for (cur = result; cur != NULL; cur = g_list_next(cur)) {
			terror(doesHaveTag = hasTag(context, cur->data, tag->name, e))
			if (doesHaveTag) {
				result2 = g_list_append(result2, strdup(cur->data));
			}
		}

finish:
		return result2;
	}

	void addDocument(gpointer key, gpointer value, gpointer user_data) {
		document_t *document = value;

		result = g_list_append(result, strdup(document->name));
	}

	GList *cur = NULL;
	gboolean addAll = TRUE;
	GList *tmp = NULL;
	document_t *document = NULL;

	if (tags != NULL) {
		for (cur = context->allTags; cur != NULL; cur = g_list_next(cur)) {
			tag_t *tag = cur->data;
			if (!tagWanted(tag, tags)) {
				continue;
			}
			if (addAll) {
				addAllDocs(tag);
				addAll = FALSE;
			} else {
				tmp = keepAllDocs(tag);
				freeSimpleList(result);
				result = tmp; tmp = NULL;
			}
		}
	} else {
		g_hash_table_foreach(context->allDocuments, &addDocument, NULL);
	}

	if (date != NULL) {
		for (cur = result; cur != NULL; cur = g_list_next(cur)) {
			document = g_hash_table_lookup(context->allDocuments, cur->data);
			terror(failIfFalse(document != NULL))
			if (document->allDates != NULL) {
				if (!hasDate(context, cur->data, date, FALSE)) {
					continue;
				}
			}
			tmp = g_list_append(tmp, strdup(cur->data));
		}
		freeSimpleList(result);
		result = tmp; tmp = NULL;
	}
finish:
	freeSimpleList(tmp);
	return result;
}

static void addDocumentsTags(context_t *context,
		char *document, GList **list, err_t *e)
{
	GList *documentsTags = NULL;
	GList *cur = NULL;

	terror(documentsTags = getTags(context, document, e))
	for (cur = documentsTags; cur != NULL; cur = g_list_next(cur)) {
		if (g_list_find_custom(*list, cur->data,
				(GCompareFunc) &strcmp) != NULL) {
			continue;
		}

		*list = g_list_append(*list, strdup(cur->data));
	}

finish:
	freeSimpleList(documentsTags);
	return;
}

GList *getTagSuggestions(context_t *context, GList *haveTags,
		date_t *date, err_t *e)
{
	GList *result = NULL;
	GList *docs = NULL;
	GList *cur = NULL;

	terror(docs = findDocs(context, haveTags, date, e))
	for (cur = docs; cur != NULL; cur = g_list_next(cur)) {
		terror(addDocumentsTags(context, cur->data, &result, e))
	}

finish:
	freeSimpleList(docs);
	return result;
}

static yearMonthDay_t *getYearMonthDay(context_t *context, date_t *date)
{
	yearMonthDay_t *result = NULL;
	GList *cur = NULL;
	yearMonthDay_t *yearMonthDay = NULL;
	yearMonthDay_t *month = NULL;
	yearMonthDay_t *year = NULL;

	for (cur = context->yearMonthDays; cur != NULL; cur = g_list_next(cur)) {
		yearMonthDay = cur->data;
		if (yearMonthDay->year.value == date->year) {
			break;
		}
		yearMonthDay = NULL;
	}
	if (yearMonthDay == NULL) {
		yearMonthDay = calloc(1, sizeof(yearMonthDay_t));
		yearMonthDay->type = YEAR;
		yearMonthDay->year.value = date->year;
		yearMonthDay->parent = NULL;
		context->yearMonthDays =
				g_list_append(context->yearMonthDays, yearMonthDay);
	}
	if (date->type == YEAR) {
		result = yearMonthDay;
		goto finish;
	}
	year = yearMonthDay; yearMonthDay = NULL;
	for (cur = year->year.months; cur != NULL; cur = g_list_next(cur)) {
		yearMonthDay = cur->data;
		if (yearMonthDay->month.value == date->month) {
			break;
		}
		yearMonthDay = NULL;
	}
	if (yearMonthDay == NULL) {
		yearMonthDay = calloc(1, sizeof(yearMonthDay_t));
		yearMonthDay->type = MONTH;
		yearMonthDay->month.value = date->month;
		yearMonthDay->parent = year;
		year->year.months = g_list_append(year->year.months, yearMonthDay);
	}
	if (date->type == MONTH) {
		result = yearMonthDay;
		goto finish;
	}
	month = yearMonthDay; yearMonthDay = NULL;
	for (cur = month->month.days; cur != NULL; cur = g_list_next(cur)) {
		yearMonthDay = cur->data;
		if (yearMonthDay->day.value == date->day) {
			break;
		}
		yearMonthDay = NULL;
	}
	if (yearMonthDay == NULL) {
		yearMonthDay = calloc(1, sizeof(yearMonthDay_t));
		yearMonthDay->type = DAY;
		yearMonthDay->day.value = date->day;
		yearMonthDay->parent = month;
		month->month.days = g_list_append(month->month.days, yearMonthDay);
	}
	result = yearMonthDay;
finish:
	return result;
}

void doDate(context_t *context, char *docName, date_t *date, err_t *e)
{
	yearMonthDay_t *yearMonthDay = NULL;
	document_t *document = NULL;
	char *baseDir = NULL;

	baseDir = getBaseDir(context);

	if (hasDate(context, docName, date, FALSE)) {
		goto finish;
	}

	document = g_hash_table_lookup(context->allDocuments, docName);
	terror(failIfFalse(document != NULL))

	yearMonthDay = getYearMonthDay(context, date);

	document->allDates = g_list_append(document->allDates, yearMonthDay);
	yearMonthDay->documents = g_list_append(yearMonthDay->documents, document);
	if (date->type == YEAR) {
		terror(runSync(e, "mkdir -p %s/years/%d", baseDir, date->year))
		terror(runSync(e, "ln -s ../../unencrypted/%s %s/years/%d/%s",
				docName, baseDir, date->year, docName))
	} else {
		if (date->type == MONTH) {
			terror(runSync(e, "mkdir -p %s/years/%d/%d",
					baseDir, date->year, date->month))
			terror(runSync(e, "ln -s ../../../unencrypted/%s %s/years/%d/%d/%s",
					docName, baseDir, date->year,
					date->month, docName))
		} else {
			terror(runSync(e, "mkdir -p %s/years/%d/%d/%d",
					baseDir, date->year,
					date->month, date->day))
			terror(runSync(e, "ln -s "
					"../../../../unencrypted/%s %s/years/%d/%d/%d/%s",
					docName, baseDir, date->year,
					date->month, date->day,
					docName))
		}
	}

finish:
	return;
}

void doUndate(context_t *context, char *docName, date_t *date, err_t *e)
{
	document_t *document = NULL;
	GList *link = NULL;
	char *baseDir = NULL;
	GList *cur = NULL;
	yearMonthDay_t *yearMonthDay = NULL;
	yearMonthDay_t *theYearMonthDay = NULL;

	baseDir = getBaseDir(context);

	if (!hasDate(context, docName, date, TRUE)) {
		goto finish;
	}

	document = g_hash_table_lookup(context->allDocuments, docName);
	terror(failIfFalse(document != NULL))

	for (cur = document->allDates; cur != NULL; cur = g_list_next(cur)) {
		yearMonthDay = cur->data;
		if (yearMonthDay->type != date->type) {
			continue;
		}
		theYearMonthDay = yearMonthDay;
		if (date->type >= DAY) {
			if (date->day != yearMonthDay->day.value) {
				continue;
			}
			yearMonthDay = yearMonthDay->parent;
		}
		if (date->type >= MONTH) {
			if (date->month != yearMonthDay->month.value) {
				continue;
			}
			yearMonthDay = yearMonthDay->parent;
		}
		if (date->type >= YEAR) {
			if (date->year != yearMonthDay->year.value) {
				continue;
			}
			yearMonthDay = yearMonthDay->parent;
		}
		link = cur;
		break;
	}

	terror(failIfFalse(link != NULL))

	document->allDates = g_list_delete_link(document->allDates, link);
	link = g_list_find(theYearMonthDay->documents, document);
	terror(failIfFalse(link != NULL))
	theYearMonthDay->documents =
			g_list_delete_link(theYearMonthDay->documents, link);

	if (date->type == YEAR) {
		terror(runSync(e, "rm -rf %s/years/%d/%s",
				baseDir, date->year, docName))
	} else {
		if (date->type == MONTH) {
			terror(runSync(e, "rm -rf %s/years/%d/%d/%s",
					baseDir, date->year,
					date->month, docName))
		} else {
			terror(runSync(e, "rm -rf "
					"%s/years/%d/%d/%d/%s",
					baseDir, date->year,
					date->month, date->day,
					docName))
		}
	}
finish:
	return;
}

GList *getTags(context_t *context, char *name, err_t *e)
{
	GList *result = NULL;
	document_t *document = NULL;
	GList *cur = NULL;
	tag_t *tag = NULL;

	document = g_hash_table_lookup(context->allDocuments, name);

	terror(failIfFalse(document != NULL))

	for (cur = document->allTags; cur != NULL; cur = g_list_next(cur)) {
		tag = cur->data;
		result = g_list_append(result, strdup(tag->name));
	}

finish:
	return result;
}

GList *getAllTags(context_t *context, err_t *e)
{
	GList *result = NULL;
	GList *cur = NULL;
	tag_t *tag = NULL;

	for (cur = context->allTags; cur != NULL; cur = g_list_next(cur)) {
		tag = cur->data;
		result = g_list_append(result, strdup(tag->name));
	}

	return result;
}

static gint compareTags(gconstpointer a, gconstpointer b)
{
	const tag_t *tagA = a;
	const tag_t *tagB = b;
	int lengthA = -1;
	int lengthB = -1;

	lengthA = g_list_length(tagA->allDocuments);
	lengthB = g_list_length(tagB->allDocuments);

	return (lengthA < lengthB) ? -1 :
			(lengthA > lengthB) ? 1 : 0;
}

void doTag(context_t *context, char *docName, char *tagName, err_t *e)
{
	GList *cur = NULL;
	tag_t *tag = NULL;
	document_t *document = NULL;
	char *baseDir = NULL;
	GList *link = NULL;

	baseDir = getBaseDir(context);

	for (cur = context->allTags; cur != NULL; cur = g_list_next(cur)) {
		tag = cur->data;
		if (strcmp(tag->name, tagName) == 0) {
			break;
		}
		tag = NULL;
	}
	if (tag == NULL) {
		tag = calloc(1, sizeof(tag_t));
		tag->name = strdup(tagName);

		terror(runSync(e, "mkdir %s/tags/%s", baseDir, tagName))
	} else {
		link = g_list_find(context->allTags, tag);
		context->allTags = g_list_delete_link(context->allTags, link);
	}

	document = g_hash_table_lookup(context->allDocuments, docName);
	terror(failIfFalse(document != NULL))

	document->allTags = g_list_append(document->allTags, tag);
	tag->allDocuments = g_list_append(tag->allDocuments, document);
	context->allTags = g_list_insert_sorted(context->allTags, tag, &compareTags);

	terror(runSync(e, "ln -s ../../unencrypted/%s %s/tags/%s/%s",
			docName, baseDir, tagName, docName))
finish:
	return;
}

void doUntag(context_t *context, char *docName, char *tagName, err_t *e)
{
	GList *cur = NULL;
	tag_t *tag = NULL;
	document_t *document = NULL;
	char *baseDir = NULL;
	GList *link = NULL;

	baseDir = getBaseDir(context);

	for (cur = context->allTags; cur != NULL; cur = g_list_next(cur)) {
		tag = cur->data;
		if (strcmp(tag->name, tagName) == 0) {
			break;
		}
	}
	terror(failIfFalse(tag != NULL))
	document = g_hash_table_lookup(context->allDocuments, docName);
	terror(failIfFalse(document != NULL))

	link = g_list_find(document->allTags, tag);
	terror(failIfFalse(link != NULL))
	document->allTags = g_list_delete_link(document->allTags, link);

	link = g_list_find(tag->allDocuments, document);
	terror(failIfFalse(link != NULL))
	tag->allDocuments = g_list_delete_link(tag->allDocuments, link);

	link = g_list_find(context->allTags, tag);
	terror(failIfFalse(link != NULL))
	context->allTags = g_list_delete_link(context->allTags, link);

	context->allTags = g_list_insert_sorted(context->allTags, tag, &compareTags);

	terror(runSync(e, "rm -rf %s/tags/%s/%s", baseDir, tagName, docName))
finish:
	return;
}

gboolean hasTag(context_t *context, char *docName, char *tagName, err_t *e)
{
	gboolean result = FALSE;
	GList *cur = NULL;
	document_t *document = NULL;
	tag_t *tag = NULL;

	document = g_hash_table_lookup(context->allDocuments, docName);
	terror(failIfFalse(document != NULL))
	for (cur = document->allTags; cur != NULL; cur = g_list_next(cur)) {
		tag = cur->data;
		if (strcmp(tag->name, tagName) == 0) {
			result = TRUE;
			break;
		}
	}

finish:
	return result;
}

gboolean getNextTokenNoTokens(GList *tokens, GList **options,
		gboolean haveMore)
{
	gboolean result = FALSE;

	if ((tokens != NULL)||(haveMore)) {
		result = FALSE;
		options = NULL;
	} else {
		result = TRUE;
		options = NULL;
	}

	return result;
}

static GList *fixArgVector(gchar **argVector)
{
	int i = 0;
	GList *result = NULL;

	for (i = 0; argVector[i] != NULL; i++) {
		if (argVector[i][0] == '\0') {
			continue;
		}
		result = g_list_append(result, strdup(argVector[i]));
	}
	return result;
}

GList *getArgList(const char *commandString)
{
	char **vector = NULL;
	GList *result = NULL;

	vector = g_strsplit(commandString, " ", -1);
	result = fixArgVector(vector);
	return result;
}

static GList *complete(context_t *context, const char *start,
		int offset_word_start, GList *optionsList, gboolean mayHitEnter) {
	char *startCopy = NULL;
	GList *result = NULL;
	GList *cur = NULL;
	int optionLength = 0;
	int startLength = 0;
	commandCompletion_t *completion = NULL;
	int word_start = 0;
	gboolean sayWontMakeSense = FALSE;
	gboolean added = FALSE;

	startCopy = strdup(start);
	g_strstrip(startCopy);
	startLength = strlen(startCopy);

	for (cur = optionsList; cur != NULL; cur = g_list_next(cur)) {
		optionLength = strlen(cur->data);
		if (startLength > optionLength) {
			continue;
		}
		if (strncmp(startCopy, cur->data, startLength) != 0) {
			continue;
		}
		completion = calloc(1, sizeof(commandCompletion_t));
		completion->cont_suffix = " ";
		completion->suffix = strdup(cur->data + startLength);
		completion->type_suffix = "";
		if (start[0] != '\0') {
			word_start = strrstr(start, startCopy) - start;
			word_start = startCopy[0] != '\0' ? word_start : word_start + 1;
		} else {
			word_start = strlen(start);
		}
		completion->word_start = word_start + offset_word_start;
		result = g_list_append(result, completion);
		added = TRUE;
	}

	sayWontMakeSense = (!mayHitEnter) && (!added);

	if (sayWontMakeSense) {
		completerPrintf(context->gl, WONT_MAKE_SENSE);
	}
	if (mayHitEnter) {
		completerPrintf(context->gl, COMMAND_COMPLETE);
	}

	free(startCopy);
	return result;
}

void freeSimpleList(GList *freeMe)
{
	GList *cur = NULL;

	if (freeMe != NULL) {
		for (cur = freeMe; cur != NULL; cur = g_list_next(cur)) {
			free(cur->data);
		}
		g_list_free(freeMe);
	}
}

GList *getCompletionsGeneric(context_t *context, const char *line,
		getNextTokenFunc_t getNextToken)
{
	GList *result = NULL;
	GList *argList = NULL;
	GList *optionsList = NULL;
	gboolean lastComplete = FALSE;
	int lineLength = 0;
	char *lastToken = NULL;
	const char *start = NULL;
	int offset_word_start = 0;
	GList *last = NULL;
	gboolean mayHitEnter = FALSE;

	argList = getArgList(line);
	lineLength = strlen(line);
	lastComplete = ((line[lineLength - 1] == ' ')||
			(argList == NULL));
	if (lastComplete) {
		mayHitEnter = getNextToken(argList, &optionsList, !lastComplete);
		start = &line[lineLength];
		offset_word_start = lineLength;
	} else {
		//NOTE: last can never be NULL !!! (see condition for 'lastComplete')
		last = g_list_last(argList);
		argList = g_list_remove_link(argList, last);
		mayHitEnter = getNextToken(argList, &optionsList, !lastComplete);
		lastToken = last->data;
		start = strrstr(line, lastToken);
		offset_word_start = start - line;
	}

	result = complete(context, start, offset_word_start,
			optionsList, mayHitEnter);
	if (optionsList != NULL) {
		freeSimpleList(optionsList);
	}
	if (last != NULL) {
		freeSimpleList(last);
	}
	return result;
}

static command_t *findCommand(char *commandString, command_t *commands)
{
	command_t *command = NULL;
	command_t *result = NULL;
	int i = 0;
	int j = 0;
	char *commandName = strdup(commandString);
	char *space = strchr(commandName, ' ');
	if (space != NULL) {
		*space = '\0';
	}

	for (i = 0; commands[i].func != NULL; i++) {
		command = &commands[i];
		for (j = 0; command->aliases[j] != NULL; j++) {
			if (strcmp(commandString, command->aliases[j]) == 0) {
				result = command;
				break;
			}
		}
		if (result != NULL) {
			break;
		}
	}
	free(commandName);
	return result;
}

static void freeCommandCompletion(commandCompletion_t *commandCompletion)
{
	free(commandCompletion->suffix);
	free(commandCompletion);
}

static void freeCommandCompletions(GList *commandCompletions)
{
	commandCompletion_t *commandCompletion = NULL;
	GList *cur = NULL;

	for (cur = commandCompletions; cur != NULL; cur = g_list_next(cur)) {
		commandCompletion = cur->data;
		freeCommandCompletion(commandCompletion);
	}
	g_list_free(commandCompletions);
}

static int completeCommand(context_t *context, WordCompletion *cpl,
		const char *rawLine, int word_end, err_t *e)
{
	int result = 0;
	int i = 0;
	int j = 0;
	command_t *command = NULL;
	char *alias = NULL;
	char *line = NULL;
	int length = 0;
	char *type_suffix = "";
	char *cont_suffix = " ";
	char *suffix = NULL;
	int aliasLength = 0;
	char *rawLineCopy = NULL;
	gboolean haveCommand = FALSE;
	GList *args = NULL;
	int word_start = 0;
	char *start = NULL;
	int commandLength = 0;
	int distance = 0;
	GList *commandCompletions = NULL;
	GList *cur = NULL;
	commandCompletion_t *commandCompletion = NULL;
	char *commandName = NULL;
	gboolean added = FALSE;

	rawLineCopy = strdup(rawLine);
	line = g_strchug(rawLineCopy);
	length = strlen(line);

	args = getArgList(line);

	if (args != NULL) {
		if (line[length - 1] == ' ') {
			haveCommand = TRUE;
		} else {
			if (g_list_next(args) != NULL) {
				haveCommand = TRUE;
			}
		}
	}

	if (!haveCommand) {
		for (word_start = 0; ((rawLine[word_start] == ' ')
				&&(rawLine[word_start] != '\0')); word_start++);
		for (i = 0; context->commands[i].func != NULL; i++) {
			command = &context->commands[i];
			for (j = 0; command->aliases[j] != NULL; j++) {
				alias = command->aliases[j];
				if (length > (aliasLength = strlen(alias))) {
					continue;
				}
				if (strncmp(line, alias, length) != 0) {
					continue;
				}
				suffix = &alias[length];
				cpl_add_completion(cpl, rawLine, word_start, word_end, suffix,
						type_suffix, cont_suffix);
				added = TRUE;
			}
		}
		if (!added) {
			completerPrintf(context->gl, WONT_MAKE_SENSE);
		}
	} else {
		commandName = (char *) args->data;
		command = findCommand(commandName, context->commands);
		if (command != NULL) {
			start = strstr(rawLine, commandName);
			commandLength = strlen(commandName);
			distance = commandLength + (start - rawLine);
			if (command->getCompletions != NULL) {
				commandCompletions =
						command->getCompletions(context, start + commandLength);
				for (cur = commandCompletions; cur != NULL; cur = g_list_next(cur)) {
					commandCompletion = cur->data;
					word_start =
							commandCompletion->word_start + distance;
					suffix = commandCompletion->suffix;
					type_suffix = commandCompletion->type_suffix;
					cont_suffix = commandCompletion->cont_suffix;
					cpl_add_completion(cpl, rawLine, word_start,
							word_end, suffix, type_suffix,
							cont_suffix);
				}
			}
		} else {
			completerPrintf(context->gl, WONT_MAKE_SENSE);
		}
	}

	if (commandCompletions != NULL) {
		freeCommandCompletions(commandCompletions);
	}
	if (args != NULL) {
		freeSimpleList(args);
	}
	free(rawLineCopy);
	return result;
}

static int match_fn(WordCompletion *cpl, void *data,
		const char *line, int word_end)
{
	int result = 0;
	err_t error;
	err_t *e = &error;
	context_t *context = NULL;
	completionFunc_t completionFunc = NULL;

	initErr(e);

	terror(failIfFalse(data != NULL))
	context = data;
	completionFunc = context->completionFunc;

	if (completionFunc != NULL) {
		terror(result = completionFunc(context,
				cpl, line, word_end, e))
	}

finish:
	return result;
}

static GList *getCommand(context_t *context,
		char *prompt, err_t *e)
{
	char *commandString = NULL;
	char *line = NULL;
	char *nl = NULL;
	GList *result = NULL;
	completionFunc_t completionFunc = NULL;

	completionFunc = context->completionFunc;
	context->completionFunc = &completeCommand;

	terror(failIfFalse((line = gl_get_line(context->gl, prompt,
			NULL, -1)) != NULL))

	commandString = strdup(line);
	nl = strchr((commandString), '\n');
	if (nl != NULL) {
		*nl = '\0';
	}
	result = getArgList(commandString);

finish:
	free(commandString);
	context->completionFunc = completionFunc;
	return result;
}

static gboolean executeCommand(context_t *context, command_t *command,
		GList *args, err_t *e)
{
	gboolean result = FALSE;
	terror(failIfFalse(command->func != NULL))
	result = command->func(context, args);
finish:
	return result;
}

void commandLoop(context_t *context, char *prompt,
		command_t *commands, err_t *e)
{
	char *commandString = NULL;
	GList *args = NULL;
	char *commandName = NULL;
	command_t *command = NULL;
	GList *next = NULL;
	gboolean argsValid = FALSE;
	gboolean quit = FALSE;
	command_t *commandsBackup = NULL;

	commandsBackup = context->commands;
	context->commands = commands;

	for(;;) {
		if (args != NULL) {
			freeSimpleList(args); args = NULL;
		}
		free(commandString); commandString = NULL;
		terror(args = getCommand(context, prompt, e));
		if (args == NULL) {
			continue;
		}
		commandName = (char *) args->data;
		command = findCommand(commandName, context->commands);
		if (command == NULL) {
			printf("unknown command %s\n", commandName);
			continue;
		}
		next = g_list_next(args);
		if (command->validateArgs != NULL) {
			argsValid = command->validateArgs(next, context);
		} else {
			argsValid = TRUE;
		}
		if (!argsValid) {
			printf("invalid arguments\n");
		} else {
			terror(quit = executeCommand(context, command, next, e))
			if (quit) {
				break;
			}
		}
	}

finish:
	if (commandsBackup != NULL) {
		context->commands = commandsBackup;
	}
	if (args != NULL) {
		freeSimpleList(args);
	}
	if (commandString != NULL) {
		free(commandString);
	}
}

gboolean documentExists(context_t *context, char *name, err_t *e)
{
	gboolean result = FALSE;

	result = (g_hash_table_lookup(context->allDocuments, name) != NULL);

	return result;
}

static void freeDocument(document_t *document)
{
	free(document->name);
	g_list_free(document->allTags);
	g_list_free(document->allDates);

	free(document);
}

static void freeTag(tag_t *tag)
{
	free(tag->name);
	g_list_free(tag->allDocuments);

	free(tag);
}

static void readTag(context_t *context, char *name, err_t *e)
{
	char *baseDir = NULL;
	char *dirName = NULL;
	DIR *dir = NULL;
	struct dirent *dirent = NULL;
	document_t *document = NULL;
	tag_t *tag = NULL;

	tag = calloc(1, sizeof(tag_t));
	tag->name = strdup(name);

	baseDir = getBaseDir(context);
	dirName = g_strdup_printf("%s/tags/%s", baseDir, name);

	dir = opendir(dirName);
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
		document = g_hash_table_lookup(context->allDocuments,
				dirent->d_name);
		terror(failIfFalse(document != NULL))
		document->allTags = g_list_append(document->allTags, tag);
		tag->allDocuments = g_list_append(tag->allDocuments, document);
	}
	closedir(dir); dir = NULL;

	context->allTags =
			g_list_insert_sorted(context->allTags, tag, &compareTags); tag = NULL;
finish:
	if (tag != NULL) {
		freeTag(tag);
	}
	free(dirName);
	return;
}

static gboolean isDay(int year, int month, char *name)
{
	gboolean result = FALSE;
	uint dayNumber = 0;
	char *endptr = NULL;

	if (*name == '\0') {
		goto finish;
	}
	dayNumber = strtoul(name, &endptr, 0);
	if (*endptr != '\0') {
		goto finish;
	}
	if ((dayNumber < 1)||(dayNumber > 31)) {
		goto finish;
	}
	//NOTE: we could do more fancy stuff here, but is it necessary??
	result = TRUE;
finish:
	return result;
}

static void readDay(context_t *context, yearMonthDay_t *month,
		char *name, err_t *e)
{
	char *dirName = NULL;
	DIR *dir = NULL;
	struct dirent *dirent = NULL;
	int dayNumber = -1;
	char *endptr = NULL;
	yearMonthDay_t *day = NULL;
	yearMonthDay_t *year = NULL;
	document_t *document = NULL;
	char *baseDir = NULL;

	baseDir = getBaseDir(context);

	year = month->parent;

	terror(failIfFalse(*name != '\0'))
	dayNumber = strtoul(name, &endptr, 0);
	terror(failIfFalse(*endptr == '\0'))
	dirName = g_strdup_printf("%s/years/%d/%d/%s", baseDir,
			year->year.value, month->month.value, name);
	day = calloc(1, sizeof(yearMonthDay_t));
	day->type = DAY;
	day->parent = month;
	day->day.value = dayNumber;

	month->month.days = g_list_append(month->month.days, day);

	dir = opendir(dirName);
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
		document =
				g_hash_table_lookup(context->allDocuments, dirent->d_name);
		terror(failIfFalse(document != NULL))
		day->documents = g_list_append(day->documents,
				document);
		document->allDates = g_list_append(document->allDates, day);
	}
	closedir(dir); dir = NULL;

finish:
	if (dir != NULL) {
		closedir(dir);
	}
	free(dirName);
	return;
}

GList *getDocuments(context_t *context, char *tagName, err_t *e)
{
	GList *result = NULL;
	GList *cur = NULL;
	tag_t *tag = NULL;
	tag_t *theTag = NULL;
	document_t *document = NULL;

	for (cur = context->allTags; cur != NULL; cur = g_list_next(cur)) {
		tag = cur->data;
		if (strcmp(tagName, tag->name) == 0) {
			theTag = tag;
			break;
		}
	}

	terror(failIfFalse(theTag != NULL))

	for (cur = theTag->allDocuments; cur != NULL; cur = g_list_next(cur)) {
		document = cur->data;
		result = g_list_append(result, strdup(document->name));
	}

finish:
	return result;
}

static void readMonth(context_t *context, yearMonthDay_t *year,
		char *name, err_t *e)
{
	char *dirName = NULL;
	DIR *dir = NULL;
	struct dirent *dirent = NULL;
	int monthNumber = -1;
	char *endptr = NULL;
	yearMonthDay_t *month = NULL;
	document_t *document = NULL;
	char *baseDir = NULL;

	baseDir = getBaseDir(context);

	terror(failIfFalse(*name != '\0'))
	monthNumber = strtoul(name, &endptr, 0);
	terror(failIfFalse(*endptr == '\0'))
	dirName = g_strdup_printf("%s/years/%d/%s", baseDir,
			year->year.value, name);
	month = calloc(1, sizeof(yearMonthDay_t));
	month->type = MONTH;
	month->parent = year;
	month->month.value = monthNumber;

	year->year.months = g_list_append(year->year.months, month);

	dir = opendir(dirName);
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
		if (dirent->d_type == DT_DIR) {
			terror(failIfFalse(isDay(year->year.value,
					monthNumber, dirent->d_name)))
			terror(readDay(context, month, dirent->d_name, e))
		} else {
			document =
					g_hash_table_lookup(context->allDocuments, dirent->d_name);
			terror(failIfFalse(document != NULL))
			month->documents = g_list_append(month->documents,
					document);
			document->allDates = g_list_append(document->allDates, month);
		}
	}
	closedir(dir); dir = NULL;

finish:
	if (dir != NULL) {
		closedir(dir);
	}
	free(dirName);
	return;
}

static gboolean isMonth(char *name)
{
	gboolean result = FALSE;
	uint monthNumber = 0;
	char *endptr = NULL;

	if (*name == '\0') {
		goto finish;
	}
	monthNumber = strtoul(name, &endptr, 0);
	if (*endptr != '\0') {
		goto finish;
	}
	if ((monthNumber < 1)||(monthNumber > 12)) {
		goto finish;
	}
	result = TRUE;
finish:
	return result;
}

static void readYear(context_t *context, char *name, err_t *e)
{
	DIR *dir = NULL;
	struct dirent *dirent = NULL;
	char *dirName = NULL;
	char *endptr = NULL;
	yearMonthDay_t *year = NULL;
	int yearNumber = -1;
	document_t *document = NULL;
	char *baseDir = NULL;

	baseDir = getBaseDir(context);

	terror(failIfFalse(*name != '\0'))
	yearNumber = strtoul(name, &endptr, 0);
	terror(failIfFalse(*endptr == '\0'))
	dirName = g_strdup_printf("%s/years/%s", baseDir, name);

	year = calloc(1, sizeof(yearMonthDay_t));
	year->type = YEAR;
	year->parent = NULL;
	year->year.value = yearNumber;

	context->yearMonthDays = g_list_append(context->yearMonthDays, year);

	dir = opendir(dirName);
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
		if (dirent->d_type == DT_DIR) {
			terror(failIfFalse(isMonth(dirent->d_name)))
			terror(readMonth(context, year, dirent->d_name, e))
		} else {
			document =
					g_hash_table_lookup(context->allDocuments, dirent->d_name);
			terror(failIfFalse(document != NULL))
			year->documents = g_list_append(year->documents,
					document);
			document->allDates = g_list_append(document->allDates, year);
		}
	}
	closedir(dir); dir = NULL;
finish:
	if (dir != NULL) {
		closedir(dir);
	}
	free(dirName);
	return;
}

void editDocument(context_t *context, char *docName)
{
	doEditDocument(context, docName);
}

static void readData(context_t *context, err_t *e)
{
	char *baseDir = NULL;
	char *processedDir = NULL;
	char *unencryptedDir = NULL;
	char *encryptedDir = NULL;
	char *tagsDir = NULL;
	char *yearsDir = NULL;
	DIR *dir = NULL;
	struct dirent *dirent = NULL;
	document_t *document = NULL;
	gboolean documentDoesExist = FALSE;

	baseDir = getBaseDir(context);
	processedDir = g_strdup_printf("%s/processed", baseDir);
	unencryptedDir = g_strdup_printf("%s/unencrypted", baseDir);
	encryptedDir = g_strdup_printf("%s/encrypted", baseDir);
	tagsDir = g_strdup_printf("%s/tags", baseDir);
	yearsDir = g_strdup_printf("%s/years", baseDir);

	context->allDocuments = g_hash_table_new_full(&g_str_hash, &g_str_equal,
			&free, (GDestroyNotify) &freeDocument);

	dir = opendir(processedDir);
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
		document = calloc(1, sizeof(document_t));
		document->name = strdup(dirent->d_name);
		g_hash_table_insert(context->allDocuments,
				strdup(document->name), document);
		document = NULL;
	}
	closedir(dir); dir = NULL;

	dir = opendir(encryptedDir);
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
		document = g_hash_table_lookup(context->allDocuments, dirent->d_name);
		if (document != NULL) {
			document->encrypted = TRUE;
			document = NULL;
			continue;
		}
		document = calloc(1, sizeof(document_t));
		document->name = strdup(dirent->d_name);
		document->encrypted = TRUE;
		g_hash_table_insert(context->allDocuments,
				strdup(document->name), document);
		context->unprocessedDocuments =
				g_list_append(context->unprocessedDocuments,
				document);
		document = NULL;
	}
	closedir(dir); dir = NULL;

	dir = opendir(unencryptedDir);
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
		terror(documentDoesExist = documentExists(context, dirent->d_name, e))
		if (documentDoesExist) {
			continue;
		}
		document = calloc(1, sizeof(document_t));
		document->name = strdup(dirent->d_name);
		g_hash_table_insert(context->allDocuments,
				strdup(document->name), document);
		context->unprocessedDocuments =
				g_list_append(context->unprocessedDocuments,
				document);
		document = NULL;
	}
	closedir(dir); dir = NULL;

	dir = opendir(tagsDir);
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
		terror(readTag(context, dirent->d_name, e))
	}
	closedir(dir); dir = NULL;

	dir = opendir(yearsDir);
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
		terror(readYear(context, dirent->d_name, e))
	}
	closedir(dir); dir = NULL;

finish:
	if (dir != NULL) {
		closedir(dir);
	}
	if (document != NULL) {
		freeDocument(document);
	}
	free(processedDir);
	free(unencryptedDir);
	free(tagsDir);
	free(yearsDir);
}

GList *getDocNames(context_t *context)
{
	GList *result = NULL;
	GList *keys = NULL;

	result = cloneStringList((keys = g_hash_table_get_keys(context->allDocuments)));

	g_list_free(keys);

	return result;

}

int readHighest(context_t *context, err_t *e)
{
	DIR *dir = NULL;
	int result = 0;
	struct dirent *dirent = NULL;
	int current = -1;
	char *period = NULL;
	char *dirName = NULL;

	dirName = g_strdup_printf("%s/unencrypted", context->baseDir);
	dir = opendir(dirName);
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
		period = strchr(dirent->d_name, '.');
		if (period != NULL) {
			*period = '\0';
		}
		current = atoi(dirent->d_name);
		if (current > result) {
			result = current;
		}
	}
finish:
	free(dirName);
	return result;
}

int getHighest(context_t *context, err_t *e)
{
	int result = -1;

	result = context->highest;
	if (result >= 0) {
		goto finish;
	}
	terror(context->highest = readHighest(context, e))
	terror(result = getHighest(context, e))
finish:
	return result;
}

void setHighest(context_t *context, int highest)
{
	context->highest = highest;
}

void addDoc(context_t *context, char *addMe, gboolean encrypted)
{
	err_t error;
	err_t *e = &error;
	document_t *document = NULL;

	initErr(e);

	document = calloc(1, sizeof(document_t));
	document->name = strdup(addMe);
	document->encrypted = encrypted;
	g_hash_table_insert(context->allDocuments,
			strdup(document->name), document);

	return;
}

char *getViewCommand(context_t *context)
{
	return context->viewCommand;
}

static void doView(context_t *context, char *path, err_t *e)
{
	char *viewCommand = NULL;

	viewCommand = getViewCommand(context);

	terror(runSync(e, "%s %s &> /dev/null", viewCommand, path))

finish:
	return;
}

void view(context_t *context, char *docName, err_t *e)
{
	document_t *document = NULL;
	char path[PATH_MAX];

	document = g_hash_table_lookup(context->allDocuments, docName);

	snprintf(path, sizeof(path), "%s/%s/%s", context->baseDir,
			document->encrypted ? "encrypted" : "unencrypted", docName);
	terror(doView(context, path, e))

finish:
	return;
}

void viewUnsubmitted(context_t *context, char *docName, err_t *e)
{
	char path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/submit/%s", context->baseDir,
			docName);
	terror(doView(context, path, e))

finish:
	return;
}

static gboolean tagYesNo(context_t *context, char *tagName,
		err_t *e)
{
	gboolean result = FALSE;
	char *prompt = NULL;
	char *input = NULL;

	prompt = g_strdup_printf("%s [y/N]?", tagName);

	for(;;) {
		free(input); input = NULL;
		terror(input = getPlainInput(context, prompt, e))
		if (strcmp(input, "") == 0) {
			break;
		}
		if (strcasecmp(input, "y") == 0) {
			result = TRUE;
			break;
		}
	}

finish:
	free(input);
	free(prompt);
	return result;
}

void tagLoop(context_t *context, ignoreTagFunc_t ignoreTagFunc,
		tagYesFunc_t tagYesFunc, GList *tags, gboolean getTags, err_t *e)
{
	GList *cur = NULL;
	char *tag = NULL;
	gboolean ignoreTag = FALSE;
	gboolean doTagYesNo = FALSE;

	terror(failIfFalse(!((tags != NULL)&&(getTags))))

	if (getTags) {
		terror(tags = getAllTags(context, e))
	}
	for (cur = tags; cur != NULL; cur = g_list_next(cur)) {
		tag = cur->data;
		if (ignoreTagFunc != NULL) {
			terror(ignoreTag = ignoreTagFunc(context, tag, e))
		} else {
			ignoreTag = FALSE;
		}
		if (ignoreTag) {
			continue;
		}
		terror(doTagYesNo = tagYesNo(context, tag, e))
		if (doTagYesNo) {
			terror(tagYesFunc(context, tag, e))
		}
	}
finish:
	if (getTags) {
		freeSimpleList(tags);
	}
	return;
}

char *getBaseDir(context_t *context)
{
	return context->baseDir;
}

static void freeYearMonthDay(yearMonthDay_t *yearMonthDay)
{
	GList *cur = NULL;
	GList *list = NULL;

	if (yearMonthDay->documents != NULL) {
		g_list_free(yearMonthDay->documents);
	}
	list = (yearMonthDay->type == YEAR) ? yearMonthDay->year.months :
			(yearMonthDay->type == MONTH) ? yearMonthDay->month.days :
					NULL;

	for (cur = list; cur != NULL; cur = g_list_next(cur)) {
		freeYearMonthDay(cur->data);
	}

	if (list != NULL) {
		g_list_free(list);
	}
}

char *getPlainInput(context_t *context, char *prompt, err_t *e)
{
	char *result = NULL;
	completionFunc_t completionFunc = NULL;
	char *line = NULL;
	char *nl = NULL;

	completionFunc = context->completionFunc;
	context->completionFunc = NULL;

	terror(failIfFalse((line = gl_get_line(context->gl, prompt,
			NULL, -1)) != NULL))
	nl = strchr(line, '\n');
	if (nl != NULL) {
		*nl = '\0';
	}
	g_strstrip(line);
	result = strdup(line);

finish:
	context->completionFunc = completionFunc;
	return result;
}

int main(int argc, char *argv[])
{
	err_t error;
	err_t *e = &error;
	GList *cur = NULL;
	GetLine *gl = NULL;
	context_t context;
	GOptionContext *optionContext = NULL;
	GError *gerror = NULL;
	GOptionEntry optionEntries[] = {
			{
					"viewCommand",
					'v',
					0,
					G_OPTION_ARG_STRING,
					&configViewCommand,
					"Command to open documents",
					NULL
			},
			{
					"baseDir",
					'b',
					0,
					G_OPTION_ARG_STRING,
					&configBaseDir,
					"Base Directory",
					NULL
			},
			{
					NULL
			}
	};

	initErr(e);

	memset(&context, 0, sizeof(context_t));
	context.highest = -1;

	optionContext = g_option_context_new("");
	g_option_context_add_main_entries(optionContext,
			optionEntries, NULL);
	terror(failIfFalse(g_option_context_parse(optionContext,
			&argc, &argv, &gerror)))

	terror(failIfFalse(configViewCommand != NULL))
	terror(failIfFalse(configBaseDir != NULL))

	context.viewCommand = configViewCommand; configViewCommand = NULL;
	context.baseDir = configBaseDir; configBaseDir = NULL;

	setlocale(LC_CTYPE, "");
	terror(readData(&context, e))

	terror(failIfFalse(((gl = new_GetLine(1024, 2048)) != NULL)))
	context.gl = gl;
	gl_customize_completion(gl, &context, &match_fn);

	terror(commandLoop(&context, "$ ", commands, e))

finish:

	free(context.viewCommand);
	free(context.baseDir);
	if (context.allDocuments != NULL) {
		g_hash_table_destroy(context.allDocuments);
	}
	if (context.unprocessedDocuments != NULL) {
		g_list_free(context.unprocessedDocuments);
	}
	for (cur = context.allTags; cur != NULL; cur = g_list_next(cur)) {
		freeTag(cur->data);
	}
	g_list_free(context.allTags);
	for (cur = context.yearMonthDays; cur != NULL; cur = g_list_next(cur)) {
		freeYearMonthDay(cur->data);
	}
	g_list_free(context.yearMonthDays);
	if (gerror != NULL) {
		g_error_free(gerror);
	}
	if (optionContext != NULL) {
		g_option_context_free(optionContext);
	}
	if (gl != NULL) {
		gl = del_GetLine(gl);
	}
	return 0;
}
