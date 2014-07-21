/*
 * quit.h
 *
 *  Created on: Mar 3, 2013
 *      Author: sancho
 */

#ifndef QUIT_H_
#define QUIT_H_

GList *getCompletionsQuit(context_t *context, const char *line);
gboolean validateArgsQuit(GList *args, context_t *context);
gboolean cmdQuit(context_t *context, GList *args);

extern char *quitAliases[];

#endif /* QUIT_H_ */
