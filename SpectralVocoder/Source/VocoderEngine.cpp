#include "VocoderEngine.h"
// #include "SpectralAnalyzer.h" // Not directly needed if only passing data
#include <juce_core/juce_core.h> // For DBG, jassert
#include <cmath> // For std::log, std::exp, std::sqrt

VocoderEngine::VocoderEngine()
    : ifft(0), // Will be properly initialized in prepare
      synthesisWindow(0, juce::dsp::WindowingFunction<float>::hann, false) // Symmetric window for OLA
{
    // Constructor: Initialize with default values or leave for prepare()
}

VocoderEngine::~VocoderEngine()
{
}

void VocoderEngine::prepare(const juce::dsp::ProcessSpec& spec, int newfftSize, int initialNumBands)
{
    sampleRate = spec.sampleRate;
    fftSize = newfftSize;
    currentNumBands = initialNumBands;
    currentBlockSize = spec.maximumBlockSize; // Store for later use if needed

    jassert(sampleRate > 0);
    jassert(fftSize > 0 && (fftSize & (fftSize - 1)) == 0); // Power of 2 check
    jassert(currentNumBands > 0);

    int fftOrder = static_cast<int>(std::log2(fftSize));
    ifft = juce::dsp::FFT(fftOrder);
    synthesisWindow.initialise(fftSize, juce::dsp::WindowingFunction<float>::hann, false); // Symmetric for OLA

    hopSize = fftSize / 4; // Common hop size for OLA

    modulatorBandFilters.resize(currentNumBands);
    for (auto& filter : modulatorBandFilters)
    {
        // BallisticsFilter processes single samples; spec.numChannels = 1.
        // spec.maximumBlockSize for BallisticsFilter isn't critical if processing sample-by-sample,
        // but using the plugin's block size is fine.
        filter.prepare({ sampleRate, currentBlockSize, 1 });
    }

    modulatorBandMagnitudes.resize(currentNumBands, 0.0f);
    
    // Buffer for IFFT output (real signal, but FFT object works on complex packed)
    ifftOutputBuffer.setSize(1, fftSize); 
    ifftOutputBuffer.clear();

    // Circular buffer for overlap-add synthesis stage
    vocodedOverlapAddBuffer.setSize(1, fftSize); // Should be large enough for one window + overlap
    vocodedOverlapAddBuffer.clear();
    outputBufferFillPointer = 0;

    // Complex data for IFFT input (real, imag, real, imag...)
    processedComplexSpectrum.resize(fftSize, 0.0f);

    updateBandInformation();
    
    prepared = true;
    DBG("VocoderEngine prepared. FFT Size: " << fftSize << ", Bands: " << currentNumBands << ", Sample Rate: " << sampleRate);
}

void VocoderEngine::setNumBands(int newNumBands)
{
    if (newNumBands != currentNumBands && newNumBands > 0)
    {
        currentNumBands = newNumBands;
        if (prepared) {
            modulatorBandFilters.resize(currentNumBands);
            // Use stored sampleRate and currentBlockSize for preparing new filters
            juce::dsp::ProcessSpec filterSpec { sampleRate, currentBlockSize, 1 };
            for (auto& filter : modulatorBandFilters)
            {
                // Check if filter is already prepared perhaps, though re-preparing is safe.
                filter.prepare(filterSpec); 
            }
            modulatorBandMagnitudes.resize(currentNumBands, 0.0f);
            updateBandInformation();
            DBG("VocoderEngine: Number of bands changed to " << currentNumBands);
        }
    }
}

