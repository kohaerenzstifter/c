#include "ksequencer.h"

static void lockMutex(mutex_t *mutex, err_t *e)
{
	defineError();

	terror(failIfFalse(mutex->initialised))
	terror(failIfFalse((pthread_mutex_lock((&(mutex->value))) == 0)))

finish:
	return;
}

static void unlockMutex(mutex_t *mutex, err_t *e)
{
	defineError();

	terror(failIfFalse(mutex->initialised))
	pthread_mutex_unlock((&(mutex->value)));

finish:
	return;
}

static void lockCounted(lockContext_t *lockContext, uint32_t mutex, err_t *e)
{
	defineError();
#if 0
	if (lockContext == NULL) {
		goto finish;
	}
#endif
#if 0
	if (lockContext->mutexes[mutex] < 1)  {
#endif
		for (uint32_t i = 0; i < mutex; i++) {
			terror(failIfFalse((lockContext->mutexes[i] <=
			  lockContext->mutexes[mutex])))
		}
		if (lockContext->mutexes[mutex] < 1) {
			terror(lockMutex(&mutexes.value[mutex], e))
		}
#if 0
	}
#endif
	lockContext->mutexes[mutex]++;

finish:
	return;
}

static void unlockCounted(lockContext_t *lockContext, uint32_t mutex, err_t *e)
{
	defineError();
#if 0
	if (lockContext == NULL) {
		goto finish;
	}
#endif
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
	for (int32_t i = (NR_MUTEXES - 1),
	  mask = (1 << i); i >= 0; i--, mask >>= 1)  {
		if (!(locks & mask)) {
			continue;
		}
		terror(lockCounted(lockContext, i, e))
	}

finish:
	return;
}

