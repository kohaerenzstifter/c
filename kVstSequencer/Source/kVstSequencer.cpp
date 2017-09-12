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

void lock(lockContext_t *lockContext, err_t *e)
{
	MARK();

	defineError();

	terror(failIfFalse((lockContext != NULL)))

	if (lockContext->count < 1) {
		terror(lockMutex(&(mutex.value), e))
	}
	lockContext->count++;

finish:
	return;
}

void unlock(lockContext_t *lockContext)
{
	MARK();

	err_t err;
	err_t *e = &err;

	initErr(e);

	if (lockContext->count < 2) {
		terror(unlockMutex(&(mutex.value), e))
	}
	lockContext->count--;

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

	gboolean locked = FALSE;

	terror(lock(lockContext, e))
	locked = TRUE;

	midiMessages = g_slist_prepend((GSList *) midiMessages, midiMessage);

finish:
	if (locked) {
		unlock(lockContext);
	}
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
	gboolean locked = FALSE;

	terror(lock(lockContext, e))
	locked = TRUE;

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
	if (locked) {
		unlock(lockContext);
	}
	free(midiMessage);
}

void unsoundPattern(lockContext_t *lockContext,
  pattern_t *pattern, err_t *e)
{
	MARK();

	defineError();

	gboolean locked = FALSE;

	terror(failIfFalse((IS_NOTE(pattern))))
	terror(lock(lockContext, e))
	locked = TRUE;

	if (EVENTSTEPS(pattern) == NULL) {
		goto finish;
	}

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
	if (locked) {
		unlock(lockContext);
	}
	return;
}

static void unsoundPattern2(lockContext_t *lockContext,
  pattern_t *pattern, err_t *e)
{
	MARK();

	defineError();

	gboolean locked = FALSE;

	terror(lock(lockContext, e))
	locked = TRUE;

	for (GSList *cur = ((GSList *) CHILDREN(pattern)); cur != NULL;
	  cur = g_slist_next(cur)) {
		terror(unsoundPattern2(lockContext, ((pattern_t *) cur->data), e))
	}

	if (IS_NOTE(pattern)) {
		terror(unsoundPattern(lockContext, pattern, e))
	}

finish:
	if (locked) {
		unlock(lockContext);
	}
	return;
}

void unsoundAllPatterns(lockContext_t *lockContext, err_t *e)
{
	MARK();

	defineError();

	gboolean locked = FALSE;

	terror(lock(lockContext, e))
	locked = TRUE;

	if (patterns.root != NULL) {
		terror(unsoundPattern2(lockContext, ((pattern_t *) patterns.root), e))
	}

finish:
	if (locked) {
		unlock(lockContext);
	}
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

	LOCKED(step, TYPE(pattern)) = !LOCKED(step, TYPE(pattern));
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

	terror(lock(lockContext, e))
	locked = TRUE;
	dummyUserStep->set = set;

finish:
	if (locked) {
		unlock(lockContext);
	}
}

void setControllerStep(pattern_t *pattern,
  controllerUserStep_t *controllerUserStep, GSList *value,
  uint32_t idx, lockContext_t *lockContext, err_t *e)
{
	MARK();

	defineError();

	gboolean locked = FALSE;

	terror(lock(lockContext, e))
	locked = TRUE;

	controllerUserStep->value = value;
	doSetControllerStep(pattern, controllerUserStep, idx);

finish:
	if (locked) {
		unlock(lockContext);
	}
}

void setNoteStep(pattern_t *pattern, noteUserStep_t *noteUserStep,
  GSList *value, GSList *velocity, uint32_t idx, lockContext_t *lockContext,
  err_t *e)
{
	MARK();

	defineError();

	gboolean locked = FALSE;

	if ((noteUserStep->value == value)&&(noteUserStep->velocity == velocity)) {
		goto finish;
	}

	terror(lock(lockContext, e))
	locked = TRUE;
	terror(unsoundPattern(lockContext, pattern, e))

	if (value == NULL) {
		terror(setSlide(pattern, noteUserStep, FALSE, idx,
		  lockContext, e))
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
		unlock(lockContext);
	}
}

