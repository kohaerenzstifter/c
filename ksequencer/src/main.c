#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#include "ksequencer.h"
#include "gui.h"

DECLARE_LOCKCONTEXT

static void getMidiPorts(err_t *e)
{
	gboolean locked = FALSE;
	snd_seq_client_info_t *snd_seq_client_info = NULL;
	snd_seq_port_info_t *snd_seq_port_info = NULL;
	uint32_t locks = LOCK_SEQUENCER;
	int client = -1;

	terror(getLocks(&lockContext, locks, e))
	locked = TRUE;
	snd_seq_client_info_malloc(&snd_seq_client_info);
	terror(failIfFalse((snd_seq_client_info != NULL)))
	snd_seq_port_info_malloc(&snd_seq_port_info);
	terror(failIfFalse((snd_seq_port_info != NULL)))
	snd_seq_client_info_set_client(snd_seq_client_info, -1);

	while (snd_seq_query_next_client((sequencer.snd_seq),
	  snd_seq_client_info) >= 0) {
		if (snd_seq_client_info_get_type(snd_seq_client_info)
		  != SND_SEQ_KERNEL_CLIENT) {
			continue;
		}

		client = snd_seq_client_info_get_client(snd_seq_client_info);

		snd_seq_port_info_set_client(snd_seq_port_info, client);
		snd_seq_port_info_set_port(snd_seq_port_info, -1);
		while (snd_seq_query_next_port((sequencer.snd_seq),
		  snd_seq_port_info) >= 0) {
			midiPort_t *midiPort = NULL;

			if (!(snd_seq_port_info_get_type(snd_seq_port_info)
			  & SND_SEQ_PORT_TYPE_MIDI_GENERIC)) {
				continue;
			}
			if ((snd_seq_port_info_get_capability(snd_seq_port_info)
			  & (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE))
			  != (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE)) {
				continue;
			}

			midiPort = calloc(1, sizeof(midiPort_t));

			midiPort->client = snd_seq_port_info_get_client(snd_seq_port_info);
			midiPort->port = snd_seq_port_info_get_port(snd_seq_port_info);
			midiPort->clientName =
			  strdup(snd_seq_client_info_get_name(snd_seq_client_info));
			midiPort->portName =
			  strdup(snd_seq_port_info_get_name(snd_seq_port_info));

			midiPorts = g_slist_append(midiPorts, midiPort);
		}
	}

finish:
	if (locked) {
		releaseLocks(&lockContext, locks, NULL);
	}
	if (snd_seq_port_info != NULL) {
		snd_seq_port_info_free(snd_seq_port_info);
	}
	if (snd_seq_client_info != NULL) {
		snd_seq_client_info_free(snd_seq_client_info);
	}
}

static void cbFreeMidiPort(gpointer data, gpointer user_data)
{
	midiPort_t *midiPort = data;

	free(midiPort->clientName);
	free(midiPort->portName);

	free(midiPort);
}

static void freeMidiPorts()
{
	g_slist_foreach((GSList *) midiPorts, cbFreeMidiPort, NULL);
	g_slist_free(midiPorts);
}

static void initialiseMutex(mutex_t *mutex, err_t *e)
{
	terror(failIfFalse((pthread_mutex_init(&(mutex->value), NULL) == 0)))
	mutex->initialised = TRUE;

finish:
	return;
}

static void destroyMutex(mutex_t *mutex)
{
	if (!(mutex->initialised)) {
		goto finish;
	}

	pthread_mutex_destroy(&(mutex->value));

finish:
	return;
}

static void setupSynchronisation(err_t *e)
{
	pthread_cond_init(&(synchronisation.wakeupConsumers), NULL);
#if 0
	terror(resetSynchronisation(&lockContext, e))

finish:
	return;
#endif
}

static void setupSequencer(err_t *e)
{
	terror(failIfFalse(snd_seq_open(&(sequencer.snd_seq),
	  "default", SND_SEQ_OPEN_DUPLEX, 0) == 0))
	snd_seq_nonblock(sequencer.snd_seq, 1);

	terror(failIfFalse((sequencer.myPort =
	  snd_seq_create_simple_port(sequencer.snd_seq, "ksequencerd",
	  SND_SEQ_PORT_CAP_WRITE |
	  SND_SEQ_PORT_CAP_SUBS_WRITE |
	  SND_SEQ_PORT_CAP_READ |
	  SND_SEQ_PORT_CAP_SUBS_READ,
	  SND_SEQ_PORT_TYPE_MIDI_GENERIC |
	  SND_SEQ_PORT_TYPE_APPLICATION)) >= 0))

	terror(getMidiPorts(e))

finish:
	return;
}

static void teardownSynchronisation(void)
{
	;
}

static void teardownSequencer(void)
{
	freeMidiPorts();
	if (sequencer.myPort >= 0) {
		snd_seq_delete_simple_port(sequencer.snd_seq, sequencer.myPort);
	}
	if (sequencer.snd_seq != NULL) {
		snd_seq_close(sequencer.snd_seq);
	}
}

