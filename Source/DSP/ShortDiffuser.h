#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

namespace shredspace
{

/**
 * Restricted three-stage short-diffusion stage for the repeat loop.
 *
 * Implements exactly three cascaded first-order allpass sections per
 * channel, using the transfer function H(z) = (-a + z^-M) / (1 - a*z^-M)
 * realized by the difference equation y[n] = -a*x[n] + x[n-M] + a*y[n-M].
 * The left and right channels use independent stage delays; coefficients a
 * are shared across channels. No independent late-reverb tail, no feedback
 * across stages beyond the allpass recursions themselves, and no modulation
 * of the stage delays are introduced.
 *
 * The user amount d in [0, 0.40] performs an equal-power blend
 * z_diff = cos(pi*d/2)*z + sin(pi*d/2)*allpass(z), where z is the dry input
 * to the diffuser and allpass(z) is the three-stage cascade output.
 */
class ShortDiffuser
{
public:
    ShortDiffuser() = default;
    ShortDiffuser (const ShortDiffuser&) = delete;
    ShortDiffuser& operator= (const ShortDiffuser&) = delete;
    ShortDiffuser (ShortDiffuser&&) = delete;
    ShortDiffuser& operator= (ShortDiffuser&&) = delete;

    /**
     * Allocates every stage delay line, sized for the longest stage at the
     * highest supported sample rate (192 kHz). All allocation performed by
     * this class is confined to this call; the process path never resizes.
     */
    void prepare (double sampleRate, int maxBlockSize);

    /** Clears all delay-line state to zero with no denormal residue. */
    void reset() noexcept;

    /**
     * Sets the diffusion amount. d is a normalized control in [0, 0.40] and
     * is clamped to that range on entry before the equal-power law. Smoothed
     * linearly at 30 ms.
     */
    void setAmount (float d) noexcept;

    /**
     * Per-sample process for one channel. channelIndex 0 selects the left
     * stage delays, 1 selects the right stage delays. No allocation,
     * resizing, locking, I/O, or string construction occurs here.
     */
    [[nodiscard]] float processSample (float z, int channelIndex) noexcept;

    /**
     * Per-channel block helper. Advances the amount smoother once per block
     * (control-rate) and processes samples in place. channelIndex 0 selects
     * the left stage delays, 1 the right stage delays. No allocation,
     * resizing, locking, I/O, or string construction occurs here.
     */
    void process (float* channel, int channelIndex, int numSamples) noexcept;

private:
    struct Stage
    {
        std::vector<float> buffer;
        int writePos { 0 };
        int delaySamples { 1 };
        float coeff { 0.0f };
    };

    static constexpr double amountSmoothingSeconds = 0.030;
    static constexpr double maxSupportedSampleRate = 192000.0;
    static constexpr double maxAmount = 0.40;

    // Stage delays in milliseconds; left channel then right channel.
    static constexpr double stageMsLeft[3]  = { 3.1, 4.7, 6.9 };
    static constexpr double stageMsRight[3] = { 3.7, 5.3, 7.9 };
    static constexpr float stageCoeffs[3]    = { 0.45f, 0.38f, 0.32f };

    // Longest stage delay, sized for 192 kHz with a one-sample guard.
    static constexpr int stageCapacity = 1518;

    void configureStage (Stage& stage, double milliseconds, float coeff) noexcept;
    [[nodiscard]] float runStage (Stage& stage, float x) noexcept;

    Stage stages_[2][3];
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedAmount_;
    double sampleRate_ { 1.0 };
    float amount_ { 0.0f };
};

} // namespace shredspace
