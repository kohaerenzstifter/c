#include "kVstSequencer.h"

#include "PluginProcessor.h"

static void lockMutex(mutex_t *mutex, err_t *e)
{
	MARK();

	defineError();

	terror(failIfFalse(mutex->initialised))
	terror(failIfFalse((pthread_mutex_lock((&(mutex->value))) == 0)))

finish:
	return;
}

static void unlockMutex(mutex_t *mutex, err_t *e)
{
	MARK();

	defineError();

	terror(failIfFalse(mutex->initialised))
	pthread_mutex_unlock((&(mutex->value)));

finish:
	return;
}

static void lockCounted(lockContext_t *lockContext, uint32_t mutex, err_t *e)
{
	MARK();

	defineError();

	if (lockContext == NULL) {
		goto finish;
	}

	if (lockContext->mutexes[mutex] < 1)  {
		for (uint32_t i = 0; i < mutex; i++) {
			terror(failIfFalse((lockContext->mutexes[i] <=
			  lockContext->mutexes[mutex])))
		}
		if (lockContext->mutexes[mutex] < 1) {
			terror(lockMutex(&mutexes.value[mutex], e))
		}
	}
	lockContext->mutexes[mutex]++;

finish:
	return;
}

static void unlockCounted(lockContext_t *lockContext, uint32_t mutex, err_t *e)
{
	MARK();

	defineError();

	if (lockContext == NULL) {
		goto finish;
	}

	terror(failIfFalse((lockContext->mutexes[mutex] >  0)))
	if (lockContext->mutexes[mutex] < 2) {
		terror(unlockMutex(&mutexes.value[mutex], e))
	}
	lockContext->mutexes[mutex]--;

finish:
	return;
}

void getLocks(lockContext_t *lockContext, uint32_t locks, err_t *e)
{
	MARK();

	for (int32_t i = (NR_MUTEXES - 1),
	  mask = (1 << i); i >= 0; i--, mask >>= 1)  {
		if (!(locks & mask)) {
			continue;
		}
		terror(lockCounted(lockContext, i, e))
	}

finish:
if (hasFailed(e)) abort();
	return;
}

void releaseLocks(lockContext_t *lockContext, uint32_t locks, err_t *e)
{
	MARK();

	defineError();

	for (uint32_t i = 0, mask = 1; i < NR_MUTEXES; i++, mask <<= 1)  {
		if (!(locks & mask)) {
			continue;
		}
		terror(unlockCounted(lockContext, i, e))
	}

finish:
	return;
}

static void requireLocks(lockContext_t *lockContext,
  uint32_t required, err_t *e)
{
	MARK();

	for (uint32_t i = 0, mask = 1; i < NR_MUTEXES; i++, mask <<= 1)  {
		if (!(required & mask)) {
			continue;
		}
		terror(failIfFalse((lockContext->mutexes[i] > 0)))
	}

finish:
	return;
}


static gboolean anyStepSetForChild(pattern_t *parent, uint32_t idx,
  pattern_t *child)
{
	MARK();

	gboolean result = FALSE;
	uint32_t barsFactor = (NR_BARS(child) / NR_BARS(parent));
	uint32_t stepsPerBarFactor =
	  (NR_USERSTEPS_PER_BAR(child) / NR_USERSTEPS_PER_BAR(parent));
	uint32_t nrParentSteps = NR_USERSTEPS(parent);

	for (uint32_t i = 0; i < barsFactor; i++) {
		for (uint32_t j = 0; j < stepsPerBarFactor; j++) {
			uint32_t childIdx =
			  ((idx * stepsPerBarFactor) + (i * stepsPerBarFactor *
			  nrParentSteps) + j);
			if ((result = IS_SET(USERSTEP_AT(child, childIdx), TYPE(child)))) {
				break;
			}
		}
	}

	return result;
}

