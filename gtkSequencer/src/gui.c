#include "gui.h"

static void unsoundNoteEvent(noteEvent_t *off)
{
	if (off->off.noteOffLink == NULL) {
		goto finish;
	}

	off->noteValue->snd_seq_event->type = SND_SEQ_EVENT_NOTEOFF;
	snd_seq_event_output(sequencer.value, (snd_seq_event_t *) off->noteValue->snd_seq_event);
	snd_seq_drain_output(sequencer.value);
	notesOff.value =
	  g_slist_delete_link((GSList *) notesOff.value, (GSList *) off->off.noteOffLink);
	off->off.noteOffLink = NULL;

finish:
	return;
}

void unsoundPattern(pattern_t *pattern)
{
	uint32_t eventStepsPerBar;

	if (pattern->real.type != PATTERNTYPE_NOTE) {
		goto finish;
	}

	eventStepsPerBar = pattern->real.userStepsPerBar * EVENTSTEPS_PER_USERSTEP(pattern);

	for (uint32_t i = 0; i < pattern->real.bars; i++) {
		for (uint32_t j = 0; j < eventStepsPerBar; j++) {
			uint32_t idx = (i * eventStepsPerBar) + j;

			if (pattern->real.note.steps.event[idx].onNoteEvent == NULL) {
				continue;
			}
			unsoundNoteEvent((noteEvent_t *)
			  pattern->real.note.steps.event[idx].onNoteEvent->on.offNoteEvent);
		}
	}

finish:
	return;
}

void setSteps(pattern_t *pattern)
{
	void **userSteps = ADDR_USER_STEPS(pattern);
	void **eventSteps = ADDR_EVENT_STEPS(pattern);
	uint32_t nrEventSteps = NUMBER_EVENTSTEPS(pattern);
	uint32_t szEventStep = SIZE_EVENTSTEP(pattern);

	*userSteps = calloc(NUMBER_USERSTEPS(pattern), SIZE_USERSTEP(pattern));
	if (eventSteps != NULL) {
		*eventSteps = calloc(nrEventSteps, szEventStep);
	}
}

void setDummyStep(dummyUserStep_t *step, gboolean value, uint32_t lockCount)
{
	LOCK();

	step->set = value;

	UNLOCK();

}

void setControllerStep(pattern_t *pattern,
  controllerUserStep_t *step, GSList *value, uint32_t lockCount)
{
	controllerValue_t *controllerValue = NULL;
	uint32_t idx = step - pattern->real.controller.steps.user;

	LOCK();

	step->value = value;

	if (value == NULL) {
		step->event = NULL;
		goto finish;
	}

	controllerValue = value->data;
	step->event =
	  (volatile snd_seq_event_t **) &(pattern->real.controller.steps.event[idx].value);
	*step->event = controllerValue->event;

finish:
	UNLOCK();
}

static void redirect(pattern_t *pattern, uint32_t startIdx, noteEvent_t *cmp, noteEvent_t *set)
{
	uint32_t numberSteps =
	  (pattern->real.userStepsPerBar * pattern->real.bars);

	for (uint32_t i = startIdx; i < numberSteps; i++) {
		if (pattern->real.note.steps.user[i].onNoteEvent != cmp) {
			break;
		}
		pattern->real.note.steps.user[i].onNoteEvent = set;
	}
}

static void freeNoteEvent(noteEvent_t *noteEvent)
{
	if (noteEvent == pendingOff) {
		pendingOff = NULL;
	}
	free(noteEvent);
}

