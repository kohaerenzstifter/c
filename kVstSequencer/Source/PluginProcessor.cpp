/* Copyright 2017 Martin Knappe (martin.knappe at gmail dot com) */

/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "kVstSequencer.h"

#define MILLION (1000000)
#define FACTOR ((MICROTICKS_PER_BAR < MILLION) ? \
  ((uint64_t ) (((double) MICROTICKS_PER_BAR) / ((double) MILLION))) : \
  ((uint64_t ) (((double) MILLION) / ((double) MICROTICKS_PER_BAR))))


static CriticalSection *criticalSection = NULL;

void lock(void)
{
	MARK();

	criticalSection->enter();
}

void unlock(void)
{
	MARK();

	criticalSection->exit();
}

static KVstSequencerAudioProcessorEditor
  *kVstSequencerAudioProcessorEditor = NULL;
static uint64_t nextEventStep = 0;
static gboolean isPlaying = FALSE;
static gboolean initialised = FALSE;

#define RESET_PATTERN() \
  do { \
	nextEventStep = 0; \
  } while (FALSE);

void performNoteEventStep(noteEventStep_t *noteEventStep,
  MidiBuffer& midiBuffer, int32_t *position, uint8_t channel)
{
	MARK();

	if (noteEventStep->offNoteEvent != NULL) {
		if (noteEventStep->onNoteEvent == NULL) {
			MidiMessage off =
			  MidiMessage::noteOff(channel,
			  MIDI_PITCH(noteEventStep->offNoteEvent));
			midiBuffer.addEvent(off, (*position)++);
		} else {
			noteEvent_t *noteEvent =
			  (noteEvent_t *) noteEventStep->offNoteEvent;
			midiMessage_t *midiMessage = getNoteOffMidiMessage(noteEvent);
			fireMidiMessage(midiMessage, NULL);
		}
		notesOff.value = g_slist_delete_link((GSList *) notesOff.value,
		  (GSList *) noteEventStep->offNoteEvent->off.noteOffLink);
		noteEventStep->offNoteEvent->off.noteOffLink = NULL;
	}

	if (noteEventStep->onNoteEvent != NULL) {
		MidiMessage on =
		  MidiMessage::noteOn(channel, MIDI_PITCH(noteEventStep->onNoteEvent),
		    ((juce::uint8) ((velocity * noteEventStep->onNoteEvent->on.velocity) / 127)));
		midiBuffer.addEvent(on, (*position)++);
		notesOff.value =
		  noteEventStep->onNoteEvent->on.offNoteEvent->off.noteOffLink =
		  g_slist_prepend(((GSList *) notesOff.value), (GSList *)
		  noteEventStep->onNoteEvent->on.offNoteEvent);
	}
}

void performControllerEventStep(pattern_t *pattern,
  controllerEventStep_t *controllerEventStep,
  MidiBuffer& midiBuffer, int32_t *position, uint8_t channel)
{
	MARK();

	if (controllerEventStep->controllerValue == NULL) {
		goto finish;
	}

	
	midiBuffer.addEvent(MidiMessage::controllerEvent(channel,
	  pattern->controller.parameter,
	  controllerEventStep->controllerValue->value), (*position)++);

finish:
	return;
}

void performStep(pattern_t *pattern, uint64_t eventStep,
  MidiBuffer& midiBuffer, int32_t *position)
{
	MARK();

	uint32_t factor = 0;
	uint32_t numberOfEventSteps = 0;
	uint32_t idx = 0;
	GSList *cur = NULL;
	uint32_t userStepsPerBar = 0;
	uint32_t eventStepsPerBar = 0;

	for (cur = (GSList *) pattern->children; cur != NULL;
	  cur = g_slist_next(cur)) {
		pattern_t *pattern = (pattern_t *) cur->data;
		performStep(pattern, eventStep, midiBuffer, position);
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
		performNoteEventStep(((noteEventStep_t *) EVENTSTEP_AT(pattern, idx)),
		  midiBuffer, position, CHANNEL(pattern));
	} else {
		performControllerEventStep(pattern, ((controllerEventStep_t *)
		  EVENTSTEP_AT(pattern, idx)), midiBuffer, position, CHANNEL(pattern));
	}
finish:
	return;
}

static int32_t nextPressedIdx = 0;
static int32_t pressed[20];