void fireMidiMessage(lockContext_t *lockContext,
  midiMessage_t *midiMessage, err_t *e)
{
	MARK();

	defineError();

	terror(requireLocks(lockContext, (LOCK_SEQUENCER), e))

	midiMessages = g_slist_prepend((GSList *) midiMessages, midiMessage);

finish:
	return;
}

static midiMessage_t *getMidiMessage(midiMessageType_t midiMessageType)
{
	MARK();

	midiMessage_t *result = (midiMessage_t *) calloc(1, sizeof(midiMessage_t));
	result->midiMessageType = midiMessageType;

	return result;
}

midiMessage_t *getControllerMidiMessage(uint8_t parameter, int8_t value)
{
	MARK();

	midiMessage_t *result = getMidiMessage(midiMessageTypeController);
	result->controller.parameter = parameter;
	result->controller.value = value;

	return result;
}

midiMessage_t *getNoteOffMidiMessage(noteEvent_t *noteEvent)
{
	MARK();

	midiMessage_t *result = getMidiMessage(midiMessageTypeNoteOff);
	result->noteOff.noteNumber = MIDI_PITCH(noteEvent);

	return result;
}

static void unsoundNoteEvent(lockContext_t *lockContext,
  noteEvent_t *offNoteEvent, err_t *e)
{
	MARK();

	midiMessage_t *midiMessage = NULL;
	terror(requireLocks(lockContext, (LOCK_DATA), e))

	if (offNoteEvent->off.noteOffLink == NULL) {
		goto finish;
	}

	midiMessage = getNoteOffMidiMessage(offNoteEvent);
	terror(fireMidiMessage(lockContext, midiMessage, e))
	midiMessage = NULL;
	notesOff.value =
	  g_slist_delete_link((GSList *) notesOff.value,
	  (GSList *) offNoteEvent->off.noteOffLink);
	offNoteEvent->off.noteOffLink = NULL;

finish:
	free(midiMessage);
}

void unsoundPattern(lockContext_t *lockContext,
  pattern_t *pattern, err_t *e)
{
	MARK();

	defineError();

	terror(failIfFalse((TYPE(pattern) == patternTypeNote)))

	terror(requireLocks(lockContext, (LOCK_DATA | LOCK_SEQUENCER), e))

	for (uint32_t i = 0; i < NR_EVENTSTEPS(pattern); i++) {
		noteEventStep_t *noteEventStep =
		  (noteEventStep_t *) EVENTSTEP_AT(pattern, i);
		if (noteEventStep->onNoteEvent == NULL) {
			continue;
		}
		terror(unsoundNoteEvent(lockContext, (noteEvent_t *)
		  noteEventStep->onNoteEvent->on.offNoteEvent, e))
	}

finish:
	return;
}

static void redirect(pattern_t *pattern, uint32_t startIdx,
  noteEvent_t *find, noteEvent_t *replace)
{
	MARK();

	uint32_t numberSteps = NR_USERSTEPS(pattern);

	for (uint32_t i = startIdx; i < numberSteps; i++) {
		noteUserStep_t *noteUserStep =
		  (noteUserStep_t *) USERSTEP_AT(pattern, i);

		if (noteUserStep->onNoteEvent != find) {
			break;
		}
		noteUserStep->onNoteEvent = replace;
	}
}

void moveStepOnToNext(pattern_t *pattern, noteUserStep_t *noteUserStep)
{
	MARK();

	noteEventStep_t *isNoteOnEventStep =
	  (noteEventStep_t *) noteUserStep->onNoteEvent->noteEventStep;
	noteEventStep_t *mustNoteOnEventStep =
	  isNoteOnEventStep + EVENTSTEPS_PER_USERSTEP(TYPE((pattern)));

	noteUserStep->onNoteEvent->noteEventStep = mustNoteOnEventStep;

	mustNoteOnEventStep->onNoteEvent = isNoteOnEventStep->onNoteEvent;
	isNoteOnEventStep->onNoteEvent = NULL;
}

