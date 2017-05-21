/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "kVstSequencer.h"

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
}

KVstSequencerAudioProcessor::~KVstSequencerAudioProcessor()
{
	MARK();
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

    return 0.0;
}

int KVstSequencerAudioProcessor::getNumPrograms()
{
	MARK();

    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
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

void KVstSequencerAudioProcessor::changeProgramName (int index, const String& newName)
{
	MARK();

}

//==============================================================================
void KVstSequencerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
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
bool KVstSequencerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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
static int lastIntPosition = -1;
void KVstSequencerAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
	MARK();

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);

        // ..do something to the data...
    }

	AudioPlayHead::CurrentPositionInfo newTime;
	if (getPlayHead() != 0 && getPlayHead()->getCurrentPosition (newTime)) {
		fprintf(getOutFile(), "%s:%d\n", __FILE__, __LINE__);
		fprintf(getOutFile(), "%s:%d isPlaying: %s\n", __FILE__, __LINE__, newTime.isPlaying ? "TRUE" : "FALSE");
		fprintf(getOutFile(), "%s:%d bpm: %f\n", __FILE__, __LINE__, newTime.bpm);
		fprintf(getOutFile(), "%s:%d timeSigNumerator: %d\n", __FILE__, __LINE__, newTime.timeSigNumerator);
		fprintf(getOutFile(), "%s:%d timeSigDenominator: %d\n", __FILE__, __LINE__, newTime.timeSigDenominator);
		fprintf(getOutFile(), "%s:%d timeInSamples: %lld\n", __FILE__, __LINE__, newTime.timeInSamples);
		fprintf(getOutFile(), "%s:%d timeInSeconds: %f\n", __FILE__, __LINE__, newTime.timeInSeconds);
		fprintf(getOutFile(), "%s:%d ppqPosition: %f\n", __FILE__, __LINE__, newTime.ppqPosition);
		fprintf(getOutFile(), "%s:%d ppqPositionOfLastBarStart: %f\n", __FILE__, __LINE__, newTime.ppqPositionOfLastBarStart);
		fprintf(getOutFile(), "%s:%d isLooping: %s\n", __FILE__, __LINE__, newTime.isLooping ? "TRUE" : "FALSE");
		fprintf(getOutFile(), "%s:%d ppqLoopStart: %f\n", __FILE__, __LINE__, newTime.ppqLoopStart);
		fprintf(getOutFile(), "%s:%d ppqLoopEnd: %f\n", __FILE__, __LINE__, newTime.ppqLoopEnd);

		int intPosition = ((int) newTime.ppqPosition);
		MidiMessage m; 
		if (intPosition == lastIntPosition) {
			return;
		}
		lastIntPosition = intPosition;
		if ((intPosition) & 1) {
			fprintf(getOutFile(), "note on\n");
			m = MidiMessage::noteOn(1, 30, 0.9f);
		} else {
			fprintf(getOutFile(), "note off\n");
			m = MidiMessage::noteOff(1, 30, 0.9f);
		}
		fprintf(getOutFile(), "adding event %d\n", intPosition);
		midiMessages.addEvent (m, intPosition);
	} else {
		fprintf(getOutFile(), "%s:%d\n", __FILE__, __LINE__);
	} 

		MidiMessage msg;
		int ignore;
        for (MidiBuffer::Iterator it (midiMessages); it.getNextEvent (msg, ignore);)
        {
			fprintf(getOutFile(), "msg.getDescription: %s\n", msg.getDescription().toRawUTF8());
#if 0
            if      (msg.isNoteOn())  notes.add (msg.getNoteNumber());
            else if (msg.isNoteOff()) notes.removeValue (msg.getNoteNumber());
#endif
        } 
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

    return new KVstSequencerAudioProcessorEditor (*this);
}

//==============================================================================
void KVstSequencerAudioProcessor::getStateInformation (MemoryBlock& destData)
{
	MARK();

    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void KVstSequencerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
	MARK();

    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	MARK();

    return new KVstSequencerAudioProcessor();
}
