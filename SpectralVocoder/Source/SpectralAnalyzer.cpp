#include "SpectralAnalyzer.h"
#include <juce_core/juce_core.h> // For DBG

SpectralAnalyzer::SpectralAnalyzer(int fftOrder, int hopSize)
    : order(fftOrder),
      fftSize(1 << order), // fftSize = 2^fftOrder
      actualHopSize(hopSize),
      modulatorFFT(order),
      carrierFFT(order),
      window(fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    // Ensure fftSize is adequate for the window
    jassert(fftSize > 0);

    // Initialize FFT input buffers (these are temporary storage before copying to FFT object)
    fftModulatorInput.resize(fftSize, 0.0f);
    fftCarrierInput.resize(fftSize, 0.0f);

    // Initialize FFT output buffers
    // For performRealOnlyForwardTransform, output is packed complex numbers.
    // The size of the output array will be fftSize. (e.g., for fftSize 1024, it stores 512 complex numbers)
    // First element is DC, last is Nyquist (if fftSize is even).
    modulatorFFTOutput.resize(fftSize, 0.0f); // Stores N/2+1 complex values, packed into N floats
    carrierFFTOutput.resize(fftSize, 0.0f);
}

SpectralAnalyzer::~SpectralAnalyzer()
{
    // Buffers will be cleaned up automatically by their destructors (std::vector, juce::AudioBuffer)
}

void SpectralAnalyzer::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxBlockSize = spec.maximumBlockSize; // Max samples we can receive in one doAnalysis call

    jassert(sampleRate > 0);
    jassert(maxBlockSize > 0);
    if (actualHopSize == 0) actualHopSize = fftSize / 4; // Default hop size if not specified properly

    // Initialize windowed data buffers (1 channel, fftSize)
    windowedModulatorData.setSize(1, fftSize, false, true, true); // clearExtraSpace=true, avoidRealloc=true, zeroInitialise=true
    windowedCarrierData.setSize(1, fftSize, false, true, true);
    windowedModulatorData.clear();
    windowedCarrierData.clear();
    
    // Initialize circular input buffers (1 channel, fftSize for simplicity, could be larger if needed)
    // These will hold incoming audio until enough samples for an FFT are available.
    // For overlap-add, these need to hold fftSize samples.
    modulatorInputBuffer.setSize(1, fftSize, false, true, true);
    carrierInputBuffer.setSize(1, fftSize, false, true, true);
    modulatorInputBuffer.clear();
    carrierInputBuffer.clear();

    modulatorInputBufferFillPtr = 0;
    carrierInputBufferFillPtr = 0;
    
    prepared = true;
    DBG("SpectralAnalyzer prepared. FFT Size: " << fftSize << ", Hop Size: " << actualHopSize << ", Sample Rate: " << sampleRate);
}

void SpectralAnalyzer::processInputBuffer(
    const juce::AudioBuffer<float>& inputSamples,
    juce::AudioBuffer<float>& circularBuffer,
    int& fillPointer,
    std::vector<float>& fftOutputData,
    juce::dsp::FFT& targetFFT,
    juce::AudioBuffer<float>& targetWindowedData,
    std::vector<float>& targetTempFFTInputData)
{
    if (!prepared) return;

    int numInputSamples = inputSamples.getNumSamples();
    if (numInputSamples == 0) return;

    // For simplicity, process only the first channel of inputSamples
    // In a stereo setup, you might want to average L/R or process them separately
    const float* currentInputReadPtr = inputSamples.getReadPointer(0); // Assuming mono or taking left channel

    int samplesProcessed = 0;
    while (samplesProcessed < numInputSamples)
    {
        int samplesToCopy = std::min(numInputSamples - samplesProcessed, fftSize - fillPointer);
        
        // Copy to circular buffer from the current read position in inputSamples
        circularBuffer.copyFrom(0, fillPointer, currentInputReadPtr + samplesProcessed, samplesToCopy);
        fillPointer += samplesToCopy;
        samplesProcessed += samplesToCopy;

        if (fillPointer == fftSize) // Buffer is full, ready for an FFT frame
        {
            performFFT(circularBuffer, fftOutputData, targetFFT, targetWindowedData, targetTempFFTInputData);
            
            // Overlap: shift old data by hopSize and reset fillPointer
            int samplesToKeep = fftSize - actualHopSize;
            if (samplesToKeep > 0)
            {
                // Move the last 'samplesToKeep' samples to the beginning of the buffer
                // This is done by copying from circularBuffer (offset actualHopSize) to circularBuffer (offset 0)
                // A temporary buffer might be safer if source and destination overlap in a problematic way,
                // but juce::AudioBuffer::copyFrom handles this internally for non-overlapping regions or uses memmove.
                // For clarity and safety with potential overlaps if not perfectly aligned:
                juce::AudioBuffer<float> tempShiftBuffer(1, samplesToKeep);
                tempShiftBuffer.copyFrom(0, 0, circularBuffer.getReadPointer(0, actualHopSize), samplesToKeep);
                circularBuffer.clear(0, fftSize); // Clear the whole buffer
                circularBuffer.copyFrom(0, 0, tempShiftBuffer.getReadPointer(0), samplesToKeep); // Copy back the kept samples
            }
            else // hopSize >= fftSize, no overlap or negative overlap (discard all)
            {
                 circularBuffer.clear(0, fftSize);
            }
            fillPointer = samplesToKeep > 0 ? samplesToKeep : 0;
        }
    }
}