void removeNote(noteUserStep_t *noteUserStep)
{
	MARK();

	noteUserStep->onNoteEvent->on.offNoteEvent->noteEventStep->offNoteEvent
	  = NULL;
	free((noteEvent_t *) noteUserStep->onNoteEvent->on.offNoteEvent);
	noteUserStep->onNoteEvent->noteEventStep->onNoteEvent = NULL;
	free(noteUserStep->onNoteEvent);
}

void previousNoteOff(pattern_t *pattern,
  noteUserStep_t *noteUserStep, noteUserStep_t *previous, uint32_t eventStepIdx)
{
	MARK();

	noteEvent_t *noteEvent =
	  (noteEvent_t *) previous->onNoteEvent->on.offNoteEvent;

	noteUserStep->onNoteEvent->on.offNoteEvent->noteEventStep->offNoteEvent
	  = NULL;

	noteEvent->noteEventStep =
	  (noteEventStep_t *) EVENTSTEP_AT(pattern, eventStepIdx);
	noteEvent->noteEventStep->offNoteEvent = noteEvent;
}

void nextNoteOn(pattern_t *pattern, noteEventStep_t *offNoteEventStep,
  noteUserStep_t *next, noteValue_t *noteValue, uint32_t userStepIdx,
  uint32_t eventStepIdx)
{
	MARK();

	noteEvent_t *onNoteEvent = NULL;
	noteEvent_t *noteOffEvent = NULL;

	noteEventStep_t *eventStep =
	  (noteEventStep_t *) EVENTSTEP_AT(pattern, eventStepIdx);
	noteEventStep_t *mustStep =
	  eventStep + EVENTSTEPS_PER_USERSTEP(TYPE((pattern)));

	onNoteEvent = (noteEvent_t *) calloc(1, sizeof(noteEvent_t));
	onNoteEvent->noteValue = noteValue;
	onNoteEvent->noteEventStep = mustStep;

	noteOffEvent = (noteEvent_t *) calloc(1, sizeof(noteEvent_t));
	noteOffEvent->noteValue = noteValue;
	noteOffEvent->noteEventStep = offNoteEventStep;

	onNoteEvent->on.offNoteEvent = noteOffEvent;

	redirect(pattern, (userStepIdx + 1), next->onNoteEvent, onNoteEvent);

	mustStep->onNoteEvent = onNoteEvent;
	offNoteEventStep->offNoteEvent = noteOffEvent;
	eventStep->onNoteEvent = NULL;
}

static void unsetNoteStep(pattern_t *pattern,
  noteUserStep_t *noteUserStep, uint32_t userStepIdx)
{
	MARK();

	noteUserStep_t *previous = (userStepIdx < 1) ? NULL :
	  (noteUserStep_t *) USERSTEP_AT(pattern, (userStepIdx - 1));
	noteUserStep_t *next =
	  (userStepIdx >= (NR_USERSTEPS(pattern) - 1)) ? NULL :
	  (noteUserStep_t *) USERSTEP_AT(pattern, (userStepIdx + 1));

	uint32_t eventStepIdx =
	  userStepIdx * EVENTSTEPS_PER_USERSTEP(TYPE((pattern)));

	if (noteUserStep->onNoteEvent == NULL) {
		goto finish;
	}

	if ((previous == NULL)||(noteUserStep->onNoteEvent
	  != previous->onNoteEvent)) {
		if ((next != NULL)&&(noteUserStep->onNoteEvent == next->onNoteEvent)) {
			moveStepOnToNext(pattern, noteUserStep);
		} else {
			removeNote(noteUserStep);
		}
	} else {
		noteEventStep_t *offNoteEventStep =
		  (noteEventStep_t *)
		  previous->onNoteEvent->on.offNoteEvent->noteEventStep;
		previousNoteOff(pattern, noteUserStep, previous, eventStepIdx);
		if ((next != NULL)&&(noteUserStep->onNoteEvent == next->onNoteEvent)) {
			nextNoteOn(pattern, offNoteEventStep, next,
			  (noteValue_t *) noteUserStep->value->data, userStepIdx,
			  eventStepIdx);
		}
	}

finish:
	noteUserStep->onNoteEvent = NULL;
}

