#include "PluginProcessor.h"
#include "PluginEditor.h" 
#include "SpectralAnalyzer.h" // Already included but good for clarity
#include "VocoderEngine.h"  // Already included but good for clarity

//==============================================================================
SpectralVocoderAudioProcessor::SpectralVocoderAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::quadraphonic(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#else
    :
#endif
      parameters (*this, nullptr, juce::Identifier ("SpectralVocoderParams"),
                  {
                      std::make_unique<juce::AudioParameterInt> ("numBands",    // parameterID
                                                                 "Number of Bands", // parameter name
                                                                 4,             // min value
                                                                 128,           // max value
                                                                 20),           // default value
                      std::make_unique<juce::AudioParameterFloat> ("attackTime",  // parameterID
                                                                   "Attack Time", // parameter name
                                                                   juce::NormalisableRange<float>(1.0f, 500.0f, 0.1f, 0.3f), // range, last arg is skewFactor
                                                                   5.0f),        // default value
                      std::make_unique<juce::AudioParameterFloat> ("releaseTime", // parameterID
                                                                   "Release Time",// parameter name
                                                                   juce::NormalisableRange<float>(1.0f, 2000.0f, 0.1f, 0.3f), // range
                                                                   50.0f)       // default value
                  })
{
    numBandsParamPtr = dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter("numBands"));
    attackTimeParamPtr = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("attackTime"));
    releaseTimeParamPtr = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("releaseTime"));

    jassert(numBandsParamPtr != nullptr);
    jassert(attackTimeParamPtr != nullptr);
    jassert(releaseTimeParamPtr != nullptr);

    currentFFTSize = 1 << defaultFFTOrder;
    currentHopSize = currentFFTSize / defaultHopSizeFactor;

    spectralAnalyzer = std::make_unique<SpectralAnalyzer>(defaultFFTOrder, currentHopSize);
    vocoderEngine = std::make_unique<VocoderEngine>();

    currentMode = VocoderMode::Classic; // Default mode
    lastNumBands = numBandsParamPtr->get(); 

    parameters.addParameterListener("numBands", this);
    // Attack and release times are read directly in processBlock, no listener needed for them specifically
    // unless other components in PluginProcessor need immediate notification.
}

SpectralVocoderAudioProcessor::~SpectralVocoderAudioProcessor()
{
    parameters.removeParameterListener("numBands", this);
    // spectralAnalyzer and vocoderEngine are cleaned up by unique_ptr
}

void SpectralVocoderAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "numBands")
    {
        int newNumBands = static_cast<int>(newValue);
        if (vocoderEngine && newNumBands != lastNumBands) {
            vocoderEngine->setNumBands(newNumBands);
            lastNumBands = newNumBands;
            DBG("PluginProcessor::parameterChanged - numBands set to: " << newNumBands);
        }
    }
    // Other parameters can be handled here if needed for immediate processor-level changes
}


//==============================================================================
const juce::String SpectralVocoderAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SpectralVocoderAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true; 
   #else
    return false; 
   #endif
}

bool SpectralVocoderAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SpectralVocoderAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false; 
   #endif
}

double SpectralVocoderAudioProcessor::getTailLengthSeconds() const
{
    return 0.0; // Adjust if your effects have tails (e.g. reverb, delay)
}

int SpectralVocoderAudioProcessor::getNumPrograms()
{
    return 1;   
}

int SpectralVocoderAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SpectralVocoderAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String SpectralVocoderAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void SpectralVocoderAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void SpectralVocoderAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Analyzer spec: Using 2 channels for its internal processing, can be adapted.
    // It processes modulator and carrier separately, each assumed mono for FFT.
    // The '2' here refers to the number of channels SpectralAnalyzer's internal buffers might be sized for if it were to handle stereo FFT directly.
    // However, SpectralAnalyzer's processInputBuffer currently takes a mono view (channel 0) of the passed AudioBuffer.
    // So, the numChannels in this spec is more about what SpectralAnalyzer itself expects for its internal setup.
    // Let's assume it's designed to handle up to 2 channels internally if it were to do stereo analysis, even if current use is mono.
    juce::dsp::ProcessSpec analyzerSpec { sampleRate, static_cast<juce::uint32>(samplesPerBlock), static_cast<juce::uint32>(getTotalNumInputChannels()) };
    spectralAnalyzer->prepare(analyzerSpec);
    
    // Verify actual FFT size from analyzer after its preparation, if it could change
    // currentFFTSize = spectralAnalyzer->getFFTSize(); // Assuming it's fixed by constructor args for now

    int initialNumBands = numBandsParamPtr->get();
    lastNumBands = initialNumBands; // Store initial value

    // Engine spec: Outputting stereo
    juce::dsp::ProcessSpec engineSpec { sampleRate, static_cast<juce::uint32>(samplesPerBlock), static_cast<juce::uint32>(getTotalNumOutputChannels()) };
    vocoderEngine->prepare(engineSpec, currentFFTSize, initialNumBands);
    DBG("PluginProcessor::prepareToPlay. SR: " << sampleRate << ", BS: " << samplesPerBlock << ", FFT: " << currentFFTSize << ", Bands: " << initialNumBands);
}

void SpectralVocoderAudioProcessor::releaseResources()
{
    // spectralAnalyzer->release(); // If they had such methods
    // vocoderEngine->release();
    spectralAnalyzer.reset();
    vocoderEngine.reset();
    DBG("PluginProcessor::releaseResources");
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SpectralVocoderAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainInputSet = layouts.getMainInputChannelSet();
    const auto& mainOutputSet = layouts.getMainOutputChannelSet();

    if (mainInputSet.size() != 4) return false;
    if (mainOutputSet.size() != 2) return false;

    // Further checks can be done here, e.g. mainInputSet == juce::AudioChannelSet::quadraphonic()
    // For now, just checking channel counts.
    return true;
}
#endif