void releaseLocks(lockContext_t *lockContext, uint32_t locks, err_t *e)
{
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

static void unsoundNoteEvent(noteEvent_t *offNoteEvent)
{
	if (offNoteEvent->off.noteOffLink == NULL) {
		goto finish;
	}

	offNoteEvent->noteValue->snd_seq_event->dest = sequencer.snd_seq_addr;
	offNoteEvent->noteValue->snd_seq_event->type = SND_SEQ_EVENT_NOTEOFF;

	snd_seq_event_output(sequencer.snd_seq,
	  (snd_seq_event_t *) offNoteEvent->noteValue->snd_seq_event);
	snd_seq_drain_output(sequencer.snd_seq);
	notesOff.value =
	  g_slist_delete_link((GSList *) notesOff.value,
	  (GSList *) offNoteEvent->off.noteOffLink);
	offNoteEvent->off.noteOffLink = NULL;

finish:
	return;
}

void unsoundPattern(lockContext_t *lockContext,
  pattern_t *pattern, err_t *e)
{
	defineError();

	terror(failIfFalse((TYPE(pattern) == patternTypeNote)))

	terror(requireLocks(lockContext, (LOCK_DATA | LOCK_SEQUENCER), e))

	for (uint32_t i = 0; i < NR_EVENTSTEPS(pattern); i++) {
		noteEventStep_t *noteEventStep = EVENTSTEP_AT(pattern, i);
		if (noteEventStep->onNoteEvent == NULL) {
			continue;
		}
		unsoundNoteEvent((noteEvent_t *)
		  noteEventStep->onNoteEvent->on.offNoteEvent);
	}

finish:
	return;
}

static void redirect(pattern_t *pattern, uint32_t startIdx,
  noteEvent_t *find, noteEvent_t *replace)
{
	uint32_t numberSteps = NR_USERSTEPS(pattern);

	for (uint32_t i = startIdx; i < numberSteps; i++) {
		noteUserStep_t *noteUserStep = USERSTEP_AT(pattern, i);

		if (noteUserStep->onNoteEvent != find) {
			break;
		}
		noteUserStep->onNoteEvent = replace;
	}
}

static void unsetNoteStep(pattern_t *pattern,
  noteUserStep_t *noteUserStep, uint32_t userStepIdx)
{
	noteUserStep_t *previous = (userStepIdx < 1) ? NULL :
	  USERSTEP_AT(pattern, (userStepIdx - 1));
	noteUserStep_t *next =
	  (userStepIdx >= (NR_USERSTEPS(pattern) - 1)) ? NULL :
	  USERSTEP_AT(pattern, (userStepIdx + 1));

	uint32_t eventStepIdx =
	  userStepIdx * EVENTSTEPS_PER_USERSTEP(TYPE((pattern)));

	void moveStepOnToNext(void) {
		noteEventStep_t *isNoteOnEventStep =
		  (noteEventStep_t *) noteUserStep->onNoteEvent->noteEventStep;
		noteEventStep_t *mustNoteOnEventStep =
		  isNoteOnEventStep + EVENTSTEPS_PER_USERSTEP(TYPE((pattern)));

		noteUserStep->onNoteEvent->noteEventStep = mustNoteOnEventStep;

		mustNoteOnEventStep->onNoteEvent = isNoteOnEventStep->onNoteEvent;
		isNoteOnEventStep->onNoteEvent = NULL;
	}

	void removeNote(void) {
		noteUserStep->onNoteEvent->on.offNoteEvent->noteEventStep->offNoteEvent
		  = NULL;
		free((noteEvent_t *) noteUserStep->onNoteEvent->on.offNoteEvent);
		noteUserStep->onNoteEvent->noteEventStep->onNoteEvent = NULL;
		free(noteUserStep->onNoteEvent);
	}

	void previousNoteOff(void) {
		noteEvent_t *noteEvent =
		  (noteEvent_t *) previous->onNoteEvent->on.offNoteEvent;

		noteUserStep->onNoteEvent->on.offNoteEvent->noteEventStep->offNoteEvent
		  = NULL;

		noteEvent->noteEventStep = EVENTSTEP_AT(pattern, eventStepIdx);
		noteEvent->noteEventStep->offNoteEvent = noteEvent;
	}

	void nextNoteOn(noteEventStep_t *offNoteEventStep,
	  noteValue_t *noteValue) {
		noteEvent_t *onNoteEvent = NULL;
		noteEvent_t *noteOffEvent = NULL;

		noteEventStep_t *eventStep = EVENTSTEP_AT(pattern, eventStepIdx);
		noteEventStep_t *mustStep =
		  eventStep + EVENTSTEPS_PER_USERSTEP(TYPE((pattern)));

		onNoteEvent = calloc(1, sizeof(noteEvent_t));
		onNoteEvent->noteValue = noteValue;
		onNoteEvent->noteEventStep = mustStep;

		noteOffEvent = calloc(1, sizeof(noteEvent_t));
		noteOffEvent->noteValue = noteValue;
		noteOffEvent->noteEventStep = offNoteEventStep;

		onNoteEvent->on.offNoteEvent = noteOffEvent;

		redirect(pattern, (userStepIdx + 1), next->onNoteEvent, onNoteEvent);

		mustStep->onNoteEvent = onNoteEvent;
		offNoteEventStep->offNoteEvent = noteOffEvent;
		eventStep->onNoteEvent = NULL;
	}

	if (noteUserStep->onNoteEvent == NULL) {
		goto finish;
	}

	if ((previous == NULL)||(noteUserStep->onNoteEvent
	  != previous->onNoteEvent)) {
		if ((next != NULL)&&(noteUserStep->onNoteEvent == next->onNoteEvent)) {
			moveStepOnToNext();
		} else {
			removeNote();
		}
	} else {
		noteEventStep_t *offNoteEventStep =
		  (noteEventStep_t *)
		  previous->onNoteEvent->on.offNoteEvent->noteEventStep;
		previousNoteOff();
		if ((next != NULL)&&(noteUserStep->onNoteEvent == next->onNoteEvent)) {
			nextNoteOn(offNoteEventStep, noteUserStep->value->data);
		}
	}

finish:
	noteUserStep->onNoteEvent = NULL;
}

static void doSetNoteStep(pattern_t *pattern,
  noteUserStep_t *noteUserStep, uint32_t userStepIdx)
{
	noteEvent_t *onNoteEvent = NULL;
	noteEvent_t *offNoteEvent = NULL;
	noteValue_t *noteValue = noteUserStep->value->data;
	uint32_t eventStepIdx =
	  userStepIdx * EVENTSTEPS_PER_USERSTEP(TYPE((pattern)));
	noteUserStep_t *previous = (userStepIdx < 1) ? NULL :
	  USERSTEP_AT(pattern, (userStepIdx - 1));
	noteUserStep_t *next =
	  (userStepIdx >= (NR_USERSTEPS(pattern) - 1)) ? NULL :
	  USERSTEP_AT(pattern, (userStepIdx + 1));
	if ((previous != NULL)&&(previous->slide)&&
	  (previous->value != NULL)) {
		noteValue_t *previousNoteValue = previous->value->data;
		if ((noteValue->tone == previousNoteValue->tone)&&
		  (noteValue->sharp == previousNoteValue->sharp)&&
		  (noteValue->octave == previousNoteValue->octave)) {
			onNoteEvent = previous->onNoteEvent;
			onNoteEvent->on.offNoteEvent->noteEventStep->offNoteEvent = NULL;
			free((noteEvent_t *) onNoteEvent->on.offNoteEvent);
		}
	}
	if (onNoteEvent == NULL) {
		onNoteEvent = calloc(1, sizeof(noteEvent_t));
		onNoteEvent->noteEventStep = EVENTSTEP_AT(pattern, (eventStepIdx));
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
			free(next->onNoteEvent);
			redirect(pattern,
			  (userStepIdx + 1), next->onNoteEvent, onNoteEvent);
		}
	}
	if (offNoteEvent == NULL) {
		offNoteEvent = calloc(1, sizeof(noteEvent_t));
		eventStepIdx += EVENTSTEPS_PER_USERSTEP(TYPE((pattern)));
		if (!noteUserStep->slide) {
			eventStepIdx--;
		}
		offNoteEvent->noteEventStep = EVENTSTEP_AT(pattern, (eventStepIdx));
		offNoteEvent->noteEventStep->offNoteEvent = offNoteEvent;
		offNoteEvent->noteValue = noteUserStep->value->data;
	}
	noteUserStep->onNoteEvent = onNoteEvent;
	onNoteEvent->on.offNoteEvent = offNoteEvent;
}