static void doSetNoteStep(pattern_t *pattern,
  noteUserStep_t *noteUserStep, uint32_t userStepIdx)
{
	MARK();

	noteEvent_t *onNoteEvent = NULL;
	noteEvent_t *offNoteEvent = NULL;
	noteValue_t *noteValue = (noteValue_t *) noteUserStep->value->data;
	uint32_t eventStepIdx =
	  userStepIdx * EVENTSTEPS_PER_USERSTEP(TYPE((pattern)));
	noteUserStep_t *previous = (userStepIdx < 1) ? NULL :
	  (noteUserStep_t *) USERSTEP_AT(pattern, (userStepIdx - 1));
	noteUserStep_t *next =
	  (userStepIdx >= (NR_USERSTEPS(pattern) - 1)) ? NULL :
	  (noteUserStep_t *) USERSTEP_AT(pattern, (userStepIdx + 1));
	if ((previous != NULL)&&(previous->slide)&&
	  (previous->value != NULL)) {
		noteValue_t *previousNoteValue = (noteValue_t *) previous->value->data;
		if ((noteValue->note == previousNoteValue->note)&&
		  (noteValue->sharp == previousNoteValue->sharp)&&
		  (noteValue->octave == previousNoteValue->octave)) {
			onNoteEvent = previous->onNoteEvent;
			onNoteEvent->on.offNoteEvent->noteEventStep->offNoteEvent = NULL;
			free((noteEvent_t *) onNoteEvent->on.offNoteEvent);
		}
	}
	if (onNoteEvent == NULL) {
		onNoteEvent = (noteEvent_t *) calloc(1, sizeof(noteEvent_t));
		onNoteEvent->noteEventStep =
		  (noteEventStep_t *) EVENTSTEP_AT(pattern, (eventStepIdx));
		onNoteEvent->noteEventStep->onNoteEvent = onNoteEvent;
		onNoteEvent->noteValue = (noteValue_t *) noteUserStep->value->data;
	}
	if ((next != NULL)&&(noteUserStep->slide)&&
	  (next->value != NULL)) {
		noteValue_t *nextNoteValue = (noteValue_t *) next->value->data;
		if ((noteValue->note == nextNoteValue->note)&&
		  (noteValue->sharp == nextNoteValue->sharp)&&
		  (noteValue->octave == nextNoteValue->octave)) {
			offNoteEvent = (noteEvent_t *) next->onNoteEvent->on.offNoteEvent;
			next->onNoteEvent->noteEventStep->onNoteEvent = NULL;
			free(next->onNoteEvent);
			redirect(pattern,
			  (userStepIdx + 1), next->onNoteEvent, onNoteEvent);
		}
	}
	if (offNoteEvent == NULL) {
		offNoteEvent = (noteEvent_t *) calloc(1, sizeof(noteEvent_t));
		eventStepIdx += EVENTSTEPS_PER_USERSTEP(TYPE((pattern)));
		if (!noteUserStep->slide) {
			eventStepIdx--;
		}
		offNoteEvent->noteEventStep =
		  (noteEventStep_t *) EVENTSTEP_AT(pattern, (eventStepIdx));
		offNoteEvent->noteEventStep->offNoteEvent = offNoteEvent;
		offNoteEvent->noteValue = (noteValue_t *) noteUserStep->value->data;
	}
	noteUserStep->onNoteEvent = onNoteEvent;
	onNoteEvent->on.velocity =
	  ((controllerValue_t *) (noteUserStep->velocity->data))->value;
	onNoteEvent->on.offNoteEvent = offNoteEvent;
}

