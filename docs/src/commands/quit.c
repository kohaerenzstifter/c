#include <stdlib.h>
#include <glib.h>
#include <stdio.h>

#include "../docs.h"

char *quitAliases[] = { "quit", "exit", NULL };

gboolean validateArgsQuit(GList *args, context_t *context)
{
	return args == NULL;
}

GList *getCompletionsQuit(context_t *context, const char *line)
{
	GList *result = NULL;

	result = getCompletionsGeneric(context, line, &getNextTokenNoTokens);

	return result;
}

gboolean cmdQuit(context_t *context, GList *args)
{
	printf("Bye!\n");
	return TRUE;
}
