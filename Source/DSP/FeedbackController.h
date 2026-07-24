#pragma once

#include <algorithm>
#include <cmath>

namespace shredspace
{

/**
 * Converts a global T60 target into the feedback gain for one delay voice.
 *
 * T60 target changes are smoothed multiplicatively (in the log domain) with a
 * 50 ms time constant. All real-time methods are noexcept and perform no
 * allocation, locking, I/O, or string construction.
 */
class FeedbackController
{
public:
    static constexpr double kMaxFeedbackGain = 0.985;
    static constexpr double kT60SmoothingTimeSeconds = 0.050;

    FeedbackController() = default;

    /** Configures the 50 ms smoothing coefficient. */
    void prepare (double sampleRate) noexcept;

    /** Resets both ends of the smoother to a strictly positive T60 value. */
    void reset (double T60GlobalSeconds = 1.0) noexcept;

    /** Sets the global T60 target used by subsequent processing calls. */
    void setT60Target (double T60GlobalSeconds) noexcept;

    /**
     * Advances T60 smoothing by one sample and returns this voice's gain.
     */
    [[nodiscard]] double processSample (double delayTimeSeconds,
                                        double tailScaleVoice) noexcept;

    /**
     * Advances T60 smoothing by numSamples and returns this voice's gain.
     * This is suitable when one gain value is calculated per audio block.
     */
    [[nodiscard]] double processBlock (double delayTimeSeconds,
                                       double tailScaleVoice,
                                       int numSamples) noexcept;

    /**
     * Calculates an unsmoothed feedback gain.
     *
     * If T60_voice <= 0, T <= 0, or either value is non-finite, the safe
     * feedback value 0.0 is returned. This prevents NaN/inf and is always
     * within the unconditional feedback ceiling.
     */
    [[nodiscard]] static double calculateGain (double T60GlobalSeconds,
                                               double tailScaleVoice,
                                               double delayTimeSeconds) noexcept;

    [[nodiscard]] double getSmoothedT60() const noexcept;

private:
    static constexpr double kMinimumT60Seconds = 1.0e-6;

    [[nodiscard]] static double sanitiseT60 (double T60Seconds) noexcept;
    [[nodiscard]] static double applyFeedbackCeiling (double gain) noexcept;
    void advanceSmoothing (int numSamples) noexcept;

    double smoothingCoefficient_ {};
    double smoothedLogT60_ {};
    double targetLogT60_ {};
};

} // namespace shredspace