static void doSetControllerStep(pattern_t *pattern,
  controllerUserStep_t *controllerUserStep, uint32_t idx)
{
	MARK();

	controllerValue_t *controllerValue = (controllerUserStep->value == NULL) ?
	  NULL : (controllerValue_t *) controllerUserStep->value->data;
	controllerEventStep_t *controllerEventStep =
	  (controllerEventStep_t *) EVENTSTEP_AT(pattern, (idx *
	  EVENTSTEPS_PER_USERSTEP((pattern->patternType))));

	controllerEventStep->controllerValue = controllerValue;
}

gboolean anyChildStepSet(pattern_t *pattern, uint32_t idx)
{
	MARK();

	gboolean result = FALSE;

	for (GSList *cur = (GSList *) pattern->children; cur != NULL;
	  cur = g_slist_next(cur)) {
		if ((result = anyStepSetForChild(pattern, idx,
		  (pattern_t *) cur->data))) {
			break;
		}
	}

	return result;
}

void lockUserStep(pattern_t *pattern, uint32_t idx)
{
	MARK();

	void *step = USERSTEP_AT(pattern, idx);

	LOCKED(step, pattern->patternType) = !LOCKED(step, pattern->patternType);
}

void lockSlide(pattern_t *pattern, uint32_t idx)
{
	MARK();

	noteUserStep_t *noteUserStep =
	  (noteUserStep_t *) (USERSTEP_AT(pattern, idx));

	noteUserStep->slideLocked = !noteUserStep->slideLocked;
}

void setDummyStep(pattern_t *pattern, dummyUserStep_t *dummyUserStep,
  gboolean set, lockContext_t *lockContext, err_t *e)
{
	MARK();

	defineError();

	gboolean locked = FALSE;
	uint32_t locks = LOCK_DATA;

	terror(getLocks(lockContext, locks, e))
	locked = TRUE;
	dummyUserStep->set = set;

finish:
	if (locked) {
		releaseLocks(lockContext, locks, NULL);
	}
}

void setControllerStep(pattern_t *pattern,
  controllerUserStep_t *controllerUserStep, GSList *value,
  uint32_t idx, lockContext_t *lockContext, err_t *e)
{
	MARK();

	defineError();

	gboolean locked = FALSE;
	uint32_t locks = LOCK_DATA;

	terror(getLocks(lockContext, locks, e))
	locked = TRUE;

	controllerUserStep->value = value;
	doSetControllerStep(pattern, controllerUserStep, idx);

finish:
	if (locked) {
		releaseLocks(lockContext, locks, NULL);
	}
}

void setNoteStep(pattern_t *pattern, noteUserStep_t *noteUserStep,
  GSList *value, GSList *velocity, uint32_t idx, lockContext_t *lockContext,
  gboolean live, err_t *e)
{
	MARK();

	defineError();

	gboolean locked = FALSE;
	uint32_t locks = (LOCK_DATA | LOCK_SEQUENCER);

	if ((noteUserStep->value == value)&&(noteUserStep->velocity == velocity)) {
		goto finish;
	}

	if (live) {
		terror(getLocks(lockContext, locks, e))
		locked = TRUE;
		terror(unsoundPattern(lockContext, pattern, e))
	}

	if (value == NULL) {
		terror(setSlide(pattern, noteUserStep, FALSE, idx,
		  lockContext, FALSE, e))
	}
	terror(unsetNoteStep(pattern, noteUserStep, idx))

	noteUserStep->value = value;
	noteUserStep->velocity = velocity;
	if (noteUserStep->value == NULL) {
		goto finish;
	}
	doSetNoteStep(pattern, noteUserStep, idx);

finish:
	if (locked) {
		terror(releaseLocks(lockContext, locks, NULL))
	}
}

