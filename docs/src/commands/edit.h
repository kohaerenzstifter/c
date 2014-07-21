/*
 * edit.h
 *
 *  Created on: Mar 3, 2013
 *      Author: sancho
 */

#ifndef EDIT_H_
#define EDIT_H_

void doEditDocument(context_t *context, char *docName);
gboolean cmdEdit(context_t *context, GList *args);
GList *getCompletionsEdit(context_t *context, const char *line);
gboolean validateArgsEdit(GList *args, context_t *context);
extern char *editAliases[];

#endif /* EDIT_H_ */
