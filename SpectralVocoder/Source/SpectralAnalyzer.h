#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

class SpectralAnalyzer
{
public:
    SpectralAnalyzer(int fftOrder, int hopSize);
    ~SpectralAnalyzer();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void doAnalysis(const juce::AudioBuffer<float>& modulatorBuffer, const juce::AudioBuffer<float>& carrierBuffer);

    const std::vector<float>& getModulatorSpectrum() const;
    const std::vector<float>& getCarrierSpectrum() const;
    int getFFTSize() const;
    int getHopSize() const;


private:
    void performFFT(const juce::AudioBuffer<float>& inputSegment, 
                    std::vector<float>& fftOutput,
                    juce::dsp::FFT& fft, 
                    juce::AudioBuffer<float>& windowedData,
                    std::vector<float>& tempFFTInputDataContainer);

    void processInputBuffer(const juce::AudioBuffer<float>& inputSamples,
                            juce::AudioBuffer<float>& circularBuffer,
                            int& fillPointer,
                            std::vector<float>& fftOutputData,
                            juce::dsp::FFT& targetFFT,
                            juce::AudioBuffer<float>& targetWindowedData,
                            std::vector<float>& targetTempFFTInputData);


    int order;
    int fftSize;
    int actualHopSize;

    double sampleRate = 0.0;
    int maxBlockSize = 0;

    juce::dsp::FFT modulatorFFT;
    juce::dsp::FFT carrierFFT;
    juce::dsp::WindowingFunction<float> window;

    // Buffers for windowed input data (single channel for FFT)
    juce::AudioBuffer<float> windowedModulatorData;
    juce::AudioBuffer<float> windowedCarrierData;

    // Temporary containers for preparing data for FFT object input (real part)
    std::vector<float> fftModulatorInput; // size fftSize
    std::vector<float> fftCarrierInput;   // size fftSize

    // Buffers to store FFT output data (complex, packed: real, imag, real, imag...)
    // Size fftSize for performRealOnlyForwardTransform output (packed format)
    std::vector<float> modulatorFFTOutput;
    std::vector<float> carrierFFTOutput;

    // Circular buffers to accumulate input samples
    juce::AudioBuffer<float> modulatorInputBuffer; // 1 channel, size fftSize
    juce::AudioBuffer<float> carrierInputBuffer;   // 1 channel, size fftSize

    int modulatorInputBufferFillPtr = 0;
    int carrierInputBufferFillPtr = 0;
    
    bool prepared = false;
};