void SpectralAnalyzer::doAnalysis(const juce::AudioBuffer<float>& modulatorBuffer,
                                  const juce::AudioBuffer<float>& carrierBuffer)
{
    if (!prepared)
    {
        DBG("SpectralAnalyzer::doAnalysis called before prepare() or with invalid setup.");
        return;
    }

    // Process modulator input
    // Ensure modulatorBuffer has at least one channel
    if (modulatorBuffer.getNumChannels() > 0) {
        processInputBuffer(modulatorBuffer, modulatorInputBuffer, modulatorInputBufferFillPtr,
                           modulatorFFTOutput, modulatorFFT, windowedModulatorData, fftModulatorInput);
    } else {
        // Handle empty modulator buffer if necessary, e.g., fill modulatorFFTOutput with zeros or skip
    }

    // Process carrier input
    // Ensure carrierBuffer has at least one channel
    if (carrierBuffer.getNumChannels() > 0) {
        processInputBuffer(carrierBuffer, carrierInputBuffer, carrierInputBufferFillPtr,
                           carrierFFTOutput, carrierFFT, windowedCarrierData, fftCarrierInput);
    } else {
        // Handle empty carrier buffer
    }
}


void SpectralAnalyzer::performFFT(const juce::AudioBuffer<float>& inputSegment, // This should be fftSize samples
                                  std::vector<float>& fftOutputContainer,      // Container for FFT results (packed complex)
                                  juce::dsp::FFT& fft,
                                  juce::AudioBuffer<float>& windowedDataBuffer, // Temporary buffer for windowed data
                                  std::vector<float>& tempFFTInputDataContainer) // Temporary for FFT input prep
{
    if (inputSegment.getNumSamples() != fftSize)
    {
        jassertfalse; 
        return;
    }
    
    const float* readPtr = inputSegment.getReadPointer(0); // Assuming inputSegment is mono
    
    // 1. Copy input segment to tempFFTInputDataContainer (std::vector<float>)
    std::copy(readPtr, readPtr + fftSize, tempFFTInputDataContainer.begin());

    // 2. Apply windowing function directly on tempFFTInputDataContainer
    // window.applyWindowing(tempFFTInputDataContainer.data(), fftSize); // Apply to std::vector's data
    // For safety and consistency with windowedDataBuffer usage:
    float* windowedWritePtr = windowedDataBuffer.getWritePointer(0);
    std::copy(tempFFTInputDataContainer.begin(), tempFFTInputDataContainer.end(), windowedWritePtr);
    window.applyWindowing(windowedWritePtr, fftSize);


    // 3. Copy windowed data from windowedDataBuffer to fftOutputContainer (which serves as FFT input/output)
    std::copy(windowedWritePtr, windowedWritePtr + fftSize, fftOutputContainer.begin());

    // 4. Perform forward FFT (in-place)
    fft.performRealOnlyForwardTransform(fftOutputContainer.data());
}

const std::vector<float>& SpectralAnalyzer::getModulatorSpectrum() const
{
    return modulatorFFTOutput;
}

const std::vector<float>& SpectralAnalyzer::getCarrierSpectrum() const
{
    return carrierFFTOutput;
}

int SpectralAnalyzer::getFFTSize() const
{
    return fftSize;
}

int SpectralAnalyzer::getHopSize() const
{
    return actualHopSize;
}