void VocoderEngine::calculateBandLimits(int numBandsToUse, float minFreq, float maxFreq,
                                        std::vector<float>& bandStartFreqs, std::vector<float>& bandEndFreqs,
                                        std::vector<int>& bandStartBins, std::vector<int>& bandEndBins)
{
    bandStartFreqs.resize(numBandsToUse);
    bandEndFreqs.resize(numBandsToUse);
    bandStartBins.resize(numBandsToUse);
    bandEndBins.resize(numBandsToUse);

    if (numBandsToUse == 0 || fftSize == 0 || sampleRate == 0.0) return;

    float nyquist = static_cast<float>(sampleRate / 2.0);
    minFreq = juce::jmax(minFreq, 20.0f);          // Ensure minFreq is reasonable
    maxFreq = juce::jmin(maxFreq, nyquist * 0.98f); // Ensure maxFreq is below Nyquist

    if (minFreq >= maxFreq) { // Safety check
        minFreq = 20.0f;
        maxFreq = nyquist * 0.98f;
    }
    
    // For logarithmic spacing of center frequencies (more common for vocoders)
    // We will define bands by their start/end frequencies directly for simpler spectral bin mapping.
    // This defines edges of bands logarithmically.
    double minLog = std::log(minFreq);
    double maxLog = std::log(maxFreq);
    double logRange = maxLog - minLog;

    for (int i = 0; i < numBandsToUse; ++i)
    {
        bandStartFreqs[i] = static_cast<float>(std::exp(minLog + (double(i) / numBandsToUse) * logRange));
        bandEndFreqs[i] = static_cast<float>(std::exp(minLog + (double(i + 1) / numBandsToUse) * logRange));

        // Convert frequencies to FFT bin indices
        // Bin index = frequency / (sampleRate / fftSize)
        float binWidth = static_cast<float>(sampleRate / fftSize);
        bandStartBins[i] = static_cast<int>(std::floor(bandStartFreqs[i] / binWidth));
        bandEndBins[i]   = static_cast<int>(std::ceil(bandEndFreqs[i] / binWidth));

        // Ensure bins are within valid range [0, fftSize/2] (since we use real FFT output)
        bandStartBins[i] = juce::jlimit(0, fftSize / 2, bandStartBins[i]);
        bandEndBins[i]   = juce::jlimit(bandStartBins[i] + 1, fftSize / 2 + 1, bandEndBins[i]); // Ensure end bin is after start
    }
}


void VocoderEngine::updateBandInformation()
{
    if (fftSize == 0 || sampleRate == 0) return;
    // Calculate band frequency ranges and corresponding FFT bin indices
    // Example: Logarithmic spacing from 100 Hz up to Nyquist/2 for more musical bands
    calculateBandLimits(currentNumBands, 100.0f, static_cast<float>(sampleRate / 2.0f * 0.8f), // go up to 80% of Nyquist
                        bandStartFrequencies, bandEndFrequencies,
                        bandStartIndices, bandEndIndices);
}