static void unsetNoteStep(pattern_t *pattern, noteUserStep_t *step)
{
	uint32_t userIdx = step - pattern->real.note.steps.user;
	uint32_t eventIdx = userIdx * NOTEEVENTSTEPS_PER_USERSTEP;
	noteUserStep_t *previous = (userIdx < 1) ? NULL :
	  (noteUserStep_t *) &pattern->real.note.steps.user[userIdx - 1];
	noteUserStep_t *next =
	  (userIdx >= ((pattern->real.userStepsPerBar * pattern->real.bars) - 1))
	  ? NULL : (noteUserStep_t *) &pattern->real.note.steps.user[userIdx + 1];

	void moveStepOnToNext() {
		noteEventStep_t *isNoteOnEventStep =
		  (noteEventStep_t *) step->onNoteEvent->noteEventStep;
		noteEventStep_t *mustNoteOnEventStep =
		  isNoteOnEventStep + NOTEEVENTSTEPS_PER_USERSTEP;

		step->onNoteEvent->noteEventStep = mustNoteOnEventStep;

		mustNoteOnEventStep->onNoteEvent = isNoteOnEventStep->onNoteEvent;
		isNoteOnEventStep->onNoteEvent = NULL;
	}

	void removeNote() {
		step->onNoteEvent->on.offNoteEvent->noteEventStep->offNoteEvent = NULL;
		freeNoteEvent((noteEvent_t *) step->onNoteEvent->on.offNoteEvent);
		step->onNoteEvent->noteEventStep->onNoteEvent = NULL;
		freeNoteEvent((noteEvent_t *) step->onNoteEvent);
	}

	void previousNoteOff() {
		noteEvent_t *noteEvent = (noteEvent_t *) previous->onNoteEvent->on.offNoteEvent;

		step->onNoteEvent->on.offNoteEvent->noteEventStep->offNoteEvent = NULL;

		noteEvent->noteEventStep =
		  &(pattern->real.note.steps.event[eventIdx]);
		noteEvent->noteEventStep->offNoteEvent = noteEvent;
	}

	void nextNoteOn(noteEventStep_t *offStep, noteValue_t *noteValue) {
		noteEvent_t *onNoteEvent = NULL;
		noteEvent_t *noteOffEvent = NULL;

		noteEventStep_t *eventStep =
		  (noteEventStep_t *) &(pattern->real.note.steps.event[eventIdx]);
		noteEventStep_t *mustStep = eventStep + NOTEEVENTSTEPS_PER_USERSTEP;

		onNoteEvent = calloc(1, sizeof(noteEvent_t));
		onNoteEvent->noteValue = noteValue;
		onNoteEvent->noteEventStep = mustStep;

		noteOffEvent = calloc(1, sizeof(noteEvent_t));
		noteOffEvent->noteValue = noteValue;
		noteOffEvent->noteEventStep = offStep;

		onNoteEvent->on.offNoteEvent = noteOffEvent;

		redirect(pattern, (userIdx + 1), (noteEvent_t *) next->onNoteEvent, (noteEvent_t *) onNoteEvent);

		mustStep->onNoteEvent = onNoteEvent;
		offStep->offNoteEvent = noteOffEvent;
		eventStep->onNoteEvent = NULL;

	}

	if (step->onNoteEvent == NULL) {
		goto finish;
	}

	if ((previous == NULL)||(step->onNoteEvent != previous->onNoteEvent)) {
		if ((next != NULL)&&(step->onNoteEvent == next->onNoteEvent)) {
			moveStepOnToNext();
		} else {
			removeNote();
		}
	} else {
		noteEventStep_t *offStep = (noteEventStep_t *) previous->onNoteEvent->on.offNoteEvent->noteEventStep;
		previousNoteOff();
		if ((next != NULL)&&(step->onNoteEvent == next->onNoteEvent)) {
			nextNoteOn(offStep, step->value->data);
		}
	}


finish:
	step->onNoteEvent = NULL;
}

