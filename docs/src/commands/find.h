/*
 * find.h
 *
 *  Created on: Mar 3, 2013
 *      Author: sancho
 */

#ifndef FIND_H_
#define FIND_H_

GList *getCompletionsFind(context_t *context, const char *line);
gboolean validateArgsFind(GList *args, context_t *context);
gboolean cmdFind(context_t *context, GList *args);
extern char *findAliases[];

#endif /* FIND_H_ */
