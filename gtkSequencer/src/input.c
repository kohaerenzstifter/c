#include "gtkSequencer.h"

void *inputFunction(void *param)
{
	syncEvent_t *getSyncEvent() {
		syncEvent_t *result = NULL;

		result = malloc(sizeof(syncEvent_t));
		result->next = NULL;

		return result;
	}

	void doAddSyncEvent(syncEvent_t *syncEvent) {
		if (syncEvents.queue.head == NULL) {
			syncEvents.queue.head = syncEvents.queue.tail = syncEvent;
		} else {
			syncEvents.queue.tail->next = syncEvent;
			syncEvents.queue.tail = syncEvents.queue.tail->next;
		}
	}

	int sndSeqEventInput(snd_seq_event_t **ev) {
		int result = 0;

		pthread_mutex_lock(&mutex);
		result = snd_seq_event_input(sequencer.value, ev);
		pthread_mutex_unlock(&mutex);

		return result;
	}


	err_t error;
	err_t *e = &error;
	gboolean haveFirstEvent = FALSE;
	gboolean ticking = FALSE;

	void addSyncEvent(gpointer data, gpointer user_data) {
		snd_seq_event_t *ev = data;
		struct timespec now;
		syncEvent_t *syncEvent = NULL;

		syncEvent = getSyncEvent();

		terror(failIfFalse((clock_gettime(CLOCK_MONOTONIC_RAW, &now) == 0)))
		if (!haveFirstEvent) {
			subtractSeconds = now.tv_sec;
			haveFirstEvent = TRUE;
		}
		syncEvent->at = ((now.tv_sec
		  - subtractSeconds) * 1000000000) + now.tv_nsec;

		switch (ev->type) {
			case SND_SEQ_EVENT_CLOCK:
				ticking = TRUE;
				syncEvent->syncEventType = SYNCEVENTTYPE_TICK;
				break;
			case SND_SEQ_EVENT_START:
			case SND_SEQ_EVENT_STOP:
				if (ticking) {
					ticking = FALSE;
					syncEvent->syncEventType = SYNCEVENTTYPE_STOP;
					break;
				}
			default:
				free(syncEvent); syncEvent = NULL;
		}
		if (syncEvent == NULL) {
			goto finish;
		}

		pthread_mutex_lock(((pthread_mutex_t *) &(syncEvents.mutex)));
		doAddSyncEvent(syncEvent);
		pthread_mutex_unlock(((pthread_mutex_t *) &(syncEvents.mutex)));
		pthread_cond_broadcast(((pthread_cond_t *) &(syncEvents.wakeupConsumers)));
finish:
		return;
	}

	GSList *events = NULL;
	snd_seq_event_t *ev = NULL;
	snd_seq_event_t *snd_seq_event = NULL;
	struct pollfd *pollfds = NULL;
	nfds_t nfds = 0;

	initErr(e);

	nfds = snd_seq_poll_descriptors_count(sequencer.value, POLLIN);
	pollfds = malloc(sizeof(struct pollfd) * nfds);
	snd_seq_poll_descriptors(sequencer.value, pollfds, nfds, POLLIN);

	while (!goingDown) {
		int pollResult = poll(pollfds, nfds, 1000);

		if (pollResult < 0) {
#define VALID_POLL_ERROR(e) (e == EINTR)
			terror(failIfFalse((VALID_POLL_ERROR(errno))))
		}

		while (sndSeqEventInput(&ev) >= 0) {
#define interesting(e) \
  ((e->type == SND_SEQ_EVENT_START) \
  || \
  (e->type == SND_SEQ_EVENT_STOP) \
  || \
  (e->type == SND_SEQ_EVENT_CLOCK))  
			if (!interesting(ev)) {
				continue;
			}

			snd_seq_event = malloc(sizeof(snd_seq_event_t));
			*snd_seq_event = *ev;
			events = g_slist_append(events, snd_seq_event); snd_seq_event = NULL;
		}
		g_slist_foreach(events, addSyncEvent, NULL);
		g_slist_free_full(events, free); events = NULL;
	};
finish:
	free(snd_seq_event);
	g_slist_free_full(events, free);
	free(pollfds);
	return NULL;
}