/*
 * help.h
 *
 *  Created on: Mar 3, 2013
 *      Author: sancho
 */

#ifndef HELP_H_
#define HELP_H_

GList *getCompletionsHelp(context_t *context, const char *line);
gboolean validateArgsHelp(GList *args, context_t *context);
gboolean cmdHelp(context_t *context, GList *args);
extern char *helpAliases[];

#endif /* HELP_H_ */
