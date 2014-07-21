#ifndef DOCS_H_
#define DOCS_H_

#include <kohaerenzstiftung.h>

typedef struct _context context_t;

typedef gboolean (*commandFunc_t)(context_t *context, GList *args);
typedef gboolean (*getNextTokenFunc_t)(GList *tokens,
		GList **options, gboolean haveMore);
typedef GList *(*commandCompletionFunc_t)(context_t *context,
		const char *line);
typedef gboolean(*validateArgs_t)(GList *args, context_t *context);
typedef gboolean (*ignoreTagFunc_t)(context_t *context, char *tag, err_t *e);
typedef void (*tagYesFunc_t)(context_t *context, char *tag, err_t *e);

typedef struct _command {
	commandCompletionFunc_t getCompletions;
	validateArgs_t validateArgs;
	commandFunc_t func;
	char **aliases;
} command_t;

#define YEAR 0
#define MONTH 1
#define DAY 2

typedef struct _date {
	int type;
	int year;
	int month;
	int day;
} date_t;

GList *getCompletionsGeneric(context_t *context, const char *line,
		getNextTokenFunc_t getNextToken);
void freeSimpleList(GList *freeMe);
void commandLoop(context_t *context, char *prompt,
		command_t *commands, err_t *e);

GList *getDocNames(context_t *context);
gboolean documentExists(context_t *context, char *name, err_t *e);
gboolean getNextTokenNoTokens(GList *tokens, GList **options,
		gboolean haveMore);
char *getViewCommand(context_t *context);
char *getBaseDir(context_t *context);
int readHighest(context_t *context, err_t *e);
int getHighest(context_t *context, err_t *e);
void setHighest(context_t *context, int highest);
void addDoc(context_t *context, char *addMe, gboolean encrypted);
void doTag(context_t *context, char *docName, char *tagName, err_t *e);
gboolean hasTag(context_t *context, char *docName, char *tag, err_t *e);
GList *getTags(context_t *context, char *document, err_t *e);
GList *getAllTags(context_t *context, err_t *e);
void view(context_t *context, char *document, err_t *e);
void viewUnsubmitted(context_t *context, char *docName, err_t *e);
void doUntag(context_t *context, char *document, char *tag, err_t *e);
void doDate(context_t *context, char *document, date_t *date, err_t *e);
void doUndate(context_t *context, char *document, date_t *date, err_t *e);
gboolean hasDate(context_t *context, char *docName,
		date_t *date, gboolean exact);
GList *getAllDates(context_t *context, char *docName, err_t *e);
char *getUnprocessed(context_t *context);
void editDocument(context_t *context, char *docName);
date_t *parseDate(char *parseMe, err_t *e);
void setProcessed(context_t *context, char *docName, err_t *e);
char *getPlainInput(context_t *context, char *prompt, err_t *e);
void tagLoop(context_t *context, ignoreTagFunc_t ignoreTagFunc,
		tagYesFunc_t tagYesFunc, GList *tags, gboolean getTags, err_t *e);
GList *findDocs(context_t *context, GList *tags, date_t *date, err_t *e);
gboolean tagExists(context_t *context, char *name);
GList *getDocuments(context_t *context, char *tagName, err_t *e);
GList *getArgList(const char *commandString);
GList *cloneStringList(GList *cloneMe);
void showStringList(GList *showMe);
GList *getTagSuggestions(context_t *context, GList *haveTags,
		date_t *date, err_t *e);
#endif /* DOCS_H_ */