static void doSetControllerStep(pattern_t *pattern,
  controllerUserStep_t *controllerUserStep, uint32_t idx)
{
	controllerValue_t *controllerValue = (controllerUserStep->value == NULL) ?
	  NULL : controllerUserStep->value->data;
	controllerEventStep_t *controllerEventStep =
	  EVENTSTEP_AT(pattern, (idx *
	  EVENTSTEPS_PER_USERSTEP((pattern->patternType))));

	controllerEventStep->controllerValue = controllerValue;
}

static void resetSynchronisation(lockContext_t *lockContext, err_t *e)
{
	defineError();

	terror(requireLocks(lockContext, LOCK_SYNCHRONISATION, e))

	synchronisation.synchronisationStatus.sequence++;
	synchronisation.synchronisationStatus.lastTick.have = FALSE;
	synchronisation.synchronisationStatus.lastTick.value = 0;
	synchronisation.synchronisationStatus.lastTick.at = 0;
	synchronisation.synchronisationStatus.pace.have = FALSE;
	synchronisation.synchronisationStatus.pace.nanosecondsPerTick = 0;

	pthread_cond_broadcast((&(synchronisation.wakeupConsumers)));

finish:
	return;
}

void disconnectFromPort(lockContext_t *lockContext, err_t *e)
{
	defineError();

	gboolean locked = FALSE;
	uint32_t locks = (LOCK_SEQUENCER | LOCK_SYNCHRONISATION);

	terror(getLocks(lockContext, locks, e))
	locked = TRUE;

	snd_seq_disconnect_from(sequencer.snd_seq, sequencer.myPort,
	  sequencer.snd_seq_addr.client, sequencer.snd_seq_addr.port);
	snd_seq_disconnect_to(sequencer.snd_seq, sequencer.myPort,
	  sequencer.snd_seq_addr.client, sequencer.snd_seq_addr.port);

	sequencer.connected = FALSE;

finish:
	if (locked) {	
		releaseLocks(lockContext, locks, NULL);
	}
}