void setSlide(pattern_t *pattern, noteUserStep_t *noteUserStep,
  gboolean slide, uint32_t idx, lockContext_t *lockContext,
  gboolean live, err_t *e)
{
	MARK();

	defineError();

	gboolean locked = FALSE;
	uint32_t locks = (LOCK_DATA | LOCK_SEQUENCER);

	if (noteUserStep->slide == slide) {
		goto finish;
	}

	terror(failIfFalse(((!slide)||(noteUserStep->value != NULL))))

	if (live&&(lockContext != NULL)) {
		terror(getLocks(lockContext, locks, e))
		locked = TRUE;
		terror(unsoundPattern(lockContext, pattern, e))
	}

	terror(unsetNoteStep(pattern, noteUserStep, idx))
	noteUserStep->slide = slide;
	if (noteUserStep->value == NULL) {
		goto finish;
	}
	doSetNoteStep(pattern, noteUserStep, idx);

finish:
	if (locked) {
		terror(releaseLocks(lockContext, locks, NULL))
	}
}

void freeValue(pattern_t *pattern, void *value)
{
	MARK();

	if (IS_NOTE(pattern)) {
		freeNoteValue((noteValue_t *) value);
	} else {
		freeControllerValue((controllerValue_t *) value);
	}
}

static void cbFreeValue(gpointer data, gpointer user_data)
{
	MARK();

	freeValue((pattern_t *) user_data, data);
}

static void cbFreePattern(gpointer data, gpointer user_data)
{
	MARK();

	freePattern((pattern_t *) data);
}

void freePattern(pattern_t *pattern)
{
	MARK();

	g_slist_foreach((GSList *) CHILDREN(pattern), cbFreePattern, NULL);
	g_slist_free((GSList *) CHILDREN(pattern));

	free(pattern->name);
	if (USERSTEPS(pattern) != NULL)  {
		for (uint32_t i = 0; i < NR_USERSTEPS(pattern); i++) {
			UNSET_STEP(pattern, USERSTEP_AT(pattern, i), i);
		}
		free(USERSTEPS(pattern));
	}
	if (!IS_DUMMY(pattern)) {
		g_slist_foreach(VALUES(pattern), cbFreeValue, pattern);
		g_slist_free(VALUES(pattern));
		free(EVENTSTEPS(pattern));
	}
	free(pattern);
}

pattern_t *allocatePattern(pattern_t *parent)
{
	MARK();

	pattern_t *result = (pattern_t *) calloc(1, sizeof(pattern_t));

	return result;
}


void freeControllerValue(controllerValue_t *controllerValue)
{
	MARK();

	free((char *) controllerValue->name);
	free(controllerValue);
}

void freeNoteValue(noteValue_t *noteValue)
{
	MARK();

	free((char *) noteValue->name);
	free(noteValue);
}

noteValue_t *allocateNoteValue(void)
{
	MARK();

	noteValue_t *result = (noteValue_t *) calloc(1, sizeof((*result)));

	return result;
}

controllerValue_t *allocateControllerValue(void)
{
	MARK();

	controllerValue_t *result =
	  (controllerValue_t *) calloc(1, sizeof((*result)));

	return result;
}

void setSteps(pattern_t *pattern)
{
	MARK();

	USERSTEPS(pattern) =
	  (char *) calloc(NR_USERSTEPS(pattern), SZ_USERSTEP(pattern));
	if (!IS_DUMMY(pattern)) {
		EVENTSTEPS(pattern) =
		  (char *) calloc(NR_EVENTSTEPS(pattern), SZ_EVENTSTEP(pattern));
	}
}