void setSlide(pattern_t *pattern, noteUserStep_t *noteUserStep,
  gboolean slide, uint32_t idx, lockContext_t *lockContext,
  err_t *e)
{
	MARK();

	defineError();

	gboolean locked = FALSE;

	if (noteUserStep->slide == slide) {
		goto finish;
	}

	terror(failIfFalse(((!slide)||(noteUserStep->value != NULL))))

	if (lockContext != NULL) {
		terror(lock(lockContext, e))
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
		unlock(lockContext);
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
  lockContext_t *lockContext, int32_t shift, err_t *e)
{
	MARK();

	defineError();

	gboolean locked = FALSE;
	void *userSteps = USERSTEPS(pattern);
	void *eventSteps = NULL;
	uint32_t haveBars = NR_BARS(pattern);
	uint32_t haveStepsPerBar = NR_STEPS_PER_BAR(pattern);
	uint32_t nrSteps = (bars * stepsPerBar);
	uint32_t lastStepIdx = nrSteps - 1;
	uint32_t stepDivisor = (stepsPerBar <= haveStepsPerBar) ?
	  1 : (stepsPerBar / haveStepsPerBar);
	int32_t shiftAdd = (stepDivisor * shift);
	uint32_t stepMultiplier = (stepsPerBar >= haveStepsPerBar) ?
	  1 : (haveStepsPerBar / stepsPerBar);

	terror(lock(lockContext, e))
	locked = TRUE;

	if (IS_NOTE(pattern)) {
		terror(unsoundPattern(lockContext, pattern, e))
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
			int32_t targetStepIdx =
			  (i * stepsPerBar) + j + shiftAdd;
			gboolean last;

			if (targetStepIdx < 0) {
				targetStepIdx = nrSteps + targetStepIdx;
			} else if (targetStepIdx > lastStepIdx) {
				targetStepIdx = (targetStepIdx - nrSteps);
			}
			last = (targetStepIdx == lastStepIdx);

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
		unlock(lockContext);
	}
	free(userSteps);
	free(eventSteps);
}

void deleteChild(pattern_t *parent, GSList *childLink,
  lockContext_t *lockContext, err_t *e)
{
	MARK();

	defineError();

	gboolean locked = FALSE;
	pattern_t *pattern = (pattern_t *) childLink->data;

	terror(lock(lockContext, e))
	locked = TRUE;

	if (IS_NOTE(pattern)) {
		terror(unsoundPattern(lockContext, pattern, e))
	}

	freePattern((pattern_t *) childLink->data);

	CHILDREN(parent) =
	  g_slist_delete_link((GSList *) CHILDREN(parent), childLink);

finish:
	if (locked) {
		unlock(lockContext);
	}
}

void promotePattern(pattern_t *pattern, lockContext_t *lockContext, err_t *e)
{
	MARK();

	defineError();

	gboolean locked = FALSE;
	pattern_t *parent = NULL;
	GSList *link = NULL;

	terror(lock(lockContext, e))
	locked = TRUE;

	if (IS_NOTE(pattern)) {
		terror(unsoundPattern(lockContext, pattern, e))
	}

	parent = PARENT(pattern);
	terror(failIfFalse((parent != NULL)))
	link = g_slist_find(((GSList *) CHILDREN(parent)), pattern);
	terror(failIfFalse((link != NULL)))
	CHILDREN(parent) = g_slist_delete_link(((GSList *) CHILDREN(parent)), link);
	CHILDREN(patterns.root) =
	  g_slist_append(((GSList *) CHILDREN(patterns.root)), pattern);
	PARENT(pattern) = ((pattern_t *) patterns.root);

finish:
	if (locked) {
		unlock(lockContext);
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

void allNotesOff(lockContext_t *lockContext, err_t *e)
{
	MARK();

	defineError();

	crutch_t crutch;
	gboolean locked = FALSE;

	terror(lock(lockContext, e))
	locked = TRUE;

	crutch.lockContext = lockContext;
	crutch.e = e;
	terror(g_slist_foreach((GSList *) notesOff.value, noteOff, &crutch))
	g_slist_free((GSList *) notesOff.value);
	notesOff.value = NULL;

finish:
	if (locked) {
		unlock(lockContext);
	}
}

void randomise(pattern_t *pattern, uint32_t bar, uint8_t probability,
  lockContext_t *lockContext)
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

		if (!getLocked(NULL, USERSTEP_AT(pattern, i), pattern, i, FALSE)) {
			gboolean setStep = FALSE;
			if (anyChildStepSet(pattern, i)) {
				setStep = TRUE;
			} else {
				setStep = ((rand() % 100) < probability);
			}
			if (IS_DUMMY(pattern)) {
				setDummyStep(pattern, ((dummyUserStep_t *) step),
				  setStep, lockContext, NULL);
				continue;
			}

			uint32_t idx = nrValues;
			if (setStep) {
				idx = (rand() % nrValues);
			}
			value = g_slist_nth(VALUES(pattern), idx);
			if (IS_CONTROLLER(pattern)) {
				setControllerStep(pattern,
				  (controllerUserStep_t *) step, value, i, lockContext, NULL);
				continue;
			}
			idx = (value != NULL) ? (rand() % nrVelocities) : nrVelocities;
			velocity = g_slist_nth(VELOCITIES(pattern), idx);
			setNoteStep(pattern, (noteUserStep_t *) step, value, velocity, i,
			  lockContext, NULL);

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
		  slide, i, lockContext, NULL);
	}
}

gboolean getLocked(gboolean *unlockable,
  void *step, pattern_t *pattern, uint32_t idx, gboolean onlyByParent)
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
	if (!onlyByParent) {
		if ((!result)&&(IS_SET(step, TYPE(pattern))&&(nrValues < 2))) {
			result = anyChildStepSet(pattern, idx);
		}
	}
	if (unlockable != NULL) {
		*unlockable = !result;
	}
	if (!onlyByParent) {
		if (!result) {
			result = LOCKED(step, TYPE(pattern));
		}
	}

	return result;
}