void connectToPort(lockContext_t *lockContext,
  guint client, guint port, err_t *e)
{
	defineError();

	gboolean locked = FALSE;
	uint32_t locks = (LOCK_DATA | LOCK_SEQUENCER | LOCK_SYNCHRONISATION);

	terror(getLocks(lockContext, locks, e))
	locked = TRUE;

	terror(allNotesOff(lockContext, TRUE, e))

	if (sequencer.connected) {
		if ((client == sequencer.snd_seq_addr.client)&&(port ==
		  sequencer.snd_seq_addr.port)) {
			goto finish;
		}

		terror(disconnectFromPort(lockContext, e))
	}

	terror(failIfFalse(snd_seq_connect_from(sequencer.snd_seq,
	  0, client, port) == 0))
	terror(failIfFalse(snd_seq_connect_to(sequencer.snd_seq, 0,
	  client, port) == 0))

	sequencer.snd_seq_addr.client = client;
	sequencer.snd_seq_addr.port = port;
	sequencer.connected = TRUE;

	terror(resetSynchronisation(lockContext, e))

finish:
	if (locked) {	
		releaseLocks(lockContext, locks, NULL);
	}
}

gboolean anyChildStepSet(pattern_t *pattern, uint32_t idx)
{
	gboolean result = FALSE;

	for (GSList *cur = (GSList *) pattern->children; cur != NULL;
	  cur = g_slist_next(cur)) {
		if ((result = anyStepSetForChild(pattern, idx, cur->data))) {
			break;
		}
	}

	return result;
}

void lockUserStep(pattern_t *pattern, uint32_t idx)
{
	void *step = USERSTEP_AT(pattern, idx);

	LOCKED(step, pattern->patternType) = !LOCKED(step, pattern->patternType);
}

void lockSlide(pattern_t *pattern, uint32_t idx)
{
	noteUserStep_t *noteUserStep =
	  (noteUserStep_t *) (USERSTEP_AT(pattern, idx));

	noteUserStep->slideLocked = !noteUserStep->slideLocked;
}

