/* Copyright 2017 Martin Knappe (martin.knappe at gmail dot com) */

/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#ifndef PLUGINEDITOR_H_INCLUDED
#define PLUGINEDITOR_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginProcessor.h"

#include <glib.h>

//==============================================================================
/**
*/
class KVstSequencerAudioProcessorEditor  : public AudioProcessorEditor
{
public:
    KVstSequencerAudioProcessorEditor (KVstSequencerAudioProcessor&);
    ~KVstSequencerAudioProcessorEditor();

    //==============================================================================
    void paint (Graphics&) override;
    void resized(void) override;
	void handleCommandMessage(int) override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    KVstSequencerAudioProcessor& processor;
	
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KVstSequencerAudioProcessorEditor)
};

class CompositeComponent : public Component
{
public:
	CompositeComponent(Component *parent, gboolean vertical);
	virtual ~CompositeComponent() override;
	void resized() override;

private:
	gboolean vertical;
	void movedOrResized();

};


#endif  // PLUGINEDITOR_H_INCLUDED