void VocoderEngine::process(const std::vector<float>& modulatorSpectrumComplex, // Packed complex [Re, Im, Re, Im...]
                            const std::vector<float>& carrierSpectrumComplex,   // Packed complex
                            juce::AudioBuffer<float>& outputBuffer,
                            float attackTimeMs, float releaseTimeMs)
{
    if (!prepared) return;

    jassert(modulatorSpectrumComplex.size() == fftSize);
    jassert(carrierSpectrumComplex.size() == fftSize);

    // --- Modulator Analysis per Band (Simpler Spectral Approach) ---
    for (int i = 0; i < currentNumBands; ++i)
    {
        float bandEnergy = 0.0f;
        // Sum magnitudes of FFT bins within this vocoder band
        for (int bin = bandStartIndices[i]; bin < bandEndIndices[i]; ++bin)
        {
            if (bin * 2 + 1 < modulatorSpectrumComplex.size()) { // Check bounds for Re and Im parts
                float real = modulatorSpectrumComplex[bin * 2];
                float imag = modulatorSpectrumComplex[bin * 2 + 1];
                bandEnergy += std::sqrt(real * real + imag * imag); // Magnitude
            }
        }
        // Average energy in the band
        int numBinsInBand = bandEndIndices[i] - bandStartIndices[i];
        if (numBinsInBand > 0) {
            bandEnergy /= static_cast<float>(numBinsInBand);
        }
        
        // Apply ballistics filter to this band's energy
        auto& bandFilter = modulatorBandFilters[i];
        bandFilter.setAttackTime(attackTimeMs);    // Set times each process call, or only when they change
        bandFilter.setReleaseTime(releaseTimeMs);
        // BallisticsFilter processSample expects a raw sample, not an AudioBlock
        float envelopeValue = bandFilter.processSample(0, bandEnergy); // Process the single energy value for channel 0
        modulatorBandMagnitudes[i] = envelopeValue;
    }

    // --- Carrier Processing & Synthesis (Simpler Spectral Approach) ---
    // Clear the processed spectrum (which will be input to IFFT)
    std::fill(processedComplexSpectrum.begin(), processedComplexSpectrum.end(), 0.0f);

    for (int i = 0; i < currentNumBands; ++i)
    {
        float modulatorGain = modulatorBandMagnitudes[i];
        // Apply this gain to the carrier spectrum bins within this band
        for (int bin = bandStartIndices[i]; bin < bandEndIndices[i]; ++bin)
        {
            if (bin * 2 + 1 < carrierSpectrumComplex.size() && bin * 2 + 1 < processedComplexSpectrum.size()) {
                float carrierReal = carrierSpectrumComplex[bin * 2];
                float carrierImag = carrierSpectrumComplex[bin * 2 + 1];
                
                // Simple scaling of carrier components by modulator band gain
                processedComplexSpectrum[bin * 2]     = carrierReal * modulatorGain;
                processedComplexSpectrum[bin * 2 + 1] = carrierImag * modulatorGain;
            }
        }
    }
    
    // --- IFFT and Overlap-Add Synthesis ---
    // Perform IFFT. Input is processedComplexSpectrum, output is also processedComplexSpectrum (in-place)
    ifft.performRealOnlyInverseTransform(processedComplexSpectrum.data());

    // Copy IFFT output (now time-domain) to ifftOutputBuffer
    // The IFFT output is scaled by 1/fftSize by JUCE's FFT, which is often desired.
    // If not, manual scaling might be needed.
    ifftOutputBuffer.copyFrom(0, 0, processedComplexSpectrum.data(), fftSize);

    // Apply synthesis window
    synthesisWindow.applyWindowing(ifftOutputBuffer.getWritePointer(0), fftSize);

    // Overlap-add to the vocodedOverlapAddBuffer
    // The first `hopSize` samples of vocodedOverlapAddBuffer are ready for output.
    // The new ifftOutputBuffer is added, overlapping with existing data.
    for (int i = 0; i < fftSize; ++i) {
        vocodedOverlapAddBuffer.addSample(0, i, ifftOutputBuffer.getSample(0, i));
    }

    // Copy `hopSize` samples to the actual output buffer
    int numSamplesForOutput = outputBuffer.getNumSamples();
    float* outWriteL = outputBuffer.getWritePointer(0);
    // float* outWriteR = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer(1) : nullptr; // For stereo output

    const float* vocodedReadPtr = vocodedOverlapAddBuffer.getReadPointer(0);

    for (int i = 0; i < numSamplesForOutput; ++i) {
        if (outputBufferFillPointer < hopSize) { // Only copy if we have new data from the OLA buffer's "ready" section
            outWriteL[i] = vocodedReadPtr[outputBufferFillPointer];
            // if (outWriteR) outWriteR[i] = vocodedReadPtr[outputBufferFillPointer]; // Basic stereo copy
            outputBufferFillPointer++;
        } else {
            // Should not happen if processBlock calls are aligned with hopSize,
            // or if input block size matches hopSize.
            // For now, fill with silence if we run out of OLA'd samples.
            outWriteL[i] = 0.0f;
            // if (outWriteR) outWriteR[i] = 0.0f;
        }
    }
    
    // If we've outputted a hop's worth of samples, shift the OLA buffer
    if (outputBufferFillPointer >= hopSize) {
        // Shift data in vocodedOverlapAddBuffer: move last (fftSize - hopSize) samples to the beginning
        int samplesToKeep = fftSize - hopSize;
        if (samplesToKeep > 0) {
            // Use a temp buffer for safe shifting
            juce::AudioBuffer<float> temp(1, samplesToKeep);
            temp.copyFrom(0, 0, vocodedOverlapAddBuffer, 0, hopSize, samplesToKeep);
            vocodedOverlapAddBuffer.clear();
            vocodedOverlapAddBuffer.copyFrom(0, 0, temp, 0, 0, samplesToKeep);
        } else {
            vocodedOverlapAddBuffer.clear();
        }
        outputBufferFillPointer = 0; // Reset for the next block of output
    }
    // For simplicity, this OLA output assumes outputBuffer.getNumSamples() <= hopSize.
    // Proper OLA output management for variable block sizes is more complex.
}