static void addNote(int32_t number)
{
	MARK();

	gboolean ignore = (nextPressedIdx >= ARRAY_SIZE(pressed));

	for (unsigned int i = 0; ((!ignore) && (i < nextPressedIdx)); i++) {
		if (pressed[i] == number) {
			ignore = TRUE;
			break;
		}
	}

	if (ignore) {
		goto finish;
	}

	pressed[nextPressedIdx++] = number;

finish:
	return;
}

static void removeNote(int32_t number)
{
	MARK();

	unsigned int i = 0;
	int32_t atIndex = -1;

	for (i = 0; i < nextPressedIdx; i++) {
		if (pressed[i] == number) {
			atIndex = i;
			break;
		}
	}

	if (atIndex < 0) {
		goto finish;
	}

	for (i = atIndex; i < (nextPressedIdx - 1); i++) {
		pressed[i] = pressed[i + 1];
	}

	nextPressedIdx--;

finish:
	return;
}

static gboolean setRoot(MidiBuffer& midiBuffer)
{
	MARK();

	MidiMessage msg;
	int32_t ignore = 0;
	pattern_t *root = ((pattern_t *) patterns.root);

	for (MidiBuffer::Iterator it(midiBuffer); it.getNextEvent(msg, ignore);) {
		if (msg.isNoteOn()) {
			addNote(msg.getNoteNumber());
			velocity = msg.getVelocity();
		} else if (msg.isNoteOff()) {
			removeNote(msg.getNoteNumber());
		}
	}

	patterns.root = NULL;

	if (nextPressedIdx > 0) {
		int32_t idx = pressed[(nextPressedIdx - 1)] - notes[0].midiValue;

		if (idx < ARRAY_SIZE(banks)) {
			patterns.root = banks[idx];
		}
	}

	return (root != patterns.root);
}

static void process(
  AudioPlayHead::CurrentPositionInfo *currentPositionInfo,
  MidiBuffer& midiBuffer,
  err_t *e)
{
	MARK();

	midiMessage_t *midiMessage = NULL;
	gboolean stopped = FALSE;
	gboolean locked = FALSE;
	volatile GSList *cur = NULL;
	int32_t position = 0;
	int32_t stepToSignal = -1;
	uint64_t atMicrotick = 0;
	double millionTimesNumerator =
	  (((double) MILLION) * ((double) currentPositionInfo->timeSigNumerator));

	lock();
	locked = TRUE;

	if (live) {
		if (setRoot(midiBuffer)) {
			terror(allNotesOff(e))
		}
	} else {
		nextPressedIdx = 0;
	}

	midiBuffer.clear();

	if (!currentPositionInfo->isPlaying) {
		if (isPlaying) {
			terror(allNotesOff(e))
			RESET_PATTERN();
			guiSignalStop();
		}
		stopped = TRUE;
	}

	for (cur = midiMessages; cur != NULL; cur = g_slist_next(cur)) {
		MidiMessage mm;
		midiMessage = (midiMessage_t *) cur->data;

		switch (midiMessage->midiMessageType) {
			case midiMessageTypeNoteOff:
				mm = MidiMessage::noteOff(midiMessage->channel, midiMessage->noteOff.noteNumber);
				break;
			case midiMessageTypeController:
				mm = MidiMessage::controllerEvent(midiMessage->channel,
				  midiMessage->controller.parameter,
				  midiMessage->controller.value);
			default:
				break;
		}

		free(midiMessage); midiMessage = NULL;
		midiBuffer.addEvent(mm, position++);
	}
	g_slist_free((GSList *) midiMessages); midiMessages = NULL;

	if (stopped) {
		goto finish;
	}

	atMicrotick =
	  (((double) currentPositionInfo->ppqPosition) * MILLION)
	  * (((double) MICROTICKS_PER_BAR) / millionTimesNumerator);
	if (!isPlaying) {
		nextEventStep = atMicrotick;
	}
	while (atMicrotick >= nextEventStep) {
		if (patterns.root != NULL) {
			performStep(((pattern_t *) patterns.root), nextEventStep,
			  midiBuffer, &position);
		}
		nextEventStep++;
	}
	stepToSignal = (nextEventStep - 1) % (MAX_BARS * MAX_EVENTSTEPS_PER_BAR);
	guiSignalStep(stepToSignal);

finish:
	isPlaying = currentPositionInfo->isPlaying;
	free(midiMessage);
	if (locked)  {
		unlock();
	}
}

