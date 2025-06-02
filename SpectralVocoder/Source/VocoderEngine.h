#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_dsp/effects/juce_EnvelopeFollower.h> // Corrected include for EnvelopeFollower
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

// Forward declaration (if SpectralAnalyzer methods were to be called directly, not needed if only data is passed)
// class SpectralAnalyzer; 

class VocoderEngine
{
public:
    VocoderEngine();
    ~VocoderEngine();

    void prepare(const juce::dsp::ProcessSpec& spec, int newfftSize, int initialNumBands);
    
    // Uses the "Simpler Spectral Approach"
    void process(const std::vector<float>& modulatorSpectrumComplex, // Packed complex from FFT
                 const std::vector<float>& carrierSpectrumComplex,   // Packed complex from FFT
                 juce::AudioBuffer<float>& outputBuffer,
                 float attackTimeMs, float releaseTimeMs);

    void setNumBands(int newNumBands);
    int getNumBands() const { return currentNumBands; }
    int getFFTSize() const { return fftSize; }


private:
    void updateBandInformation(); // Renamed from updateFilterBank for spectral approach
    void calculateBandLimits(int numBandsToUse, float minFreq, float maxFreq, 
                             std::vector<float>& bandStartFreqs, std::vector<float>& bandEndFreqs,
                             std::vector<int>& bandStartBin, std::vector<int>& bandEndBin);

    juce::dsp::FFT ifft;
    juce::dsp::WindowingFunction<float> synthesisWindow;

    // For "Simpler Spectral Approach" - we don't use a bank of BPFs on time-domain carrier here.
    // Instead, we work with spectral bands.
    std::vector<juce::dsp::EnvelopeFollower<float>> modulatorBandEnvelopeFollowers;

    juce::AudioBuffer<float> ifftOutputBuffer;        // Time-domain data after IFFT
    juce::AudioBuffer<float> vocodedOverlapAddBuffer; // Circular buffer for OLA synthesis
    int outputBufferFillPointer = 0;
    int hopSize = 0; // Will be derived from fftSize, e.g., fftSize / 4

    int currentNumBands = 20;
    int fftSize = 0;
    double sampleRate = 0.0;

    // Store modulator band magnitudes after envelope following
    std::vector<float> modulatorBandMagnitudes; 

    // For constructing spectrum for IFFT
    std::vector<float> processedComplexSpectrum; // Packed complex data for IFFT input

    // Band definitions
    std::vector<float> bandCenterFreqs; // Not strictly needed for simpler spectral approach but good for debug/extension
    std::vector<float> bandStartFrequencies;
    std::vector<float> bandEndFrequencies;
    std::vector<int> bandStartIndices; // FFT bin indices for each band
    std::vector<int> bandEndIndices;   // FFT bin indices for each band
    
    bool prepared = false;
};
