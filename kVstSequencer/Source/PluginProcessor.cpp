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

DECLARE_LOCKCONTEXT
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
  MidiBuffer& midiBuffer, int32_t *position)
{
	MARK();

	if (noteEventStep->offNoteEvent != NULL) {
		if (noteEventStep->onNoteEvent == NULL) {
			MidiMessage off =
			  MidiMessage::noteOff(0, MIDI_PITCH(noteEventStep->offNoteEvent));
			midiBuffer.addEvent(off, (*position)++);
		} else {
			noteEvent_t *noteEvent =
			  (noteEvent_t *) noteEventStep->offNoteEvent;
			midiMessage_t *midiMessage = getNoteOffMidiMessage(noteEvent);
			fireMidiMessage(&lockContext, midiMessage, NULL);
		}
		notesOff.value = g_slist_delete_link((GSList *) notesOff.value,
		  (GSList *) noteEventStep->offNoteEvent->off.noteOffLink);
		noteEventStep->offNoteEvent->off.noteOffLink = NULL;
	}

	if (noteEventStep->onNoteEvent != NULL) {
		MidiMessage on =
		  MidiMessage::noteOn(0, MIDI_PITCH(noteEventStep->onNoteEvent),
		    noteEventStep->onNoteEvent->on.velocity);
		midiBuffer.addEvent(on, (*position)++);
		notesOff.value =
		  noteEventStep->onNoteEvent->on.offNoteEvent->off.noteOffLink =
		  g_slist_prepend(((GSList *) notesOff.value), (GSList *)
		  noteEventStep->onNoteEvent->on.offNoteEvent);
	}
}

void performControllerEventStep(pattern_t *pattern,
  controllerEventStep_t *controllerEventStep,
  MidiBuffer& midiBuffer, int32_t *position)
{
	MARK();

	if (controllerEventStep->controllerValue == NULL) {
		goto finish;
	}

	
	midiBuffer.addEvent(MidiMessage::controllerEvent(0,
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
		  midiBuffer, position);
	} else {
		performControllerEventStep(pattern, ((controllerEventStep_t *)
		  EVENTSTEP_AT(pattern, idx)), midiBuffer, position);
	}
finish:
	return;
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
	uint32_t locks = LOCK_DATA | LOCK_SEQUENCER;
	double millionTimesNumerator =
	  (((double) MILLION) * ((double) currentPositionInfo->timeSigNumerator));

	terror(getLocks(&lockContext, locks, e))
	locked = TRUE;

	if (!currentPositionInfo->isPlaying) {
		if (isPlaying) {
			terror(allNotesOff(&lockContext, TRUE, e))
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
				mm = MidiMessage::noteOff(0, midiMessage->noteOff.noteNumber);
				break;
			case midiMessageTypeController:
				mm = MidiMessage::controllerEvent(0,
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
		performStep(((pattern_t *) patterns.root), nextEventStep,
		  midiBuffer, &position);
		nextEventStep++;
	}
	stepToSignal = (nextEventStep - 1) % (MAX_BARS * MAX_EVENTSTEPS_PER_BAR);
	guiSignalStep(stepToSignal);
finish:
	isPlaying = currentPositionInfo->isPlaying;
	free(midiMessage);
	if (locked)  {
		releaseLocks(&lockContext, locks, e);
	}
}

static void initialiseMutex(mutex_t *mutex, err_t *e)
{
	MARK();

	terror(failIfFalse((pthread_mutex_init(&(mutex->value), NULL) == 0)))
	mutex->initialised = TRUE;

finish:
	return;
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
	  USERSTEP_AT(patterns.root, 0)), TRUE, &lockContext, e))

finish:
	return;
}

