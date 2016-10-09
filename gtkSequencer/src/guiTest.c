#include "gui.h"

//#define PRINT

#ifdef PRINT
#define myprintf(fmt, ...) \
	{ \
			printf(fmt, ##__VA_ARGS__); \
	}
#else
#define myprintf(fmt, ...)
#endif

static void stepTest(pattern_t *pattern, noteUserStep_t *noteUserStep, gboolean maySlide)
{
	GSList *value = NULL;
	uint32_t position = 0;
	uint32_t nrValues = g_slist_length((GSList *) pattern->real.note.values) + 1;

	if ((rand() % 2) != 0) {
		myprintf(" => not setting step\n");
		goto finish;
	}

	do {
		position = (rand() % nrValues);
		value = g_slist_nth((GSList *) pattern->real.note.values , position);
	} while (noteUserStep->value == value);

	myprintf("setting step from %p to %p\n", noteUserStep->value, value);

	setNoteStep(pattern, noteUserStep, value, 0);
	sleep(1);

	if (!maySlide) {
		myprintf("slide not allowed for last note\n");
		goto finish;
	}

	if (noteUserStep->value == NULL) {
		myprintf("cannot set slide of unset note step!\n");
		goto finish;
	}

	if ((rand() % 2) != 0) {
		myprintf("won't change slide!\n");
		goto finish;
	}

	myprintf("changing slide from %s to %s\n",
	  noteUserStep->slide ? "TRUE" : "FALSE",
	  noteUserStep->slide ? "FALSE" : "TRUE");
	setNoteSlide(pattern, noteUserStep, !(noteUserStep->slide), 0);
	sleep(1);

finish:
	return;
}

void guiTest(uint32_t iterations)
{

	uint32_t i = 0;
	uint32_t j = 0;

	pattern_t *pattern =
	  createRealPattern2(&rootPattern, "test", 0, PATTERNTYPE_NOTE, 0);
	rootPattern.children =
	  g_slist_append((GSList *) rootPattern.children, pattern);
	setSteps(pattern);
	setupNoteValue(pattern, "a", FALSE, 1, 'c', NULL);
	setupNoteValue(pattern, "b", FALSE, 1, 'e', NULL);
	setupNoteValue(pattern, "c", FALSE, 1, 'g', NULL);
	setupNoteValue(pattern, "d", FALSE, 1, 'b', NULL);

	adjustSteps(pattern, 2, 16, 0);

	uint32_t lastJ = ((pattern->real.userStepsPerBar * pattern->real.bars) - 1);
	for (i = 0; i < iterations; i++) {
		myprintf("iteration: %u\n", i);
		for (j = 0; j <= lastJ; j++) {
			myprintf("step %u ...", j);
			stepTest(pattern, (noteUserStep_t *) &(pattern->real.note.steps.user[j]), (j < lastJ));
		}
	}

	adjustSteps(pattern, 2, 16, 0);
}