static void doSetNoteStep(pattern_t *pattern, noteUserStep_t *noteUserStep)
{
	noteEvent_t *onNoteEvent = NULL;
	noteEvent_t *offNoteEvent = NULL;
	noteValue_t *noteValue = noteUserStep->value->data;
	uint32_t userIdx = noteUserStep - pattern->real.note.steps.user;
	uint32_t eventIdx = userIdx * NOTEEVENTSTEPS_PER_USERSTEP;
	noteUserStep_t *previous = (userIdx < 1) ? NULL :
	  (noteUserStep_t *) &pattern->real.note.steps.user[userIdx - 1];
	noteUserStep_t *next =
	  (userIdx >= (pattern->real.userStepsPerBar - 1)) ? NULL :
	  (noteUserStep_t *) &pattern->real.note.steps.user[userIdx + 1];

	if ((previous != NULL)&&(previous->slide)&&
	  (previous->value != NULL)) {
		noteValue_t *previousNoteValue = previous->value->data;
		if ((noteValue->tone == previousNoteValue->tone)&&
		  (noteValue->sharp == previousNoteValue->sharp)&&
		  (noteValue->octave == previousNoteValue->octave)) {
			onNoteEvent = (noteEvent_t *) previous->onNoteEvent;
			onNoteEvent->on.offNoteEvent->noteEventStep->offNoteEvent = NULL;
			freeNoteEvent((noteEvent_t *) onNoteEvent->on.offNoteEvent);
		}
	}

	if (onNoteEvent == NULL) {
		onNoteEvent = calloc(1, sizeof(noteEvent_t));
		onNoteEvent->noteEventStep =
		  &(pattern->real.note.steps.event[eventIdx]);
		onNoteEvent->noteEventStep->onNoteEvent = onNoteEvent;
		onNoteEvent->noteValue = noteUserStep->value->data;
	}

	if ((next != NULL)&&(noteUserStep->slide)&&
	  (next->value != NULL)) {
		noteValue_t *nextNoteValue = next->value->data;
		if ((noteValue->tone == nextNoteValue->tone)&&
		  (noteValue->sharp == nextNoteValue->sharp)&&
		  (noteValue->octave == nextNoteValue->octave)) {
			offNoteEvent = (noteEvent_t *) next->onNoteEvent->on.offNoteEvent;
			next->onNoteEvent->noteEventStep->onNoteEvent = NULL;
			freeNoteEvent((noteEvent_t *) next->onNoteEvent);
			redirect(pattern, (userIdx + 1), (noteEvent_t *) next->onNoteEvent, (noteEvent_t *) onNoteEvent);
		}
	}
	if (offNoteEvent == NULL) {
		offNoteEvent = calloc(1, sizeof(noteEvent_t));
		eventIdx += NOTEEVENTSTEPS_PER_USERSTEP;
		if (!noteUserStep->slide) {
			eventIdx--;
		}
		offNoteEvent->noteEventStep =
		  &(pattern->real.note.steps.event[eventIdx]);
		offNoteEvent->noteEventStep->offNoteEvent = offNoteEvent;
		offNoteEvent->noteValue = noteUserStep->value->data;
	}

	noteUserStep->onNoteEvent = onNoteEvent;
	onNoteEvent->on.offNoteEvent = offNoteEvent;
}

void setNoteStep(pattern_t *pattern, noteUserStep_t *step, GSList *value, uint32_t lockCount)
{
	gboolean slide = step->slide;

	LOCK();

	unsoundPattern(pattern);

	unsetNoteStep(pattern, step);

	step->value = value;
	step->slide = (value != NULL) ? slide : FALSE;

	if (step->value == NULL) {
		goto finish;
	}

	doSetNoteStep(pattern, step);

finish:
	UNLOCK();
}

void setNoteSlide(pattern_t *pattern, noteUserStep_t *step, gboolean slide, uint32_t lockCount)
{
	GSList *value = (GSList *) step->value;

	LOCK();

	unsoundPattern(pattern);

	unsetNoteStep(pattern, step);

	step->value = value;
	step->slide = slide;

	if (step->value == NULL) {
		goto finish;
	}

	doSetNoteStep(pattern, step);

finish:
	UNLOCK();
}