void adjustSteps(pattern_t *pattern, uint32_t bars, uint32_t stepsPerBar,
  lockContext_t *lockContext, gboolean live, err_t *e)
{
	MARK();

	defineError();

	gboolean locked = FALSE;
	void *userSteps = USERSTEPS(pattern);
	void *eventSteps = NULL;
	uint32_t haveBars = NR_BARS(pattern);
	uint32_t haveStepsPerBar = NR_STEPS_PER_BAR(pattern);
	uint32_t lastStepIdx = (bars * stepsPerBar) - 1;
	uint32_t stepDivisor = (stepsPerBar <= haveStepsPerBar) ?
	  1 : (stepsPerBar / haveStepsPerBar);
	uint32_t stepMultiplier = (stepsPerBar >= haveStepsPerBar) ?
	  1 : (haveStepsPerBar / stepsPerBar);
	uint32_t locks = (!live) ? 0 : (IS_NOTE(pattern) && (userSteps != NULL)) ?
	  (LOCK_DATA | LOCK_SEQUENCER) : LOCK_DATA;

	if (locks != 0) {
		terror(getLocks(lockContext, locks, e))
		locked = TRUE;
		if (locks & LOCK_SEQUENCER) {
			terror(unsoundPattern(lockContext, pattern, e))
		}
	}
	if (!IS_DUMMY(pattern)) {
		eventSteps = EVENTSTEPS(pattern);
	}
	NR_BARS(pattern) = bars;
	NR_STEPS_PER_BAR(pattern) = stepsPerBar;

	terror(setSteps(pattern))

	if  (userSteps == NULL) {
		goto finish;
	}
	for (uint32_t i = 0; i < bars; i++) {
		for (uint32_t j = 0; j < stepsPerBar; j++) {
			uint32_t sourceBarIdx = i % haveBars;
			uint32_t sourceStepIdx = j / stepDivisor;
			uint32_t targetStepIdx = (i * stepsPerBar) + j;
			gboolean last = (targetStepIdx == lastStepIdx);

			sourceStepIdx *= stepMultiplier;
			sourceStepIdx += (sourceBarIdx * haveStepsPerBar);
			terror(SET_STEP_FROM_STEP(pattern,
			  USERSTEP_AT2(pattern->patternType, userSteps, sourceStepIdx),
			  USERSTEP_AT(pattern, targetStepIdx), targetStepIdx,
			  last, lockContext, e))
		}
	}

finish:
	if (locked) {
		terror(releaseLocks(lockContext, locks, NULL))
	}
	free(userSteps);
	free(eventSteps);
}

void deleteChild(pattern_t *parent, GSList *childLink,
  lockContext_t *lockContext, err_t *e)
{
	MARK();

	defineError();

	uint32_t locks = (IS_NOTE(((pattern_t *) childLink->data))) ?
	  (LOCK_DATA | LOCK_SEQUENCER) : LOCK_DATA;
	gboolean locked = FALSE;

	terror(getLocks(lockContext, locks, e))
	locked = TRUE;

	if (locks & LOCK_SEQUENCER) {
		terror(unsoundPattern(lockContext, (pattern_t *) childLink->data, e))
	}

	freePattern((pattern_t *) childLink->data);

	CHILDREN(parent) =
	  g_slist_delete_link((GSList *) CHILDREN(parent), childLink);

finish:
	if (locked) {
		terror(releaseLocks(lockContext, locks, NULL))
	}
}

typedef struct {
	lockContext_t *lockContext;
	err_t *e;
} crutch_t;

static void noteOff(gpointer data, gpointer user_data)
{
	MARK();

	midiMessage_t *midiMessage = NULL;
	noteEvent_t *noteEvent = (noteEvent_t *) data;
	crutch_t *crutch = (crutch_t *) user_data;
	lockContext_t *lockContext = crutch->lockContext;
	err_t *e = crutch->e;

	midiMessage = getNoteOffMidiMessage(noteEvent);
	terror(fireMidiMessage(lockContext, midiMessage, e))
	midiMessage = NULL;

	noteEvent->off.noteOffLink = NULL;

finish:
	free(midiMessage);
}

