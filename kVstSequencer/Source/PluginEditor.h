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

class Container
{
public:
	Container(Component *component, gboolean vertical);
    virtual ~Container();
	void addContainer(Container *container);
	void addComponent(Component *component);
	void removeContainer(Container *container);
	void removeComponent(Component *component);
	void layout(Rectangle<int> rectangle);
    //==============================================================================
protected:
	Component *component;

private:
	void addElement(void *element);
	void removeElement(void *element);
	GSList *elements;
	gboolean vertical;
};


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


#endif  // PLUGINEDITOR_H_INCLUDED
