#include <glib.h>
#include <stdio.h>

#include "../docs.h"





char *helpAliases[] = { "help", NULL };



gboolean validateArgsHelp(GList *args, context_t *context)
{
	return args == NULL;
}

GList *getCompletionsHelp(context_t *context, const char *line)
{

	GList *result = NULL;

	result = getCompletionsGeneric(context, line, &getNextTokenNoTokens);

	return result;
}

gboolean cmdHelp(context_t *context, GList *args)
{
	//TODO
	printf("HELP!!!\n");
	return FALSE;
}
