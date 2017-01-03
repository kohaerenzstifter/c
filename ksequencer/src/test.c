#include "ksequencer.h"

DECLARE_LOCKCONTEXT

void test(void)
{
	err_t err;
	err_t *e = &err;

	initErr(e);

	pattern_t *pattern = allocatePattern((pattern_t *) patterns.root);

	PARENT(pattern) = (pattern_t *) patterns.root;
	NAME(pattern) = strdup("test");
	NR_BARS(pattern) = 1;
	NR_USERSTEPS_PER_BAR(pattern) = 256;
	TYPE(pattern) = patternTypeNote;
	CHANNEL(pattern) = 0;
	CHILDREN(patterns.root) =
	  g_slist_append((GSList *) CHILDREN(patterns.root), pattern);

	noteValue_t *noteValue = allocateNoteValue();
	noteValue->name = strdup("1");
	noteValue->tone = 'c';
	noteValue->octave = 0;
	setAlsaNoteEvent(noteValue, 0, &lockContext, FALSE,  NULL);
	VALUES(pattern) =
		  g_slist_append(VALUES(pattern), noteValue);

	noteValue = allocateNoteValue();
	noteValue->name = strdup("1dssd");
	noteValue->tone = 'f';
	noteValue->octave = 0;
	setAlsaNoteEvent(noteValue, 0, &lockContext, FALSE,  NULL);
	VALUES(pattern) =
		  g_slist_append(VALUES(pattern), noteValue);

	noteValue = allocateNoteValue();
	noteValue->name = strdup("3k");
	noteValue->tone = 'a';
	noteValue->octave = 0;
	setAlsaNoteEvent(noteValue, 0, &lockContext, FALSE,  NULL);
	VALUES(pattern) =
		  g_slist_append(VALUES(pattern), noteValue);

	terror(adjustSteps(pattern, NR_BARS(pattern),
	  NR_USERSTEPS_PER_BAR(pattern), &lockContext, FALSE, e))

	uint32_t pc = 0;
	for (uint32_t i = 0; i < 100000; i++) {
		if ((i / 100000) > pc) {
			pc++;
			printf("pc: %u\n", pc);
		}
		randomise(pattern, 0, &lockContext);
	}
finish:
	return;
}