static void setupPatterns(err_t *e)
{
	MARK();

	terror(failIfFalse(((patterns.root = allocatePattern(NULL)) != NULL)))

	TYPE(patterns.root) = patternTypeDummy;
	NAME(patterns.root) = strdup("<TOP>");
	NR_BARS(patterns.root) = 1;
	NR_STEPS_PER_BAR(patterns.root) = 1;
	
	setSteps(((pattern_t *) patterns.root));
	terror(setDummyStep((pattern_t *) patterns.root, ((dummyUserStep_t *)
	  USERSTEP_AT(patterns.root, 0)), TRUE, e))

finish:
	return;
}

static void setup(err_t *e)
{
	MARK();

	sigset_t mask;
	struct sigaction action;

	sigfillset(&mask);
	sigprocmask(SIG_SETMASK, &mask, NULL);

	action.sa_handler = SIG_IGN;

	sigaction(SIGILL, &action, NULL);
	sigaction(SIGTRAP, &action, NULL);
	sigaction(SIGABRT, &action, NULL);
	sigaction(SIGBUS, &action, NULL);
	sigaction(SIGFPE, &action, NULL);
	sigaction(SIGSEGV, &action, NULL);
	sigaction(SIGPIPE, &action, NULL);
	sigaction(SIGALRM, &action, NULL);
	sigaction(SIGCHLD, &action, NULL);
	sigaction(SIGTSTP, &action, NULL);
	sigaction(SIGTTIN, &action, NULL);
	sigaction(SIGTTOU, &action, NULL);
	sigaction(SIGPROF, &action, NULL);
	sigaction(SIGWINCH, &action, NULL);
	sigaction(SIGSYS, &action, NULL);

	criticalSection = new CriticalSection();

	terror(setupPatterns(e))

	initialised = TRUE;

finish:
	return;
}

//==============================================================================
KVstSequencerAudioProcessor::KVstSequencerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
	MARK();

#if 0
	char outbuffer[100];
	char errbuffer[100];

	snprintf(outbuffer, sizeof(outbuffer), "/tmp/kVstSequencer.%d.out", getpid());
	mkfifo(outbuffer, 0666);
	snprintf(errbuffer, sizeof(errbuffer), "/tmp/kVstSequencer.%d.err", getpid());
	mkfifo(errbuffer, 0666);
	setOutput(outbuffer, errbuffer, NULL);
#endif
	setOutput("/tmp/kVstSequencer.out", "/tmp/kVstSequencer.err", NULL);

	err_t err;
	err_t *e = &err;

	initErr(e);

	terror(setup(e))

finish:
	if (hasFailed(e)) {
		throw std::runtime_error(err2string(e));		
	}
	return;
}

static void teardownSynchronisation(void)
{
	MARK();

	initialised = FALSE;
}

static void teardownPatterns(void)
{
	MARK();

	if (patterns.root == NULL) {
		goto finish;
	}

	freePattern(((pattern_t *) patterns.root));
	patterns.root = NULL;

finish:
	return;
}

static void teardown(void)
{
	MARK();

	teardownSynchronisation();
	teardownPatterns();

	delete criticalSection;
}

KVstSequencerAudioProcessor::~KVstSequencerAudioProcessor()
{
	MARK();

	teardown();
}

//==============================================================================
const String KVstSequencerAudioProcessor::getName() const
{
	MARK();

    return JucePlugin_Name;
}

