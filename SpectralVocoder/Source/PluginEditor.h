#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Forward declaration
class SpectralVocoderAudioProcessor;

// Placeholder for spectral display
class SpectralDisplayComponent : public juce::Component
{
public:
    SpectralDisplayComponent() { /* Constructor */ }
    ~SpectralDisplayComponent() override { /* Destructor */ }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::white);
        g.drawText("Spectral Display Placeholder", getLocalBounds(), juce::Justification::centred, false);
    }

    void resized() override
    {
        // Layout for sub-components if any
    }
};

//==============================================================================
/**
*/
class SpectralVocoderAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    SpectralVocoderAudioProcessorEditor (SpectralVocoderAudioProcessor&);
    ~SpectralVocoderAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SpectralVocoderAudioProcessor& audioProcessor;

    juce::Slider numBandsSlider;
    juce::Label numBandsLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> numBandsAttachment;

    juce::Slider attackTimeSlider;
    juce::Label attackTimeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackTimeAttachment;

    juce::Slider releaseTimeSlider;
    juce::Label releaseTimeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseTimeAttachment;

    SpectralDisplayComponent spectralDisplayPlaceholder; // Placeholder component

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralVocoderAudioProcessorEditor)
};