gboolean isAnyStepLockedByParent(pattern_t *pattern)
{
	MARK();

	gboolean result = FALSE;

	for (uint32_t i = 0; i < NR_USERSTEPS(pattern); i++) {
		if (getLocked(NULL, USERSTEP_AT(pattern, i), pattern, i, TRUE)) {
			result = TRUE;
			break;
		}
	}

	return result;
}


static void readWriteStream(void *data, uint32_t length, void *stream,
  gboolean reading, err_t *e)
{
	MARK();

	if (reading) {
		InputStream *inputStream = (InputStream *) stream;
		terror(failIfFalse(inputStream->read(data, length) == length))
	} else {
		OutputStream *outputStream = (OutputStream *) stream;
		terror(failIfFalse(outputStream->write(data, length)))
	}

finish:
	return;
}

static char *readStringFromStream(InputStream *inputStream, err_t *e)
{
	MARK();

	size_t length = 0;
	char *result = NULL;
	char *_result = NULL;

	terror(readWriteStream(&length, sizeof(length), inputStream, TRUE, e))
	_result = (char *) calloc(1, (length + 1));
	terror(readWriteStream(_result, length, inputStream, TRUE, e))
	_result[length] = '\0';

	result = _result; _result = NULL;
finish:
	return result;
}

static void writeStringToStream(char *string,
  OutputStream *outputStream, err_t *e)
{
	MARK();

	size_t length = strlen(string);

	terror(readWriteStream(&length, sizeof(length),
	  outputStream, FALSE, e))
	terror(readWriteStream(string, length, outputStream, FALSE, e))

finish:
	return;
}

static void loadStoreControllerValue(controllerValue_t **controllerValue,
  void *stream, gboolean load, err_t *e)
{
	MARK();

	controllerValue_t *freeMe = NULL;

	if (load) {
		freeMe = (*controllerValue) = allocateControllerValue();
		terror((*controllerValue)->name =
		  readStringFromStream(((InputStream *) stream), e))
	} else {
		terror(writeStringToStream((char *) (*controllerValue)->name,
		  ((OutputStream *) stream), e))
	}

	terror(readWriteStream((void *) &((*controllerValue)->value),
	  sizeof((*controllerValue)->value), stream, load, e))

	freeMe = NULL;
finish:
	if (freeMe != NULL)  {
		freeControllerValue(freeMe);
	}
}