bool KVstSequencerAudioProcessor::acceptsMidi() const
{
	MARK();

   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool KVstSequencerAudioProcessor::producesMidi() const
{
	MARK();

   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

double KVstSequencerAudioProcessor::getTailLengthSeconds() const
{
	MARK();

    return 0;
}

int KVstSequencerAudioProcessor::getNumPrograms()
{
	MARK();

    return 1;
/*
	NB: some hosts don't cope very well if you tell them there are 0 programs,
	so this should be at least 1, even if you're not really implementing
	programs.
*/
}

int KVstSequencerAudioProcessor::getCurrentProgram()
{
	MARK();

    return 0;
}

void KVstSequencerAudioProcessor::setCurrentProgram (int index)
{
	MARK();
}

const String KVstSequencerAudioProcessor::getProgramName (int index)
{
	MARK();

    return String();
}

void KVstSequencerAudioProcessor::changeProgramName (int index,
  const String& newName)
{
	MARK();
}

//==============================================================================
void KVstSequencerAudioProcessor::prepareToPlay (double sampleRate,
  int samplesPerBlock)
{
	MARK();

    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void KVstSequencerAudioProcessor::releaseResources()
{
	MARK();

    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool KVstSequencerAudioProcessor::isBusesLayoutSupported
  (const BusesLayout& layouts) const
{
	MARK();

  #if JucePlugin_IsMidiEffect
    ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void KVstSequencerAudioProcessor::processBlock(AudioSampleBuffer& buffer,
  MidiBuffer& mm)
{
	MARK();

	MidiMessage midiMessage; 
	AudioPlayHead::CurrentPositionInfo currentPositionInfo;
	int position = 0;
	AudioPlayHead *audioPlayHead = NULL;
	err_t err;
	err_t *e = &err;

	initErr(e);

	if (((audioPlayHead = getPlayHead()) == NULL) ||
	  (!audioPlayHead->getCurrentPosition(currentPositionInfo))) {
		goto finish;
	}

	terror(process(&currentPositionInfo, mm, e))

finish:
	return;
}

//==============================================================================
bool KVstSequencerAudioProcessor::hasEditor() const
{
	MARK();

    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* KVstSequencerAudioProcessor::createEditor()
{
	MARK();

	kVstSequencerAudioProcessorEditor =
	  new KVstSequencerAudioProcessorEditor (*this);

    return kVstSequencerAudioProcessorEditor;
}


//==============================================================================
void KVstSequencerAudioProcessor::getStateInformation (MemoryBlock& destData)
{
	MARK();

    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
	uint32_t zero = 0;
	gboolean locked = FALSE;
	MemoryOutputStream *memoryOutputStream = NULL;
	err_t err;
	err_t *e = &err;

	initErr(e);

	memoryOutputStream = new MemoryOutputStream(destData, FALSE);

	lock();
	locked = TRUE;

	if (live) {
		terror(failIfFalse(memoryOutputStream->write(&zero, sizeof(zero))))
	} else {
		terror(loadStorePattern(((pattern_t **) &(patterns.root)),
		  memoryOutputStream, FALSE, (pattern_t *) NULL, e))
	}

	for (uint32_t i = 0; i < ARRAY_SIZE(banks); i++) {
		if (banks[i] == NULL) {
			terror(failIfFalse(memoryOutputStream->write(&zero, sizeof(zero))))
			continue;
		}
		terror(loadStorePattern(((pattern_t **) &banks[i]),
		  memoryOutputStream, FALSE, (pattern_t *) NULL, e))
	}

	memoryOutputStream->flush();

finish:
	if (locked)  {
		unlock();
	}
	if (memoryOutputStream != NULL) {
		delete memoryOutputStream;
	}
}

void KVstSequencerAudioProcessor::setStateInformation (const void* data,
  int sizeInBytes)
{
	MARK();
/*
	You should use this method to restore your parameters from this memory
	block, whose contents will have been created by the getStateInformation()
	call.
*/
	uint32_t length = 0;
	gboolean locked = FALSE;
	MemoryInputStream *memoryInputStream = NULL;
	pattern_t *pattern = NULL;
	err_t err;
	err_t *e = &err;

	initErr(e);

	memoryInputStream = new MemoryInputStream(data, sizeInBytes, FALSE);

	lock();
	locked = TRUE;

	terror(failIfFalse(memoryInputStream->read(&length, sizeof(length))
	  == sizeof(length)))
	if (length != ZERO) {
		terror(memoryInputStream->setPosition((memoryInputStream->getPosition()
		  - sizeof(length))))
		terror(loadStorePattern(((pattern_t **) &pattern),
		  memoryInputStream, TRUE, (pattern_t *) NULL, e))

		terror(setLive(pattern, e))
		pattern = NULL;
	} else {
		terror(setLive(NULL, e))
	}

	for (uint32_t i = 0; i < ARRAY_SIZE(banks); i++) {
		if (banks[i] != NULL) {
			freePattern(((pattern_t *) banks[i]));
			banks[i] = NULL;
		}
		terror(failIfFalse(memoryInputStream->read(&length, sizeof(length))
		  == sizeof(length)))
		if (length == ZERO) {
			continue;
		}
		terror(memoryInputStream->setPosition((
		  memoryInputStream->getPosition() - sizeof(length))))
		terror(loadStorePattern(((pattern_t **) &pattern),
		  memoryInputStream, TRUE, (pattern_t *) NULL, e))
		banks[i] = pattern;
		pattern = NULL;
	}

finish:
	if (locked)  {
		unlock();
	}
	if (memoryInputStream != NULL) {
		delete memoryInputStream;
	}
	if (pattern != NULL) {
		freePattern(pattern);
	}
 }

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	MARK();

	RESET_PATTERN();

    return new KVstSequencerAudioProcessor();
}
