#include <stdlib.h>

#include "ksequencer.h"

DECLARE_LOCKCONTEXT

void *input(void *param)
{
	int sndSeqEventInput(snd_seq_event_t **ev, err_t *e) {
		gboolean unlock = FALSE;
		int result = 0;
		uint32_t locks = LOCK_SEQUENCER;

		terror(getLocks(&lockContext, locks, e))
		unlock = TRUE;

		result = snd_seq_event_input(sequencer.snd_seq, ev);

finish:
		if (unlock) {
			releaseLocks(&lockContext, locks, NULL);
		}
		return result;
	}


	err_t error;
	err_t *e = &error;
	gboolean haveFirstEvent = FALSE;
	gboolean ticking = FALSE;
	uint64_t lastTickAt = 0;

	void addSyncEvent(gpointer data, gpointer user_data) {
		gboolean unlock = FALSE;
		uint64_t at = 0;
		struct timespec now;
		uint32_t locks = LOCK_SYNCHRONISATION;
		snd_seq_event_t *ev = data;

		terror(failIfFalse((clock_gettime(CLOCK_MONOTONIC_RAW, &now) == 0)))
		if (!haveFirstEvent) {
			subtractSeconds = now.tv_sec;
			haveFirstEvent = TRUE;
		}
		at = ((now.tv_sec - subtractSeconds) * 1000000000) + now.tv_nsec;

		terror(getLocks(&lockContext, locks, e))
		unlock = TRUE;

		synchronisation.synchronisationStatus.sequence++;
		switch (ev->type) {
			case SND_SEQ_EVENT_CLOCK:
				synchronisation.synchronisationStatus.lastTick.have = TRUE;
				synchronisation.synchronisationStatus.lastTick.at = at;
				if (ticking) {
					synchronisation.synchronisationStatus.lastTick.value++;
					synchronisation.synchronisationStatus.pace.have = TRUE;
					synchronisation.synchronisationStatus
					  .pace.nanosecondsPerTick = (at - lastTickAt);
				} else {
					synchronisation.synchronisationStatus.lastTick.value = 0;
				}
				ticking = TRUE;
				lastTickAt = at;
				break;
			case SND_SEQ_EVENT_START:
			case SND_SEQ_EVENT_STOP:
				ticking = FALSE;
				synchronisation.synchronisationStatus.lastTick.have = FALSE;
				synchronisation.synchronisationStatus.pace.have = FALSE;
		}
		releaseLocks(&lockContext, locks, NULL);
		pthread_cond_broadcast((&(synchronisation.wakeupConsumers)));
		unlock = FALSE;

finish:
		if (unlock) {
			releaseLocks(&lockContext, locks, NULL);
		}
	}

	GSList *events = NULL;
	snd_seq_event_t *ev = NULL;
	snd_seq_event_t *snd_seq_event = NULL;
	struct pollfd *pollfds = NULL;
	nfds_t nfds = 0;

	initErr(e);

	nfds = snd_seq_poll_descriptors_count(sequencer.snd_seq, POLLIN);
	pollfds = malloc(sizeof(struct pollfd) * nfds);
	snd_seq_poll_descriptors(sequencer.snd_seq, pollfds, nfds, POLLIN);

	while (!goingDown) {
		int pollResult = poll(pollfds, nfds, 1000);

		if (pollResult < 0) {
#define VALID_POLL_ERROR(e) (e == EINTR)
			terror(failIfFalse((VALID_POLL_ERROR(errno))))
		}

		do {
			int res = 0;

			terror(res = sndSeqEventInput(&ev, e))
			if (res < 0) {
				break;
			}
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
		} while (TRUE);
		g_slist_foreach(events, addSyncEvent, NULL);
		g_slist_free_full(events, free); events = NULL;
	};
finish:
	free(snd_seq_event);
	g_slist_free_full(events, free);
	free(pollfds);

	return NULL;
}
