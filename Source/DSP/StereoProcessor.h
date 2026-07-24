#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace shredspace
{

/**
 * StereoProcessor implements wet-path stereo placement (pan), width (mid/side),
 * low-frequency mono, and correlation telemetry.
 */
class StereoProcessor
{
public:
    StereoProcessor();
    ~StereoProcessor() = default;

    StereoProcessor (const StereoProcessor&) = delete;
    StereoProcessor& operator= (const StereoProcessor&) = delete;
    StereoProcessor (StereoProcessor&&) = delete;
    StereoProcessor& operator= (StereoProcessor&&) = delete;

    /**
     * Prepares the processor. Configures the smoothers and filters.
     */
    void prepare (double sampleRate, int maxBlockSize);

    /**
     * Resets the internal state of filters, smoothers, and telemetry accumulation.
     */
    void reset() noexcept;

    /**
     * Sets the stereo pan. p is in [-1, 1], clamped on entry.
     * gain_L = cos(pi*(p+1)/4), gain_R = sin(pi*(p+1)/4).
     * Smoothed at 30 ms.
     */
    void setPan (float p) noexcept;

    /**
     * Sets the mid/side width. w is in [0, 2], clamped on entry.
     * Smoothed at 50 ms.
     */
    void setWidth (float w) noexcept;

    /**
     * Sets the crossover frequency for low-frequency mono. hz is in [60, 500], clamped on entry.
     * Smoothed multiplicatively at 50 ms.
     */
    void setLowFrequencyMonoCrossover (float hz) noexcept;

    /**
     * Enables or disables low-frequency mono summing.
     */
    void setLowFrequencyMonoEnabled (bool enabled) noexcept;

    /**
     * Processes a stereo block in-place.
     */
    void process (float* left, float* right, int numSamples) noexcept;

    /**
     * Returns the current correlation telemetry value, clamped to [-1, 1].
     */
    float getCorrelation() const noexcept;

private:
    double sampleRate_ { 44100.0 };
    bool lowFrequencyMonoEnabled_ { true };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> panSmoother_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoother_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> crossoverFreqSmoother_;

    juce::dsp::LinkwitzRileyFilter<float> crossoverFilter_;

    // Telemetry variables for correlation
    float sumLR_ { 0.0f };
    float sumL2_ { 0.0f };
    float sumR2_ { 0.0f };
    float correlationFilterCoeff_ { 0.001f };

    std::atomic<float> correlation_ { 0.0f };
};

} // namespace shredspace
