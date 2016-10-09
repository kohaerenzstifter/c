#include "gtkSequencer.h"

/*static void printNotesOff(char *addDelete, noteEvent_t *noteEvent)
{
	printf("after %s %p: ", addDelete, noteEvent);
	for (GSList *cur = (GSList *) notesOff.value; cur != NULL; cur = g_slist_next(cur)) {
		noteEvent = cur->data;
		printf("%p ", noteEvent);
	}
	printf("\n");
}*/

void *outputFunction(void *param)
{
	err_t error;
	err_t *e = &error;
	gboolean adjustSleep = FALSE;
	uint64_t afterNanoseconds = 0;
	int64_t nanosecondsToSleep = 0;
	uint64_t beforeNanoseconds = 0;

	syncEvent_t *doRemoveSyncEvent(gboolean maySleep, gboolean getLock) {
		syncEvent_t *result = NULL;
		gboolean unlock = FALSE;
		struct timespec timespec;

		if (getLock) {
			pthread_mutex_lock(((pthread_mutex_t *) &(syncEvents.mutex))); unlock = TRUE;
		}

		result = (syncEvents.queue.last != NULL) ?
		  (syncEvent_t *) syncEvents.queue.last->next :
		  (syncEvent_t *) syncEvents.queue.head;
		if (result == NULL) {
			if (maySleep) {
				if (adjustSleep) {
					adjustSleep = FALSE;
					terror(failIfFalse((clock_gettime(CLOCK_MONOTONIC_RAW,
					  &timespec) == 0)))
					afterNanoseconds = ((timespec.tv_sec
					  - subtractSeconds) * 1000000000) + timespec.tv_nsec;
					nanosecondsToSleep -=
					  (afterNanoseconds - beforeNanoseconds);
#define MAXNANOSECONDSTOSLEEP 1000000000
					if (nanosecondsToSleep > MAXNANOSECONDSTOSLEEP) {
						nanosecondsToSleep = MAXNANOSECONDSTOSLEEP;
					}
				} else {
					nanosecondsToSleep = MAXNANOSECONDSTOSLEEP;
				}
				terror(failIfFalse((clock_gettime(CLOCK_REALTIME,
				  &timespec) == 0)))
				nanosecondsToSleep += (timespec.tv_sec * 1000000000);
				nanosecondsToSleep += timespec.tv_nsec;
				nanosecondsToSleep -= adjustment;
				if (nanosecondsToSleep > 0) {
					timespec.tv_sec = nanosecondsToSleep / 1000000000;
					timespec.tv_nsec = nanosecondsToSleep % 1000000000;
					pthread_cond_timedwait(((pthread_cond_t *) &(syncEvents.wakeupConsumers)),
					  ((pthread_mutex_t *) &(syncEvents.mutex)), &timespec);
				}
				result = doRemoveSyncEvent(FALSE, FALSE);
			}
			goto finish;
		}

		if (syncEvents.queue.last != NULL) {
			syncEvents.queue.head = syncEvents.queue.last->next;
			if (syncEvents.queue.head == NULL) {
				syncEvents.queue.tail = NULL;
			}
			free((syncEvent_t *) syncEvents.queue.last);
		}
		syncEvents.queue.last = result;

finish:
		if (unlock) {
			pthread_mutex_unlock(((pthread_mutex_t *) &(syncEvents.mutex)));
		}
		return result;
	}

	void performNoteEventStep(noteEventStep_t *noteEventStep) {

		if ((noteEventStep->onNoteEvent == NULL)&&(noteEventStep->offNoteEvent == NULL)) {
			goto finish;
		}

		if (noteEventStep->offNoteEvent != NULL) {
		    if (pendingOff != NULL) {
				noteValue_t *noteValue = (noteValue_t *) pendingOff->noteValue;
				snd_seq_event_t *snd_seq_event = (snd_seq_event_t *) noteValue->snd_seq_event;
				snd_seq_event->type = SND_SEQ_EVENT_NOTEOFF;
			    snd_seq_event_output(sequencer.value,
				  (snd_seq_event_t *) snd_seq_event);
			    notesOff.value =
			      (GSList *) g_slist_delete_link((GSList *) notesOff.value,
				  (GSList *) pendingOff->off.noteOffLink);
				//printNotesOff("delete", (noteEvent_t *) pendingOff);
			    pendingOff->off.noteOffLink = NULL;
			    pendingOff = NULL;
		    } 
		    if (noteEventStep->offNoteEvent->off.noteOffLink != NULL) {
				if (noteEventStep->onNoteEvent == NULL) {
					noteEventStep->offNoteEvent->noteValue->snd_seq_event->type = SND_SEQ_EVENT_NOTEOFF;
					snd_seq_event_output(sequencer.value,
					  (snd_seq_event_t *) noteEventStep->offNoteEvent->noteValue->snd_seq_event);
					notesOff.value =
					g_slist_delete_link((GSList *) notesOff.value,
					  (GSList *) noteEventStep->offNoteEvent->off.noteOffLink);
					//printNotesOff("delete", (noteEvent_t *) noteEventStep->offNoteEvent);
					noteEventStep->offNoteEvent->off.noteOffLink = NULL;
				} else {
					pendingOff = (struct noteEvent *) noteEventStep->offNoteEvent;
				}
			}
		}
		if (noteEventStep->onNoteEvent != NULL) {
			noteEventStep->onNoteEvent->noteValue->snd_seq_event->type = SND_SEQ_EVENT_NOTEON;
			snd_seq_event_output(sequencer.value,
			  (snd_seq_event_t *) noteEventStep->onNoteEvent->noteValue->snd_seq_event);
			notesOff.value = noteEventStep->onNoteEvent->on.offNoteEvent->off.noteOffLink =
			  g_slist_prepend((GSList *) notesOff.value, (GSList *) noteEventStep->onNoteEvent->on.offNoteEvent);
			//printNotesOff("add", (noteEvent_t *) noteEventStep->onNoteEvent->on.offNoteEvent);
		}
	
finish:
	    return;
	}

	void performControllerEventStep(controllerEventStep_t *controllerEventStep) {

		if (controllerEventStep->value == NULL) {
			goto finish;
		}

		snd_seq_event_output(sequencer.value, (snd_seq_event_t *) controllerEventStep->value);

finish:
		return;
	}

	void performStep(pattern_t *pattern, uint64_t eventStep) {
		uint32_t factor = 0;
		uint32_t numberOfEventSteps = 0;
		uint32_t idx = 0;
		uint32_t bars = pattern->isRoot ? 1 :
		  pattern->real.bars;
		GSList *cur = NULL;
		uint32_t userStepsPerBar = 0;
		uint32_t eventStepsPerBar = 0;

		for (cur = (GSList *) pattern->children; cur != NULL; cur = g_slist_next(cur)) {
			pattern_t *pattern = cur->data;
			performStep(pattern, eventStep);
		}

		if (pattern->isRoot) {
			goto finish;
		}

		if (pattern->real.type == PATTERNTYPE_DUMMY) {
			goto finish;
		}

		userStepsPerBar = pattern->real.userStepsPerBar;
		eventStepsPerBar = (userStepsPerBar * (EVENTSTEPS_PER_USERSTEP(pattern)));

		factor = MAX_EVENTSTEPS_PER_BAR / eventStepsPerBar;

		if ((eventStep % factor) != 0) {
			goto finish;
		}

		numberOfEventSteps = eventStepsPerBar * bars;
		idx = (eventStep / factor) % numberOfEventSteps;

		if (pattern->real.type == PATTERNTYPE_NOTE) {
			performNoteEventStep((noteEventStep_t *)
			  &(pattern->real.note.steps.event[idx]));
		} else {
			performControllerEventStep((controllerEventStep_t *)
			  &(pattern->real.controller.steps.event[idx]));
		}
finish:
		return;
	}

	void noteOff(gpointer data, gpointer user_data) {
		noteEvent_t *noteEvent = data;

		noteEvent->noteValue->snd_seq_event->type = SND_SEQ_EVENT_NOTEOFF; ////TODO: SEGFAULT
		snd_seq_event_output(sequencer.value,
		  (snd_seq_event_t *) noteEvent->noteValue->snd_seq_event);
		noteEvent->off.noteOffLink = NULL;
	}

	syncEvent_t *syncEvent;
	gboolean haveTick = FALSE;
	gboolean haveLastConfirmedTick = FALSE;
	gboolean havePace = FALSE;
	gboolean stopPattern = FALSE;
	uint64_t lastConfirmedTick = 0;
	uint64_t lastTickAt = 0;
	uint64_t nanosecondsPerMicrotick = 0;
	uint64_t nanosecondsPerTick = 0;
	uint64_t nanosecondsPerCrotchet = 0;
	uint64_t overdueThreshold = 0;
	uint64_t atMicrotick = 0;
	uint64_t nanosecondsSinceLastTick = 0;
	uint64_t nextEventStep = 0;
	struct timespec timespec;

	initErr(e);

#define UNSET_PACE() \
  do { \
	havePace = FALSE; \
  } while (FALSE);

#define RESET_PATTERN() \
  do { \
	UNSET_PACE(); \
	haveLastConfirmedTick = FALSE; \
	atMicrotick = nextEventStep = 0; \
  } while (FALSE);

	RESET_PATTERN();

	do {
#define removeSyncEvent() doRemoveSyncEvent(TRUE, TRUE)
		if ((syncEvent = removeSyncEvent()) == NULL) {
			if (goingDown) {
				break;
			}
		}
		haveTick = FALSE;
		stopPattern = FALSE;

		if (syncEvent != NULL) {
			if (syncEvent->syncEventType == SYNCEVENTTYPE_TICK) {

				haveTick = TRUE;
				if (haveLastConfirmedTick) {
					havePace = TRUE;
					nanosecondsPerTick = syncEvent->at - lastTickAt;
					nanosecondsPerMicrotick =
					  (nanosecondsPerTick * TICKS_PER_BAR) / MICROTICKS_PER_BAR;
					overdueThreshold = 2 * nanosecondsPerTick;
					lastConfirmedTick++;
					nanosecondsPerCrotchet += nanosecondsPerTick;
					if ((lastConfirmedTick % TICKS_PER_CROTCHET) == 0) {
						gtkSignalSpeed(nanosecondsPerCrotchet);
						nanosecondsPerCrotchet = 0;
					}
				} else {
					haveLastConfirmedTick = TRUE;
					lastConfirmedTick = 0;
					nanosecondsPerCrotchet = 0;
				}
				lastTickAt = syncEvent->at;
			} else /* if (syncEvent->type == EVENTTYPE_STOP) */ {
				gtkSignalSpeed(0);
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
		  * 1000000000) + timespec.tv_nsec;
		nanosecondsSinceLastTick = beforeNanoseconds - lastTickAt;

		if (havePace) {
			uint32_t adj = 0;
			if (!haveTick) {
				adj = adjustment;
			}
			atMicrotick =
			  ((lastConfirmedTick * nanosecondsPerTick) / nanosecondsPerMicrotick)
			  + ((nanosecondsSinceLastTick + adj) / nanosecondsPerMicrotick);
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

#define NOTES_OFF() \
	do { \
		pthread_mutex_lock(&mutex); \
		g_slist_foreach((GSList *) notesOff.value, noteOff, NULL); \
		snd_seq_drain_output(sequencer.value); \
		g_slist_free((GSList *) notesOff.value); \
		notesOff.value = NULL; \
		pthread_mutex_unlock(&mutex); \
	} while (FALSE);

					NOTES_OFF();
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
			    pthread_mutex_lock(&mutex);
				performStep(((pattern_t *) &rootPattern), nextEventStep);
				//gtkSignalStep(nextEventStep);
				snd_seq_drain_output(sequencer.value);

		        pthread_mutex_unlock(&mutex);
				nextEventStep++;
			} else {
				break;
			}
		} while (TRUE);
	} while (TRUE);

finish:
	NOTES_OFF(); ////TODO: SEGFAULT
	return NULL;
}
