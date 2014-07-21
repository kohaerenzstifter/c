/*
 * view.h
 *
 *  Created on: Mar 3, 2013
 *      Author: sancho
 */

#ifndef VIEW_H_
#define VIEW_H_

gboolean cmdView(context_t *context, GList *args);
GList *getCompletionsView(context_t *context, const char *line);
gboolean validateArgsView(GList *args, context_t *context);
extern char *viewAliases[];

#endif /* VIEW_H_ */
