#include <stdlib.h>

#include "ksequencer.h"

DECLARE_LOCKCONTEXT

void *output(void *param)
{
	err_t error;
	err_t *e = &error;
	gboolean adjustSleep = FALSE;
	uint64_t afterNanoseconds = 0;
#define MAXNANOSECONDSTOSLEEPWHILEIDLE (1000000000ULL)
	int64_t nanosecondsToSleep = MAXNANOSECONDSTOSLEEPWHILEIDLE;
	uint64_t beforeNanoseconds = 0;

	synchronisationStatus_t *doRemoveSynchronisationStatus(gboolean maySleep,
	  gboolean getLock, uint64_t lastSequence) {
		synchronisationStatus_t *result = NULL;
		static synchronisationStatus_t copy;
		gboolean locked = FALSE;
		struct timespec timespec;

		if (getLock) {
			pthread_mutex_lock(&(mutexes.value[MUTEX_SYNCHRONISATION].value));
			locked = TRUE;
		}
		if (lastSequence < synchronisation.synchronisationStatus.sequence) {
			copy = synchronisation.synchronisationStatus;
			result = &copy;
			goto finish;
		}
		if (!maySleep) {
			goto finish;
		}
		if (adjustSleep) {
			adjustSleep = FALSE;
			terror(failIfFalse((clock_gettime(CLOCK_MONOTONIC_RAW,
			  &timespec) == 0)))
			afterNanoseconds = ((timespec.tv_sec
			  - subtractSeconds) * 1000000000ULL) + timespec.tv_nsec;
			nanosecondsToSleep -=
			  (afterNanoseconds - beforeNanoseconds);
		} else {
			nanosecondsToSleep = MAXNANOSECONDSTOSLEEPWHILEIDLE;
		}
		if (nanosecondsToSleep > 0) {
			terror(failIfFalse((clock_gettime(CLOCK_REALTIME,
			  &timespec) == 0)))
			nanosecondsToSleep += (timespec.tv_sec * 1000000000ULL);
			nanosecondsToSleep += timespec.tv_nsec;
			timespec.tv_sec = (nanosecondsToSleep / 1000000000ULL);
			timespec.tv_nsec = (nanosecondsToSleep % 1000000000ULL);
			pthread_cond_timedwait(((pthread_cond_t *)
			  &(synchronisation.wakeupConsumers)),
			  &(mutexes.value[MUTEX_SYNCHRONISATION].value), &timespec);
		}
		result = doRemoveSynchronisationStatus(FALSE, FALSE, lastSequence);

finish:
		if (locked) {
			pthread_mutex_unlock(&(mutexes.value[MUTEX_SYNCHRONISATION].value));
		}
		return result;
	}

	void performNoteEventStep(noteEventStep_t *noteEventStep) {

		if ((pendingOff != NULL)&&(pendingOff->off.noteOffLink != NULL)) {
			noteValue_t *noteValue = (noteValue_t *) pendingOff->noteValue;
			snd_seq_event_t *snd_seq_event = noteValue->snd_seq_event;
			snd_seq_event->dest = sequencer.snd_seq_addr;
			snd_seq_event->type = SND_SEQ_EVENT_NOTEOFF;
			snd_seq_event_output(sequencer.snd_seq, snd_seq_event);
			notesOff.value = g_slist_delete_link((GSList *) notesOff.value,
			  pendingOff->off.noteOffLink);
			pendingOff->off.noteOffLink = NULL;
			pendingOff = NULL;
		}

		if ((noteEventStep->offNoteEvent != NULL)&&
		  (noteEventStep->offNoteEvent->off.noteOffLink != NULL)) {
			if (noteEventStep->onNoteEvent == NULL) {
				noteEventStep->offNoteEvent->noteValue->snd_seq_event->dest =
				  sequencer.snd_seq_addr;
				noteEventStep->offNoteEvent->noteValue->snd_seq_event->type =
				  SND_SEQ_EVENT_NOTEOFF;
				snd_seq_event_output(sequencer.snd_seq,
				  noteEventStep->offNoteEvent->noteValue->snd_seq_event);
				notesOff.value = g_slist_delete_link((GSList *) notesOff.value,
				  noteEventStep->offNoteEvent->off.noteOffLink);
				noteEventStep->offNoteEvent->off.noteOffLink = NULL;
			} else {
				pendingOff = (struct noteEvent *) noteEventStep->offNoteEvent;
			}
		}

		if (noteEventStep->onNoteEvent != NULL) {
			noteEventStep->onNoteEvent->noteValue->snd_seq_event->dest =
			  sequencer.snd_seq_addr;
			noteEventStep->onNoteEvent->noteValue->snd_seq_event->type =
			  SND_SEQ_EVENT_NOTEON;
			snd_seq_event_output(sequencer.snd_seq,
			  noteEventStep->onNoteEvent->noteValue->snd_seq_event);
			notesOff.value =
			  noteEventStep->onNoteEvent->on.offNoteEvent->off.noteOffLink =
			  g_slist_prepend(((GSList *) notesOff.value),
			  noteEventStep->onNoteEvent->on.offNoteEvent);
		}
	}

	void performControllerEventStep(controllerEventStep_t
	  *controllerEventStep) {

		if (controllerEventStep->controllerValue == NULL) {
			goto finish;
		}

		controllerEventStep->controllerValue->snd_seq_event->dest =
			  sequencer.snd_seq_addr;
		snd_seq_event_output(sequencer.snd_seq,
		  controllerEventStep->controllerValue->snd_seq_event);

finish:
		return;
	}

	void performStep(pattern_t *pattern, uint64_t eventStep) {
		uint32_t factor = 0;
		uint32_t numberOfEventSteps = 0;
		uint32_t idx = 0;
		GSList *cur = NULL;
		uint32_t userStepsPerBar = 0;
		uint32_t eventStepsPerBar = 0;

		if (!sequencer.connected) {
			goto finish;
		}

		for (cur = (GSList *) pattern->children; cur != NULL;
		  cur = g_slist_next(cur)) {
			pattern_t *pattern = cur->data;
			performStep(pattern, eventStep);
		}

		if (IS_ROOT(pattern)) {
			goto finish;
		}

		if (IS_DUMMY(pattern)) {
			goto finish;
		}

		userStepsPerBar = NR_USERSTEPS_PER_BAR(pattern);
		eventStepsPerBar =
		  (userStepsPerBar * (EVENTSTEPS_PER_USERSTEP(TYPE(pattern))));

		factor = MAX_EVENTSTEPS_PER_BAR / eventStepsPerBar;

		if ((eventStep % factor) != 0) {
			goto finish;
		}

		numberOfEventSteps = eventStepsPerBar * NR_BARS(pattern);
		idx = (eventStep / factor) % numberOfEventSteps;

		if (IS_NOTE(pattern)) {
			performNoteEventStep(EVENTSTEP_AT(pattern, idx));
		} else {
			performControllerEventStep(EVENTSTEP_AT(pattern, idx));
		}
finish:
		return;
	}

	synchronisationStatus_t *synchronisationStatus = NULL;
	gboolean haveTick = FALSE;
	gboolean havePace = FALSE;
	gboolean stopPattern = FALSE;
	uint32_t locks = LOCK_DATA | LOCK_SEQUENCER;
	uint64_t lastConfirmedTick = 0;
	uint64_t lastTickAt = 0;
	uint64_t nanosecondsPerMicrotick = 0;
	uint64_t nanosecondsPerTick = 0;
	uint64_t nanosecondsPerCrotchet = 0;
	uint64_t overdueThreshold = 0;
	uint64_t atMicrotick = 0;
	uint64_t nanosecondsSinceLastTick = 0;
	uint64_t nextEventStep = 0;
	uint64_t lastSequence = 0;
	struct timespec timespec;

	initErr(e);

#define UNSET_PACE() \
  do { \
	havePace = FALSE; \
  } while (FALSE);

#define RESET_PATTERN() \
  do { \
	UNSET_PACE(); \
	atMicrotick = nextEventStep = 0; \
  } while (FALSE);

	RESET_PATTERN();

	do {
#define removeSynchronisationStatus() \
  doRemoveSynchronisationStatus(TRUE, TRUE, lastSequence)
		if ((synchronisationStatus = removeSynchronisationStatus()) == NULL) {
			if (goingDown) {
				break;
			}
		}

		haveTick = FALSE;
		stopPattern = FALSE;

		if (synchronisationStatus != NULL) {
			lastSequence = synchronisationStatus->sequence;
			haveTick = synchronisationStatus->lastTick.have;
			if (haveTick) {
				lastTickAt = synchronisationStatus->lastTick.at;
				lastConfirmedTick = synchronisationStatus->lastTick.value;
				havePace = synchronisationStatus->pace.have;
				if (havePace) {
					nanosecondsPerTick =
					  synchronisationStatus->pace.nanosecondsPerTick;
					overdueThreshold = 2 * nanosecondsPerTick;
					nanosecondsPerMicrotick =
					  (nanosecondsPerTick * TICKS_PER_BAR) / MICROTICKS_PER_BAR;
					nanosecondsPerCrotchet += nanosecondsPerTick;
					if ((lastConfirmedTick % TICKS_PER_CROTCHET) == 0) {
						gtkSignalSpeed(nanosecondsPerCrotchet);
						nanosecondsPerCrotchet = 0;
					}
				} else {
					nanosecondsPerCrotchet = 0;
				}
			} else {
				stopPattern = TRUE;
			}
		}

		if (stopPattern) {
			gtkSignalStop();
		}
		if ((!stopPattern)&&(!haveTick)&&(!havePace)) {
			continue;
		}
		terror(failIfFalse((clock_gettime(CLOCK_MONOTONIC_RAW,
		  &timespec) == 0)))
		beforeNanoseconds = ((timespec.tv_sec - subtractSeconds)
		  * 1000000000ULL) + timespec.tv_nsec;
		nanosecondsSinceLastTick = beforeNanoseconds - lastTickAt;
		if (havePace) {
			atMicrotick = ((lastConfirmedTick *
			  nanosecondsPerTick) / nanosecondsPerMicrotick)
			  + (nanosecondsSinceLastTick / nanosecondsPerMicrotick);
		}
#define TICK_OVERDUE() \
  ((havePace)&&(nanosecondsSinceLastTick >= overdueThreshold))
		if ((!stopPattern)&&TICK_OVERDUE()) {
			UNSET_PACE();
			continue;
		}
		do {
			if (nextEventStep > atMicrotick) {
				if (stopPattern) {
					terror(getLocks(&lockContext, locks, e))
					terror(allNotesOff(&lockContext, TRUE, e))
					terror(releaseLocks(&lockContext, locks, e))
					RESET_PATTERN();
				} else if (havePace) {
					nanosecondsToSleep =
					  (nextEventStep - atMicrotick) * nanosecondsPerMicrotick;
					nanosecondsToSleep -=
					  nanosecondsSinceLastTick % nanosecondsPerMicrotick;
					adjustSleep = TRUE;
				}
				break;
			} else if (haveTick||havePace) {
				terror(getLocks(&lockContext, locks, e))
				performStep(((pattern_t *) patterns.root), nextEventStep);
				gtkSignalStep(nextEventStep);
				snd_seq_drain_output(sequencer.snd_seq);
				terror(releaseLocks(&lockContext, locks, e))
				nextEventStep++;
			} else {
				break;
			}
		} while (TRUE);
	} while (TRUE);

finish:
	allNotesOff(&lockContext, FALSE, NULL);
	return NULL;
}