void setDummyStep(pattern_t *pattern, dummyUserStep_t *dummyUserStep,
  gboolean set, lockContext_t *lockContext, err_t *e)
{
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
  GSList *value, uint32_t idx, lockContext_t *lockContext,
  gboolean live, err_t *e)
{
	defineError();

	gboolean locked = FALSE;
	uint32_t locks = (LOCK_DATA | LOCK_SEQUENCER);

	if (noteUserStep->value == value) {
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
	if (IS_NOTE(pattern)) {
		freeNoteValue(value);
	} else {
		freeControllerValue(value);
	}
}

static void cbFreeValue(gpointer data, gpointer user_data)
{
	freeValue(user_data, data);
}

static void cbFreePattern(gpointer data, gpointer user_data)
{
	freePattern(data);
}

void freePattern(pattern_t *pattern)
{
	g_slist_foreach((GSList *) CHILDREN(pattern), cbFreePattern, NULL);
	g_slist_free((GSList *) CHILDREN(pattern));

	free(pattern->name);
	for (uint32_t i = 0; i < NR_USERSTEPS(pattern); i++) {
		UNSET_STEP(pattern, USERSTEP_AT(pattern, i), i);
	}
	free(USERSTEPS(pattern));
	if (!IS_DUMMY(pattern)) {
		g_slist_foreach(VALUES(pattern), cbFreeValue, pattern);
		g_slist_free(VALUES(pattern));
		free(EVENTSTEPS(pattern));
	}
	free(pattern);
}

pattern_t *allocatePattern(pattern_t *parent)
{
	pattern_t *result = calloc(1, sizeof(pattern_t));

	return result;
}


void freeControllerValue(controllerValue_t *controllerValue)

{
	free((snd_seq_event_t *) controllerValue->snd_seq_event);
	free((char *) controllerValue->name);
	free(controllerValue);
}

void freeNoteValue(noteValue_t *noteValue)
{
	free((snd_seq_event_t *) noteValue->snd_seq_event);
	free((char *) noteValue->name);
	free(noteValue);
}

snd_seq_event_t *getAlsaEvent(void)
{
	snd_seq_event_t *result = calloc(1, sizeof((*result)));
#if 0
	snd_seq_ev_clear(result);
#endif
	snd_seq_ev_set_direct(result);

	return result;
}

noteValue_t *allocateNoteValue(void)
{
	noteValue_t *result = calloc(1, sizeof((*result)));

	result->snd_seq_event = getAlsaEvent();

	return result;
}

controllerValue_t *allocateControllerValue(void)
{
	controllerValue_t *result = calloc(1, sizeof((*result)));

	result->snd_seq_event = getAlsaEvent();

	return result;
}

void setAlsaNoteEvent(noteValue_t *noteValue, uint8_t channel,
  lockContext_t *lockContext, gboolean live, err_t *e)
{
	defineError();

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
	
		for (uint32_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
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

	if (live)  {
		terror(requireLocks(lockContext, LOCK_DATA, e))
	}
	snd_seq_ev_set_noteon(noteValue->snd_seq_event, (channel - 1), note, 127);

finish:
	return;
}

void setAlsaControllerEvent2(snd_seq_event_t *snd_seq_event, uint8_t channel,
  uint8_t parameter, uint8_t value)
{
	snd_seq_ev_set_controller(snd_seq_event, (channel - 1),
	  parameter, value);
}

void setAlsaControllerEvent(controllerValue_t *controllerValue,
  uint8_t channel, uint8_t parameter, lockContext_t *lockContext,
  gboolean live, err_t *e)
{
	defineError();

	if (live) {
		terror(requireLocks(lockContext, LOCK_DATA, e))
	}

	terror(setAlsaControllerEvent2((snd_seq_event_t *)
	  controllerValue->snd_seq_event, channel,
      parameter, controllerValue->value))

finish:
	return;
}

void setSteps(pattern_t *pattern)
{
	USERSTEPS(pattern) =
	  calloc(NR_USERSTEPS(pattern), SZ_USERSTEP(pattern));
	if (!IS_DUMMY(pattern)) {
		EVENTSTEPS(pattern) =
		  calloc(NR_EVENTSTEPS(pattern), SZ_EVENTSTEP(pattern));
	}
}

void adjustSteps(pattern_t *pattern, uint32_t bars, uint32_t stepsPerBar,
  lockContext_t *lockContext, gboolean live, err_t *e)
{
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
	defineError();

	uint32_t locks = (IS_NOTE(((pattern_t *) childLink->data))) ?
	  (LOCK_DATA | LOCK_SEQUENCER) : LOCK_DATA;
	gboolean locked = FALSE;

	terror(getLocks(lockContext, locks, e))
	locked = TRUE;

	if (locks & LOCK_SEQUENCER) {
		terror(unsoundPattern(lockContext, childLink->data, e))
	}

	freePattern(childLink->data);

	CHILDREN(parent) =
	  g_slist_delete_link((GSList *) CHILDREN(parent), childLink);

finish:
	if (locked) {
		terror(releaseLocks(lockContext, locks, NULL))
	}
}

static void noteOff(gpointer data, gpointer user_data)
{
	noteEvent_t *noteEvent = data;

	noteEvent->noteValue->snd_seq_event->dest = sequencer.snd_seq_addr;
	noteEvent->noteValue->snd_seq_event->type = SND_SEQ_EVENT_NOTEOFF;
	snd_seq_event_output(sequencer.snd_seq,
	  (snd_seq_event_t *) noteEvent->noteValue->snd_seq_event);
	noteEvent->off.noteOffLink = NULL;
}

void allNotesOff(lockContext_t *lockContext, gboolean alreadyLocked, err_t *e)
{
	defineError();

	uint32_t locks = (LOCK_SEQUENCER | LOCK_DATA);
	gboolean unlock = FALSE;

	if (alreadyLocked) {
		terror(requireLocks(lockContext, locks, e))
	} else {
		terror(getLocks(lockContext, locks, e))
		unlock = TRUE;
	}

	g_slist_foreach((GSList *) notesOff.value, noteOff, NULL);
	snd_seq_drain_output(sequencer.snd_seq);
	g_slist_free((GSList *) notesOff.value);
	notesOff.value = NULL;

finish:
	if (unlock) {
		releaseLocks(lockContext, locks, NULL);
	}
}

void randomise(pattern_t *pattern, uint32_t bar, lockContext_t *lockContext)
{
	uint32_t lastUserstep = NR_USERSTEPS(pattern) - 1;
	uint32_t start = (bar * NR_USERSTEPS_PER_BAR(pattern));
	uint32_t end = (start + NR_USERSTEPS_PER_BAR(pattern));

	for (uint32_t i = start; i < end; i++) {
		noteUserStep_t *noteUserStep = NULL;
		GSList *value = NULL;
		gboolean slide = FALSE;
		uint32_t nrValues = NR_VALUES(pattern);
		void *step = USERSTEP_AT(pattern, i);

		if (!getLocked(NULL, USERSTEP_AT(pattern, i), pattern, i)) {
			if (IS_DUMMY(pattern)) {
				if ((rand() % 2) == 0) {
					continue;
				}
				setDummyStep(pattern, step,
				  !IS_SET(step, TYPE(pattern)), lockContext, NULL);
				continue;
			}
			if (!anyChildStepSet(pattern, i)) {
				nrValues++;
			}
			uint32_t idx = (rand() % nrValues);
			value = g_slist_nth(VALUES(pattern), idx);
			if (value == VALUE(step, TYPE(pattern))) {
				continue;
			}
			if (IS_CONTROLLER(pattern)) {
				setControllerStep(pattern,
				  step, value, i, lockContext, NULL);
				continue;
			}
			setNoteStep(pattern, step, value, i, lockContext, TRUE, NULL);

		} else if (!IS_NOTE(pattern)) {
			continue;
		}
		
		if (VALUE(step, TYPE(pattern)) == NULL) {
			continue;
		}
		if (i >= lastUserstep) {
			continue;
		}
		noteUserStep = USERSTEP_AT(pattern, i);
		if (noteUserStep->slideLocked) {
			continue;
		}
		slide = ((rand() % 2) == 0);
		if (HAS_SLIDE(step, TYPE(pattern)) == slide) {
			continue;
		}
		setSlide(pattern, step, slide, i, lockContext, TRUE, NULL);
	}
}

gboolean getLocked(gboolean *unlockable,
  void *step, pattern_t *pattern, uint32_t idx)
{
	gboolean result = FALSE;

	if ((!IS_SET(step, TYPE(pattern)))&&(!(IS_ROOT(pattern)))) {
		result = !IS_SET(PARENTSTEP(pattern, idx), TYPE(PARENT(pattern)));
	}
	if ((!result)&&(IS_SET(step, TYPE(pattern))&&(NR_VALUES(pattern) < 2))) {
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