static void setupPatterns(err_t *e)
{
	terror(failIfFalse(((patterns.root = allocatePattern(NULL)) != NULL)))

	TYPE(patterns.root) = patternTypeDummy;
	NAME(patterns.root) = strdup("<TOP>");
	NR_BARS(patterns.root) = 1;
	NR_STEPS_PER_BAR(patterns.root) = 1;
	
	setSteps(((pattern_t *) patterns.root));
	terror(setDummyStep((pattern_t *) patterns.root,
	  USERSTEP_AT(patterns.root, 0), TRUE, &lockContext, e))

finish:
	return;
}

static void teardownPatterns(void)
{
	freePattern(((pattern_t *) patterns.root));
}

static void stopThreads(void)
{
	void *value = NULL;

	goingDown = TRUE;

	pthread_join(threads.output, &value);
	pthread_join(threads.input, &value);
}

static void startThreads(err_t *e)
{
#ifdef SET_SCHEDULING_PRIORITY
	struct sched_param param;
	int policy = SCHED_RR;

	memset(&param, 0, sizeof(param));

	param.sched_priority = sched_get_priority_max(policy);
#endif

	terror(failIfFalse((pthread_create(&(threads.output),
	  NULL, output, NULL) == 0)))
	terror(failIfFalse((pthread_create(&(threads.input),
	  NULL, input, NULL) == 0)))

#ifdef SET_SCHEDULING_PRIORITY
	terror(failIfFalse((pthread_setschedparam(threads.output, policy, &param) == 0)))
	terror(failIfFalse((pthread_setschedparam(threads.input, policy, &param) == 0)))
#endif
finish:
	return;
}

static void setup(err_t *e)
{
	int pipefd[] = { -1, -1 };
	int flags = 0;

	for (uint32_t i = 0; i < NR_MUTEXES; i++) {
		terror(initialiseMutex(&(mutexes.value[i]), e))
	}

	terror(setupPatterns(e))
	terror(setupSequencer(e))
	terror(setupSynchronisation(e))
	terror(failIfFalse((pipe(pipefd) == 0)))
	terror(failIfFalse(((flags = fcntl(pipefd[PIPE_WR], F_GETFL)) != -1)))
	terror(failIfFalse(((fcntl(pipefd[PIPE_WR], F_SETFL,
	  (flags | O_NONBLOCK)) != -1))))
	terminate.pipe[PIPE_RD] = pipefd[PIPE_RD];
	pipefd[PIPE_RD] = -1;
	terminate.pipe[PIPE_WR] = pipefd[PIPE_WR];
	pipefd[PIPE_WR] = -1;
	terror(startThreads(e))

finish:
	if (pipefd[PIPE_RD] >= 0) {
		close(pipefd[PIPE_RD]);
	}
	if (pipefd[PIPE_WR] >= 0) {
		close(pipefd[PIPE_WR]);
	}
}

static void teardown(void)
{
	stopThreads();

	if (terminate.pipe[PIPE_RD] >= 0) {
		close(terminate.pipe[PIPE_RD]);
		terminate.pipe[PIPE_RD] = -1;
	}
	if (terminate.pipe[PIPE_WR] >= 0) {
		close(terminate.pipe[PIPE_WR]);
		terminate.pipe[PIPE_WR] = -1;
	}
	teardownSynchronisation();
	teardownSequencer();
	teardownPatterns();

	for (uint32_t i = 0; i < NR_MUTEXES; i++) {
		destroyMutex(&(mutexes.value[i]));
	}
}

static void handleSignal(int signum)
{
	terminate.value = TRUE;

	if (terminate.pipe[PIPE_WR] >= 0) {
		while (write(terminate.pipe[PIPE_WR], &signum,
		  sizeof(signum)) != sizeof(signum));
	}
}

static void doSignals(err_t *e)
{
	sigset_t signals;
	struct sigaction sa;

	terror(failIfFalse(sigfillset(&signals) == 0))
	terror(failIfFalse(sigdelset(&signals, SIGTERM) == 0))
	terror(failIfFalse(sigdelset(&signals, SIGINT) == 0))
	terror(failIfFalse(sigprocmask(SIG_SETMASK, &signals, NULL) == 0))

	sa.sa_handler = handleSignal;
	sa.sa_mask = signals;
	sa.sa_flags = 0;

	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	
finish:
	return;	
}

int main(int argc, char **argv)
{
	int exitStatus = EXIT_FAILURE;
	err_t error;
	err_t *e = &error;

	initErr(e);

	terror(doSignals(e))
	terror(setup(e))
	//terror(test())
	terror(gui(argc, argv, e))
	exitStatus = EXIT_SUCCESS;

finish:
	teardown();
	exit(exitStatus);
}
