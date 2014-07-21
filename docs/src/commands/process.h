/*
 * process.h
 *
 *  Created on: Mar 3, 2013
 *      Author: sancho
 */

#ifndef PROCESS_H_
#define PROCESS_H_

GList *getCompletionsProcess(context_t *context, const char *line);
gboolean validateArgsProcess(GList *args, context_t *context);
gboolean cmdProcess(context_t *context, GList *args);
extern char *processAliases[];

#endif /* PROCESS_H_ */
