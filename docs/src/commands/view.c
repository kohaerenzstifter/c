#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../docs.h"

char *viewAliases[] = { "view", "open", NULL };

GList *getCompletionsView(context_t *context, const char *line)
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
			for (cur = docNames; cur != NULL;
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

gboolean validateArgsView(GList *args, context_t *context)
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

gboolean cmdView(context_t *context, GList *args)
{
	err_t error;
	err_t *e = &error;

	initErr(e);

	terror(view(context, args->data, e))

finish:
	return FALSE;
}