static void setup(err_t *e)
{
	MARK();

	for (uint32_t i = 0; i < NR_MUTEXES; i++) {
		terror(initialiseMutex(&(mutexes.value[i]), e))
	}

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

#if 1
	char outbuffer[100];
	char errbuffer[100];

	snprintf(outbuffer, sizeof(outbuffer), "/tmp/kVstSequencer.%d.out", getpid());
	mkfifo(outbuffer, 0666);
	snprintf(errbuffer, sizeof(errbuffer), "/tmp/kVstSequencer.%d.err", getpid());
	mkfifo(errbuffer, 0666);
	setOutput(outbuffer, errbuffer, NULL);
#endif


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

static void destroyMutex(mutex_t *mutex)
{
	MARK();

	if (!(mutex->initialised)) {
		goto finish;
	}

	pthread_mutex_destroy(&(mutex->value));

	mutex->initialised = FALSE;

finish:
	return;
}

static void teardown(void)
{
	MARK();

	teardownSynchronisation();
	teardownPatterns();

	for (uint32_t i = 0; i < NR_MUTEXES; i++) {
		destroyMutex(&(mutexes.value[i]));
	}
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

    return 1.0;
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

void KVstSequencerAudioProcessor::processBlock (AudioSampleBuffer& buffer,
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

static void readWriteStream(void *data, uint32_t length, void *stream,
  gboolean reading, err_t *e)
{
	MARK();

	if (reading) {
		MemoryInputStream *memoryInputStream = (MemoryInputStream *) stream;
		terror(failIfFalse(memoryInputStream->read(data, length) == length))
	} else {
		MemoryOutputStream *memoryOutputStream = (MemoryOutputStream *) stream;
		terror(failIfFalse(memoryOutputStream->write(data, length)))
	}

finish:
	return;
}

static char *readStringFromStream(MemoryInputStream *memoryInputStream, err_t *e)
{
	MARK();

	size_t length = 0;
	char *result = NULL;
	char *_result = NULL;

	terror(readWriteStream(&length, sizeof(length), memoryInputStream, TRUE, e))
	_result = (char *) calloc(1, (length + 1));
	terror(readWriteStream(_result, length, memoryInputStream, TRUE, e))
	_result[length] = '\0';

	result = _result; _result = NULL;
finish:
	return result;
}

static void writeStringToStream(char *string,
  MemoryOutputStream *memoryOutputStream, err_t *e)
{
	MARK();

	size_t length = strlen(string);

	terror(readWriteStream(&length, sizeof(length),
	  memoryOutputStream, FALSE, e))
	terror(readWriteStream(string, length, memoryOutputStream, FALSE, e))

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
		  readStringFromStream(((MemoryInputStream *) stream), e))
	} else {
		terror(writeStringToStream((char *) (*controllerValue)->name,
		  ((MemoryOutputStream *) stream), e))
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
		  readStringFromStream(((MemoryInputStream *) stream), e))
	} else {
		terror(writeStringToStream((char *) (*noteValue)->name,
		  ((MemoryOutputStream *) stream), e))
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

static void loadStoreStep(void *step, pattern_t *pattern, void *stream,
  uint32_t idx, gboolean load, err_t *e)
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
			terror(setDummyStep(pattern, dummyUserStep, set, &lockContext, e))
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
	terror(readWriteStream(&valuePosition, sizeof(valuePosition), stream, load, e))
	if (IS_NOTE(pattern)) {
		terror(readWriteStream(&velocityPosition,
		  sizeof(velocityPosition), stream, load, e))
	}
	if ((load)&&(valuePosition > -1)) {
		if (IS_NOTE(pattern)) {
			terror(setNoteStep(pattern, (noteUserStep_t *) step,
			  g_slist_nth(VALUES(pattern), valuePosition),
			  g_slist_nth(VELOCITIES(pattern), velocityPosition),
			  idx, &lockContext, FALSE, e))
		} else {
			terror(setControllerStep(pattern, (controllerUserStep_t *) step,
			  g_slist_nth(VALUES(pattern), valuePosition), idx,
			  &lockContext, e))
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
		  idx, &lockContext, FALSE, e))
	}
	terror(readWriteStream(&(noteUserStep->slideLocked),
	  sizeof(noteUserStep->slideLocked), stream, load, e))


finish:
	return;
}

static void loadStorePattern(pattern_t **pattern,
  void *stream, gboolean load, pattern_t *parent,
  err_t *e);

static void loadStoreChildren(pattern_t *parent, void *stream,
  gboolean load, err_t *e)
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
		terror(loadStorePattern(&child, stream, load, parent, e))
		if (load) {
			parent->children =
			  g_slist_append((GSList *) parent->children, child);
		}
	}

finish:
	return;
}

static void loadStorePattern(pattern_t **pattern,
  void *stream, gboolean load, pattern_t *parent,
  err_t *e)
{
	MARK();

	void *freeMe = NULL;
	pattern_t *p = NULL;
	MemoryOutputStream *memoryOutputStream = NULL;
	MemoryInputStream *memoryInputStream = NULL;

	if (load) {
		memoryInputStream = (MemoryInputStream *) stream;
		freeMe = p = allocatePattern(parent);
		PARENT(p) = parent;
		terror(NAME(p) = readStringFromStream(memoryInputStream, e))	
	} else {
		memoryOutputStream = (MemoryOutputStream *) stream;
		p = *pattern;
		terror(writeStringToStream(NAME(p), memoryOutputStream, e))
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
		  NR_USERSTEPS_PER_BAR(p), &lockContext, FALSE, 0,e))
	}

	if (!IS_DUMMY(p)) {
		terror(loadStoreValues(p, stream, load, e))
		if (!IS_NOTE(p)) {
			terror(readWriteStream((void *) PTR_PARAMETER(p),
			  sizeof(PARAMETER(p)), stream, load, e))
		}
		terror(loadStoreVelocities(p, stream, load, e))
	}

	for (uint32_t i = 0; i < NR_USERSTEPS(p); i++) {
		void *step = USERSTEP_AT(p, i);
		terror(loadStoreStep(step, p, stream, i, load, e))
	}

	terror(loadStoreChildren(p, stream, load, e))

	if (load) {
		*pattern = p;
	}
	freeMe = NULL;
finish:
	if (freeMe != NULL) {
		freePattern((pattern_t *) freeMe);
	}
}

//==============================================================================
void KVstSequencerAudioProcessor::getStateInformation (MemoryBlock& destData)
{
	MARK();

    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
	err_t err;
	err_t *e = &err;

	initErr(e);

	MemoryOutputStream *memoryOutputStream =
	  new MemoryOutputStream(destData, FALSE);

	terror(loadStorePattern(((pattern_t **) &(patterns.root)),
	  memoryOutputStream, FALSE, (pattern_t *) NULL, e))

finish:
	if (memoryOutputStream != NULL) {
		delete memoryOutputStream;
	}
	return;
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
	err_t err;
	err_t *e = &err;
	pattern_t *pattern = NULL;

	initErr(e);

	MemoryInputStream *memoryInputStream =
	  new MemoryInputStream(data, sizeInBytes, FALSE);

	terror(loadStorePattern(((pattern_t **) &pattern),
	  memoryInputStream, TRUE, (pattern_t *) NULL, e))
	if (patterns.root != NULL) {
		freePattern(((pattern_t *) patterns.root));
	}
	patterns.root = pattern;
	pattern = NULL;

finish:
	if (memoryInputStream != NULL) {
		delete memoryInputStream;
	}
	if (pattern != NULL) {
		freePattern(pattern);
	}
	return;
 }

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	MARK();

	RESET_PATTERN();

    return new KVstSequencerAudioProcessor();
}
