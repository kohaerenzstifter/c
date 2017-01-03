#include "gtkSequencer.h"


static volatile gboolean terminate = FALSE;
static int signalPipe[2];


static void setupRootPattern()
{
	rootPattern.isRoot = TRUE;
	rootPattern.children = NULL;
}

static void teardownPattern(pattern_t *pattern)
{
	//TODO
}

static void setupSequencer(char *portString, err_t *e)
{
	gboolean ok = FALSE;

	terror(failIfFalse(snd_seq_open(&(sequencer.value),
	  "default", SND_SEQ_OPEN_DUPLEX, 0) == 0))
	snd_seq_nonblock(sequencer.value, 1);

	terror(failIfFalse(snd_seq_parse_address(sequencer.value,
	  &port, portString) == 0))

	snd_seq_create_simple_port(sequencer.value, "ksequencerd",
					 SND_SEQ_PORT_CAP_WRITE |
					 SND_SEQ_PORT_CAP_SUBS_WRITE,
					 SND_SEQ_PORT_TYPE_MIDI_GENERIC |
					 SND_SEQ_PORT_TYPE_APPLICATION);
	snd_seq_connect_from(sequencer.value, 0, port.client, port.port);
	snd_seq_connect_to(sequencer.value, 0, port.client, port.port);

	ok = TRUE;
finish:
	if (!ok) {
		snd_seq_close(sequencer.value);
	}
}

static void teardownSequencer(void)
{
	if (sequencer.value != NULL) {
		snd_seq_close(sequencer.value);
	}
}

static void setupSyncEvents(void)
{
	syncEvents.queue.head = syncEvents.queue.tail =
	  syncEvents.queue.last = NULL;
	pthread_cond_init(((pthread_cond_t *) &(syncEvents.wakeupConsumers)), NULL);
	pthread_mutex_init(((pthread_mutex_t *) &(syncEvents.mutex)), NULL);
}

static void setupNotesOff(void)
{
	notesOff.value = NULL;
}

static void teardownNotesOff(void)
{
	g_slist_free((GSList *) notesOff.value);
}

static void teardownSyncEvents(void)
{
	void freeSyncEvents(syncEvent_t *syncEvent) {
		if ((syncEvent != NULL)&&(syncEvent->next != NULL)) {
			freeSyncEvents((syncEvent_t *) syncEvent->next);
		}
		free(syncEvent);
	}

	freeSyncEvents((syncEvent_t *) syncEvents.queue.head);
	pthread_cond_destroy(((pthread_cond_t *) &(syncEvents.wakeupConsumers)));
	pthread_mutex_destroy(((pthread_mutex_t *) &(syncEvents.mutex)));
}

static void startThreads(err_t *e)
{
	struct sched_param param;
	int policy = SCHED_RR;

	memset(&param, 0, sizeof(param));

	param.sched_priority = sched_get_priority_max(policy);

	terror(failIfFalse((pthread_create(&(threads.output), NULL, outputFunction, NULL) == 0)))
	terror(failIfFalse((pthread_create(&(threads.input), NULL, inputFunction, NULL) == 0)))

//	terror(failIfFalse((pthread_setschedparam(threads.output, policy, &param) == 0)))
//	terror(failIfFalse((pthread_setschedparam(threads.input, policy, &param) == 0)))

finish:
	return;
}

static void stopThreads(void)
{
	void *value = NULL;

	goingDown = TRUE;

	pthread_join(threads.output, &value);
	pthread_join(threads.input, &value);

}

static void setup(char *portString, err_t *e)
{

    pthread_mutex_init(&mutex, NULL);
	setupNotesOff();
	terror(setupSyncEvents())
	terror(setupSequencer(portString, e))
	gtkInit();

	setupRootPattern();

	terror(startThreads(e))
finish:
	return;
}

static void teardown(void)
{
	stopThreads();

	teardownPattern(((pattern_t *) &rootPattern));

	//TODO

	teardownSequencer();
	teardownSyncEvents();
	teardownNotesOff();
	pthread_mutex_destroy(&mutex);
}

static void handleSignal(int signum)
{
	terminate = TRUE;
	write(signalPipe[1], &signum, sizeof(int));
}

int main(int argc, char *argv[])
{

	GIOChannel *gSignalIn = NULL;

	gboolean deliverSignal(GIOChannel *source, GIOCondition cond, gpointer d) {
		gchar buffer[sizeof(int)];
		gsize count = 0;

		while (g_io_channel_read_chars(source, buffer,
		  sizeof(buffer), &count, NULL) == G_IO_STATUS_NORMAL);

		gtkPrepareShutdown();
		gtk_main_quit();

		return TRUE;
	}

	void doSignals(void) {
		struct sigaction action;
		sigset_t signals;
		int fdFlags = 0;

		pipe(signalPipe);
		fdFlags = fcntl(signalPipe[1], F_GETFL);
		fcntl(signalPipe[1], F_SETFL, fdFlags | O_NONBLOCK);

		gSignalIn = g_io_channel_unix_new(signalPipe[0]);
		g_io_channel_set_flags(gSignalIn,
		  g_io_channel_get_flags(gSignalIn) | G_IO_FLAG_NONBLOCK, NULL);
		g_io_add_watch(gSignalIn, G_IO_IN | G_IO_PRI, deliverSignal, NULL);

		sigfillset(&signals);
		sigdelset(&signals, SIGTERM);
		sigdelset(&signals, SIGINT);
		sigprocmask(SIG_SETMASK, &signals, NULL);

		action.sa_handler = &handleSignal;
		action.sa_mask = signals;
		action.sa_flags = 0;

		sigaction(SIGTERM, &action, NULL);
		sigaction(SIGINT, &action, NULL);
	}


	GOptionContext *optionContext = NULL;
	GError *gerror = NULL;
	gchar *portString = NULL;

	err_t error;
	err_t *e = &error;

	GOptionEntry optionEntries[] = {
			{
					"port",
					'p',
					0,
					G_OPTION_ARG_STRING,
					&portString,
					"port",
					NULL
			},
			{
					NULL
			}
	};

	initErr(e);

	doSignals();

	optionContext = g_option_context_new("");
	g_option_context_add_main_entries(optionContext,
			optionEntries, NULL);
	terror(failIfFalse(g_option_context_parse(optionContext,
			&argc, &argv, &gerror)))

	terror(failIfFalse((portString != NULL)))

	memset(&sequencer, 0, sizeof(sequencer));
	memset(((void *) &syncEvents), 0, sizeof(syncEvents));
	memset(&threads, 0, sizeof(threads));
	memset(((void *) &notesOff), 0, sizeof(notesOff));


	terror(setup(portString, e))

	gtkFunction(argc, argv);
	//guiTest(100000000);

finish:
	teardown();
	if (gSignalIn != NULL) {
		g_io_channel_unref(gSignalIn);
	}
	g_free(portString);
	if (gerror != NULL) {
		g_error_free(gerror);
	}
	if (optionContext != NULL) {
		g_option_context_free(optionContext);
	}

	return EXIT_SUCCESS;
}