#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace shredspace
{

/**
 * Mild in-loop saturation stage for the repeat loop.
 *
 * Implements the unity-slope shaper s_D(x) = tanh(D*x)/D, blended linearly
 * with the dry signal so that at amount s = 0 the stage is exactly
 * transparent and the small-signal slope at x = 0 is unity for every amount.
 * The drive mapping D = 1 + 4*s keeps the maximum drive conservative (D = 5
 * at s = 1). Base-rate processing only: no oversampling, no makeup gain, no
 * added DC offset, and no state beyond the amount smoother.
 */
class SoftSaturator
{
public:
    SoftSaturator() = default;
    SoftSaturator (const SoftSaturator&) = delete;
    SoftSaturator& operator= (const SoftSaturator&) = delete;
    SoftSaturator (SoftSaturator&&) = delete;
    SoftSaturator& operator= (SoftSaturator&&) = delete;

    /**
     * Initializes the amount smoother for the given sample rate. All state
     * owned by this class is confined to this call.
     */
    void prepare (double sampleRate, int maxBlockSize);

    /** Restores the stage to its initial (transparent) behavior. */
    void reset() noexcept;

    /**
     * Sets the saturation amount. s is a normalized control in [0, 1] and is
     * clamped on entry. Smoothed at 30 ms.
     */
    void setAmount (float s) noexcept;

    /** Per-sample process. No allocation, locking, I/O, or string use. */
    float processSample (float x) noexcept;

    /**
     * Per-channel block helper. Advances the amount smoother once per block
     * (control-rate) and processes samples in place. No allocation, resizing,
     * locking, I/O, or string construction occurs here.
     */
    void process (float* channel,
                  int numSamples) noexcept;

private:
    static constexpr double amountSmoothingSeconds = 0.030;
    static constexpr float maxDriveScale = 4.0f;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedAmount_;
    float amount_ { 0.0f };
};

} // namespace shredspace