void adjustSteps(pattern_t *pattern, uint32_t bars, uint32_t userStepsPerBar,
  uint32_t lockCount)
{
	uint32_t haveBars = pattern->real.bars;
	uint32_t haveUserStepsPerBar = pattern->real.userStepsPerBar;
	uint32_t stepDivisor = (userStepsPerBar <= haveUserStepsPerBar) ?
	  1 : (userStepsPerBar / haveUserStepsPerBar);
	uint32_t stepMultiplier = (userStepsPerBar >= haveUserStepsPerBar) ?
	  1 : (haveUserStepsPerBar / userStepsPerBar);
	void *userSteps = *(ADDR_USER_STEPS(pattern));
	void **foo = ADDR_EVENT_STEPS(pattern);
	void *eventSteps = NULL;

	if (foo != NULL) {
		eventSteps = *(foo);
	}

	LOCK();

	unsoundPattern(pattern);

	pattern->real.bars = bars;
	pattern->real.userStepsPerBar = userStepsPerBar;

	setSteps(pattern);

	for (uint32_t i = 0; i < bars; i++) {
		for (uint32_t j = 0; j < userStepsPerBar; j += stepDivisor) {
			uint32_t targetStep = (i * userStepsPerBar) + j;
			uint32_t sourceBar = i % haveBars;
			uint32_t sourceStep = j / stepDivisor;
			sourceStep *= stepMultiplier;
			sourceStep += (sourceBar * haveUserStepsPerBar);
			SET_STEP_FROM_STEP(pattern,
			  USERSTEP(pattern, userSteps, sourceStep),
			  USERSTEP2(pattern, targetStep));
		}
	}
	UNLOCK();

	if ((userSteps) != NULL) {
		free((userSteps));
	}
	if ((eventSteps) != NULL) {
		free((eventSteps));
	}
}

snd_seq_event_t *getAlsaEvent(void)
{
	snd_seq_event_t *result = calloc(1, sizeof(snd_seq_event_t));

	snd_seq_ev_clear(result);
	snd_seq_ev_set_direct(result);
	result->dest = port;

	return result;
}

void setAlsaNoteEvent(uint8_t channel, noteValue_t *noteValue)
{
	int32_t note2Int(noteValue_t *noteValue) {
		int32_t result = 0;
		struct {
			char tone;
			int32_t value;
		} values[] = {
			{'c', 24},
			{'d', 26},
			{'e', 28},
			{'f', 29},
			{'g', 31},
			{'a', 33},
			{'b', 35}
		};
	
		for (uint32_t i = 0; i < sizeof(values)/sizeof(values[0]); i++) {
			if (values[i].tone == noteValue->tone) {
				result = values[i].value;
				break;
			}
		}
	
		if (noteValue->sharp) {
			result++;
		}
	
		result += 12 * noteValue->octave;
		
		return result;
	}

	int32_t note = note2Int(noteValue);

	snd_seq_ev_clear((snd_seq_event_t *) noteValue->snd_seq_event);
	snd_seq_ev_set_direct(noteValue->snd_seq_event);
	snd_seq_ev_set_noteon(noteValue->snd_seq_event, (channel - 1), note, 127);
	noteValue->snd_seq_event->dest = port;
}

void setupNoteValue(pattern_t  *pattern, const char *name, gboolean sharp,
  int8_t octave, char tone, noteValue_t *noteValue)
{
	char namebuffer[50];

	if (noteValue == NULL) {
		noteValue = calloc(1, sizeof(noteValue_t));
		pattern->real.note.values =
		  g_slist_append((GSList *) pattern->real.note.values, noteValue);
#define NAMEFORMAT "%c%s %d"
		if (name[0] == '\0') {
			snprintf(namebuffer, sizeof(namebuffer), NAMEFORMAT, tone, sharp ? "#" : "", octave);
			name = namebuffer;
		}
		noteValue->snd_seq_event = getAlsaEvent();
	} else {
		free((noteEvent_t *) noteValue->name);
	}

	noteValue->tone = tone;
	noteValue->name = strdup(name);
	noteValue->octave = octave;
	noteValue->sharp = sharp;

	setAlsaNoteEvent(pattern->real.channel, noteValue);
}

pattern_t *allocatePattern(pattern_t *parent)
{
	pattern_t *result = calloc(1, sizeof(pattern_t));

	result->isRoot = (parent == NULL);
	result->children = NULL;
	result->real.parent = parent;

	return result;
}

pattern_t *createRealPattern2(pattern_t *parent, const gchar *name, gint channel,
  patternType_t patternType, gint controller)
{
	pattern_t *result = allocatePattern(parent);

	result->real.name = strdup(name);
	result->real.channel = channel;

	result->real.bars =
	  parent->isRoot ? 1 : parent->real.bars;
	result->real.userStepsPerBar =
	  parent->isRoot ? 1 : parent->real.userStepsPerBar;
	PATTERN_TYPE(result) = patternType;

	if (patternType == PATTERNTYPE_CONTROLLER) {
		result->real.controller.parameter = controller;
	}

	return result;
}