// This method is not strictly needed if parameters are fetched directly in processBlock
// or if listeners handle all necessary updates. Kept for potential future use.
void SpectralVocoderAudioProcessor::updateProcessingParameters()
{
    // int newNumBands = numBandsParamPtr->get();
    // if (newNumBands != lastNumBands) {
    //     if (vocoderEngine) vocoderEngine->setNumBands(newNumBands);
    //     lastNumBands = newNumBands;
    // }
    // Attack and release are fetched directly in processBlock via their pointers.
}


void SpectralVocoderAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    // Clear output buffer before processing to avoid leftover audio
    for (int i = 0; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    // updateProcessingParameters(); // Call if defined and needed for pre-processing updates
                                  // Or handle parameter changes via listeners as implemented.

    if (totalNumInputChannels < 4) {
        // Not enough inputs, vocoder cannot function as designed.
        // Output silence or bypass. For now, outputting silence (already cleared).
        return;
    }

    // Create separate AudioBuffer instances for modulator and carrier.
    // SpectralAnalyzer expects mono input (uses channel 0 of the passed buffer).
    // We prepare stereo buffers here and pass them; SpectralAnalyzer will pick channel 0.
    juce::AudioBuffer<float> modulatorSignalBuffer(2, numSamples);
    juce::AudioBuffer<float> carrierSignalBuffer(2, numSamples);
    modulatorSignalBuffer.clear(); // Ensure silence if not enough input channels
    carrierSignalBuffer.clear();   // Ensure silence if not enough input channels

    // Modulator (Inputs 1 & 2 - buffer channels 0 & 1)
    // Copy channel 0 (Input 1) to modulatorSignalBuffer channel 0
    if (totalNumInputChannels > 0) { // Input 1 available
        modulatorSignalBuffer.copyFrom(0, 0, buffer, 0, 0, numSamples);
    }
    // Copy channel 1 (Input 2) to modulatorSignalBuffer channel 1
    if (totalNumInputChannels > 1) { // Input 2 available
        modulatorSignalBuffer.copyFrom(1, 0, buffer, 1, 0, numSamples);
    } else if (totalNumInputChannels > 0) { // Only Input 1 available for modulator
        modulatorSignalBuffer.copyFrom(1, 0, modulatorSignalBuffer, 0, 0, numSamples); // Copy L to R for modulator
    }

    // Carrier (Inputs 3 & 4 - buffer channels 2 & 3)
    // Copy channel 2 (Input 3) to carrierSignalBuffer channel 0
    if (totalNumInputChannels > 2) { // Input 3 available
        carrierSignalBuffer.copyFrom(0, 0, buffer, 2, 0, numSamples);
    }
    // Copy channel 3 (Input 4) to carrierSignalBuffer channel 1
    if (totalNumInputChannels > 3) { // Input 4 available
        carrierSignalBuffer.copyFrom(1, 0, buffer, 3, 0, numSamples);
    } else if (totalNumInputChannels > 2) { // Only Input 3 available for carrier
        carrierSignalBuffer.copyFrom(1, 0, carrierSignalBuffer, 0, 0, numSamples); // Copy L to R for carrier
    }
    // Note: If totalNumInputChannels is <3, carrierSignalBuffer remains silent, which is acceptable.
    // SpectralAnalyzer will process silence for carrier if carrierSignalBuffer is empty/silent.

    // Perform spectral analysis
    if (spectralAnalyzer) {
        spectralAnalyzer->doAnalysis(modulatorSignalBuffer, carrierSignalBuffer);
    }

    // Perform vocoding and synthesis
    if (vocoderEngine && spectralAnalyzer) {
        // Ensure attack and release times are current from parameters
        float currentAttack = attackTimeParamPtr->get();
        float currentRelease = releaseTimeParamPtr->get();

        vocoderEngine->process(spectralAnalyzer->getModulatorSpectrum(),
                               spectralAnalyzer->getCarrierSpectrum(),
                               buffer, // Output directly to the plugin's output buffer
                               currentAttack,
                               currentRelease);
    }
    
    // If any channels were not written to by the vocoder engine (e.g. if it only produces mono but plugin is stereo)
    // ensure they are cleared. (Already done at the start, but good to be mindful of).
}

//==============================================================================
bool SpectralVocoderAudioProcessor::hasEditor() const
{
    return true; 
}

juce::AudioProcessorEditor* SpectralVocoderAudioProcessor::createEditor()
{
    return new SpectralVocoderAudioProcessorEditor (*this); 
}

//==============================================================================
void SpectralVocoderAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SpectralVocoderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
    
    // After loading state, ensure parameters are refreshed
    // This will trigger parameterChanged for "numBands" if it changed.
    // For other parameters not using listeners for processor-side changes,
    // they will be picked up at the next processBlock or by calling a refresh method.
    // For numBands, the listener should handle the update to vocoderEngine.
    // Re-cache lastNumBands after state load to ensure listener logic is correct on next manual change.
    if (numBandsParamPtr) { // Check if params are valid (they should be)
         int newNumBands = numBandsParamPtr->get();
         if (newNumBands != lastNumBands) { // If loaded state changed numBands
            if (vocoderEngine) vocoderEngine->setNumBands(newNumBands);
         }
         lastNumBands = newNumBands;
    }
}

juce::AudioProcessorValueTreeState& SpectralVocoderAudioProcessor::getValueTreeState()
{
    return parameters;
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpectralVocoderAudioProcessor();
}