static void loadStoreNoteValue(noteValue_t **noteValue, void *stream,
  gboolean load, err_t *e)
{
	MARK();

	noteValue_t *freeMe = NULL;

	if (load) {
		freeMe = (*noteValue) = allocateNoteValue();
		terror((*noteValue)->name =
		  readStringFromStream(((InputStream *) stream), e))
	} else {
		terror(writeStringToStream((char *) (*noteValue)->name,
		  ((OutputStream *) stream), e))
	}

	terror(readWriteStream((void *) &((*noteValue)->note),
	  sizeof((*noteValue)->note), stream, load, e))
	terror(readWriteStream((void *) &((*noteValue)->sharp),
	  sizeof((*noteValue)->sharp), stream, load, e))
	terror(readWriteStream((void *) &((*noteValue)->octave),
	  sizeof((*noteValue)->octave), stream, load, e))

	freeMe = NULL;
finish:
	if (freeMe != NULL) {
		freeNoteValue(freeMe);
	}
}

static void loadStoreValue(void **value, pattern_t *pattern, void *stream,
  gboolean load, gboolean velocities, err_t *e)
{
	MARK();

	if (velocities || IS_CONTROLLER(pattern)) {
		terror(loadStoreControllerValue(((controllerValue_t **) value),
		  stream, load, e))
	} else {
		terror(loadStoreNoteValue(((noteValue_t **) value), stream, load, e))
	}

finish:
	return;
}

static void loadStoreValuesVelocities(pattern_t *pattern, void *stream,
  gboolean load, gboolean velocities, err_t *e)
{
	MARK();

	guint length = 0;
	uint32_t i = 0;
	void *value = NULL;

	if (!load) {
		length = velocities ? NR_VELOCITIES(pattern) : NR_VALUES(pattern);
	}

	terror(readWriteStream(&length, sizeof(length), stream, load, e))
	for (i = 0; i < length; i++) {
		if (!load) {
			value = g_slist_nth_data(velocities ?
			  VELOCITIES(pattern) : VALUES(pattern), i);
		}
		terror(loadStoreValue(&value, pattern, stream, load, velocities, e))
		if (load) {
			if (velocities) {
				VELOCITIES(pattern) =
				  g_slist_append(VELOCITIES(pattern), value);
			} else {
				VALUES(pattern) = g_slist_append(VALUES(pattern), value);
			}
		}
	}

finish:
	return;
}

static void loadStoreValues(pattern_t *pattern, void *stream,
  gboolean load, err_t *e)
{
	terror(loadStoreValuesVelocities(pattern, stream, load, FALSE, e))

finish:
	return;
}

static void loadStoreVelocities(pattern_t *pattern, void *stream,
  gboolean load, err_t *e)
{
	terror(loadStoreValuesVelocities(pattern, stream, load, TRUE, e))

finish:
	return;
}

static void loadStoreStep(lockContext_t *lockContext, void *step,
  pattern_t *pattern, void *stream, uint32_t idx, gboolean load, err_t *e)
{
	MARK();

	gint valuePosition = -1;
	gint velocityPosition = -1;
	noteUserStep_t *noteUserStep = NULL;
	gboolean set = FALSE;
	gboolean slide = FALSE;

	terror(readWriteStream(LOCKED_PTR(step, TYPE(pattern)),
	  sizeof(LOCKED(step, TYPE(pattern))), stream, load, e))

	if (IS_DUMMY(pattern)) {
		dummyUserStep_t *dummyUserStep = (dummyUserStep_t *) step;

		if (!load) {
			set = dummyUserStep->set;
		}
		terror(readWriteStream(&set, sizeof(set), stream, load, e))
		if ((load)&&(set)) {
			terror(setDummyStep(pattern, dummyUserStep, set, lockContext, e))
		}
		goto finish;
	}

	if ((!load)&&(VALUE(step, TYPE(pattern)) != NULL)) {
		valuePosition = g_slist_position(VALUES(pattern),
		  (GSList *) VALUE(step, TYPE(pattern)));
		if (IS_NOTE(pattern))  {
			velocityPosition = g_slist_position(VELOCITIES(pattern),
			  (GSList *) VELOCITY(step, TYPE(pattern)));
		}
	}
	terror(readWriteStream(&valuePosition, sizeof(valuePosition),
	  stream, load, e))
	if (IS_NOTE(pattern)) {
		terror(readWriteStream(&velocityPosition,
		  sizeof(velocityPosition), stream, load, e))
	}
	if ((load)&&(valuePosition > -1)) {
		if (IS_NOTE(pattern)) {
			terror(setNoteStep(pattern, (noteUserStep_t *) step,
			  g_slist_nth(VALUES(pattern), valuePosition),
			  g_slist_nth(VELOCITIES(pattern), velocityPosition),
			  idx, lockContext, e))
		} else {
			terror(setControllerStep(pattern, (controllerUserStep_t *) step,
			  g_slist_nth(VALUES(pattern), valuePosition), idx,
			  lockContext, e))
		}
	}
	if (IS_CONTROLLER(pattern)) {
		goto finish;
	}

	noteUserStep = (noteUserStep_t *) step;
	if (!load) {
		slide = noteUserStep->slide;
	}
	terror(readWriteStream(&slide, sizeof(slide), stream, load, e))
	if (load&&slide) {
		terror(setSlide(pattern, noteUserStep, slide,
		  idx, lockContext, e))
	}
	terror(readWriteStream(&(noteUserStep->slideLocked),
	  sizeof(noteUserStep->slideLocked), stream, load, e))


finish:
	return;
}

