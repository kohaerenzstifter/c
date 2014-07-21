/*
 * submit.h
 *
 *  Created on: Mar 3, 2013
 *      Author: sancho
 */

#ifndef SUBMIT_H_
#define SUBMIT_H_


gboolean cmdSubmit(context_t *context, GList *args);
GList *getCompletionsSubmit(context_t *context, const char *line);
gboolean validateArgsSubmit(GList *args, context_t *context);

extern char *submitAliases[];

#endif /* SUBMIT_H_ */