void allNotesOff(lockContext_t *lockContext, gboolean alreadyLocked, err_t *e)
{
	MARK();

	defineError();

	crutch_t crutch;
	gboolean unlock = FALSE;
	uint32_t locks = (LOCK_SEQUENCER | LOCK_DATA);

	if (alreadyLocked) {
		terror(requireLocks(lockContext, locks, e))
	} else {
		terror(getLocks(lockContext, locks, e))
		unlock = TRUE;
	}

	crutch.lockContext = lockContext;
	crutch.e = e;
	terror(g_slist_foreach((GSList *) notesOff.value, noteOff, &crutch))
	g_slist_free((GSList *) notesOff.value);
	notesOff.value = NULL;

finish:
	if (unlock) {
		releaseLocks(lockContext, locks, NULL);
	}
}

void randomise(pattern_t *pattern, uint32_t bar, lockContext_t *lockContext)
{
	MARK();

	uint32_t lastUserstep = NR_USERSTEPS(pattern) - 1;
	uint32_t start = (bar * NR_USERSTEPS_PER_BAR(pattern));
	uint32_t end = (start + NR_USERSTEPS_PER_BAR(pattern));
	uint32_t nrVelocities = IS_NOTE(pattern) ? NR_VELOCITIES(pattern) : 0;

	for (uint32_t i = start; i < end; i++) {
		noteUserStep_t *noteUserStep = NULL;
		GSList *value = NULL;
		GSList *velocity = NULL;
		gboolean slide = FALSE;
		uint32_t nrValues = NR_VALUES(pattern);
		void *step = USERSTEP_AT(pattern, i);

		if (!getLocked(NULL, USERSTEP_AT(pattern, i), pattern, i)) {
			if (IS_DUMMY(pattern)) {
				if ((rand() % 2) == 0) {
					continue;
				}
				setDummyStep(pattern, (dummyUserStep_t *) step,
				  !IS_SET((dummyUserStep_t *) step, TYPE(pattern)), lockContext, NULL);
				continue;
			}
			if (!anyChildStepSet(pattern, i)) {
				nrValues++;
			}
			uint32_t idx = (rand() % nrValues);
			value = g_slist_nth(VALUES(pattern), idx);
			if (IS_CONTROLLER(pattern)) {
				setControllerStep(pattern,
				  (controllerUserStep_t *) step, value, i, lockContext, NULL);
				continue;
			}
			idx = (value != NULL) ? (rand() % nrVelocities) : nrVelocities;
			velocity = g_slist_nth(VELOCITIES(pattern), idx);
			setNoteStep(pattern, (noteUserStep_t *) step, value, velocity, i,
			  lockContext, TRUE, NULL);

		} else if (!IS_NOTE(pattern)) {
			continue;
		}
		
		if (VALUE(step, TYPE(pattern)) == NULL) {
			continue;
		}
		if (i >= lastUserstep) {
			continue;
		}
		noteUserStep = (noteUserStep_t *) USERSTEP_AT(pattern, i);
		if (noteUserStep->slideLocked) {
			continue;
		}
		slide = ((rand() % 2) == 0);
		if (HAS_SLIDE(step, TYPE(pattern)) == slide) {
			continue;
		}
		setSlide(pattern, (noteUserStep_t *) step,
		  slide, i, lockContext, TRUE, NULL);
	}
}

gboolean getLocked(gboolean *unlockable,
  void *step, pattern_t *pattern, uint32_t idx)
{
	MARK();

	gboolean result = FALSE;
	uint32_t nrValues = NR_VALUES(pattern);

	if (IS_NOTE(pattern)) {
		nrValues *= NR_VELOCITIES(pattern);
	}

	if ((!IS_SET(step, TYPE(pattern)))&&(!(IS_ROOT(pattern)))) {
		result = !IS_SET(PARENTSTEP(pattern, idx), TYPE(PARENT(pattern)));
	}
	if ((!result)&&(IS_SET(step, TYPE(pattern))&&(nrValues < 2))) {
		result = anyChildStepSet(pattern, idx);
	}
	if (unlockable != NULL) {
		*unlockable = !result;
	}
	if (!result) {
		result = LOCKED(step, TYPE(pattern));
	}

	return result;
}
