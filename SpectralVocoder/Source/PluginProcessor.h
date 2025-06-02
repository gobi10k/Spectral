#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "SpectralAnalyzer.h" // Include actual analyzer
#include "VocoderEngine.h"  // Include actual engine

//==============================================================================
/**
*/
class SpectralVocoderAudioProcessor  : public juce::AudioProcessor,
                                     public juce::AudioProcessorValueTreeState::Listener // For parameter changes
{
public:
    //==============================================================================
    enum VocoderMode
    {
        Classic
        // Add other modes here if needed
    };

    //==============================================================================
    SpectralVocoderAudioProcessor();
    ~SpectralVocoderAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState();

    // Parameter listener callback
    void parameterChanged(const juce::String& parameterID, float newValue) override;

private:
    //==============================================================================
    void updateProcessingParameters(); // Renamed from updateParameters for clarity

    // Parameters (raw pointers, owned by APVTS)
    juce::AudioParameterInt* numBandsParamPtr = nullptr;
    juce::AudioParameterFloat* attackTimeParamPtr = nullptr;
    juce::AudioParameterFloat* releaseTimeParamPtr = nullptr;

    juce::AudioProcessorValueTreeState parameters;

    // Actual instances of analyzer and engine
    std::unique_ptr<SpectralAnalyzer> spectralAnalyzer;
    std::unique_ptr<VocoderEngine> vocoderEngine;

    VocoderMode currentMode;
    
    // Store last known values to detect changes
    int lastNumBands = 0;
    // Attack/Release are read directly in processBlock via APVTS pointers for simplicity here

    // FFT and Hop Size constants
    static constexpr int defaultFFTOrder = 11; // FFT size = 2048
    static constexpr int defaultHopSizeFactor = 4; // Hop size = FFT_SIZE / 4
    
    int currentFFTSize = 0;
    int currentHopSize = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralVocoderAudioProcessor)
};