static void loadStoreChildren(lockContext_t *lockContext, pattern_t *parent,
  void *stream, gboolean load, err_t *e)
{
	MARK();

	uint32_t count = 0;

	if (!load) {
		count = g_slist_length((GSList *) parent->children);
	}

	terror(readWriteStream(&count, sizeof(count), stream, load, e))

	for (uint32_t i = 0; i < count; i++) {
		pattern_t *child = NULL;
		if (!load) {
			child =
			  (pattern_t *) g_slist_nth_data((GSList *) parent->children, i);
		}
		terror(loadStorePattern(lockContext, &child, stream, load, parent, e))
		if (load) {
			parent->children =
			  g_slist_append((GSList *) parent->children, child);
		}
	}

finish:
	return;
}

void loadStorePattern(lockContext_t *lockContext, pattern_t **pattern,
  void *stream, gboolean load, pattern_t *parent,
  err_t *e)
{
	MARK();

	void *freeMe = NULL;
	pattern_t *p = NULL;
	OutputStream *outputStream = NULL;
	InputStream *inputStream = NULL;

	if (load) {
		inputStream = (InputStream *) stream;
		freeMe = p = allocatePattern(parent);
		PARENT(p) = parent;
		terror(NAME(p) = readStringFromStream(inputStream, e))	
	} else {
		outputStream = (OutputStream *) stream;
		p = *pattern;
		terror(writeStringToStream(NAME(p), outputStream, e))
	}

	terror(readWriteStream(&TYPE(p), sizeof(TYPE(p)), stream, load, e))

	if (!IS_DUMMY(p)) {
		terror(readWriteStream((void *) PTR_CHANNEL(p),
		  sizeof(CHANNEL(p)), stream, load, e))
	}

	terror(readWriteStream((void *) &NR_USERSTEPS_PER_BAR(p),
	  sizeof(NR_USERSTEPS_PER_BAR(p)), stream, load, e))
	terror(readWriteStream((void *) &NR_BARS(p),
	  sizeof(NR_BARS(p)), stream, load, e))

	if (load) {
		terror(adjustSteps(p, NR_BARS(p),
		  NR_USERSTEPS_PER_BAR(p), lockContext, 0,e))
	}

	if (!IS_DUMMY(p)) {
		terror(loadStoreValues(p, stream, load, e))
		if (!IS_NOTE(p)) {
			terror(readWriteStream((void *) PTR_PARAMETER(p),
			  sizeof(PARAMETER(p)), stream, load, e))
		} else {
			terror(loadStoreVelocities(p, stream, load, e))
		}
	}

	for (uint32_t i = 0; i < NR_USERSTEPS(p); i++) {
		void *step = USERSTEP_AT(p, i);

		terror(loadStoreStep(lockContext, step, p, stream, i, load, e))
	}

	terror(loadStoreChildren(lockContext, p, stream, load, e))

	if (load) {
		*pattern = p;
	}

	freeMe = NULL;
finish:
	if (freeMe != NULL) {
		freePattern((pattern_t *) freeMe);
	}

}


void setLive(lockContext_t *lockContext, pattern_t *newRoot, err_t *e)
{
	MARK();

	defineError();

	gboolean locked = FALSE;

	terror(lock(lockContext, e))
	locked = TRUE;

	terror(unsoundAllPatterns(lockContext, e))

	if (patterns.root != NULL) {
		freePattern(((pattern_t *) patterns.root));
	}
	patterns.root = newRoot;
	live = (newRoot == NULL);

finish:
	if (locked) {
		unlock(lockContext);
	}
}
