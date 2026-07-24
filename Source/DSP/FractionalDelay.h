#pragma once

#include <cstdint>
#include <vector>

namespace shredspace
{

/**
 * A single-channel fractional delay line with two transition read heads.
 *
 * Delay positions are measured backwards from the current write position. In
 * the interpolation routine, an absolute circular-buffer position i + mu is
 * represented by i = floor(position) and mu in [0, 1). The four samples are
 * therefore x[i - 1], x[i], x[i + 1], and x[i + 2], with every index wrapped
 * into the circular buffer.
 */
class FractionalDelay
{
public:
    enum class TransitionMode
    {
        smooth,
        bend
    };

    static constexpr double kMinimumDelaySamples = 8.0;
    static constexpr double kMaximumDelaySeconds = 8.25;
    static constexpr double kMinimumGlideMilliseconds = 5.0;
    static constexpr double kMaximumGlideMilliseconds = 1000.0;

    /**
     * Modulation is bounded to +/- 250 ms before it is added to each read
     * head's base delay. The resulting final position is clamped again.
     */
    static constexpr double kMaximumModulationSeconds = 0.250;

    FractionalDelay() = default;
    FractionalDelay (const FractionalDelay&) = delete;
    FractionalDelay& operator= (const FractionalDelay&) = delete;
    FractionalDelay (FractionalDelay&&) = delete;
    FractionalDelay& operator= (FractionalDelay&&) = delete;

    /**
     * Allocates ceil(8.25 * sampleRate) + 4 guard samples.
     * This is the only member function that allocates buffer memory.
     */
    void prepare (double sampleRate, int maxBlockSize);

    /** Zeroes the buffer and all read-head state without reallocating. */
    void reset() noexcept;

    /** Changes the delay immediately, without a transition. */
    void setDelaySamples (double delaySamples) noexcept;

    /** Starts a stationary two-head equal-power crossfade. */
    void setSmoothDelaySamples (double targetDelaySamples,
                                double glideMilliseconds) noexcept;

    /** Starts a single-head delay ramp, intentionally producing pitch bend. */
    void setBendDelaySamples (double targetDelaySamples,
                              double glideMilliseconds) noexcept;

    /** Selects Smooth or Bend behavior for a new delay target. */
    void setDelaySamples (double targetDelaySamples,
                          double glideMilliseconds,
                          TransitionMode mode) noexcept;

    void setModulationOffsetSamples (double offsetSamples) noexcept;

    /**
     * Writes one input sample and returns the delayed output sample.
     * No memory allocation, resizing, locking, I/O, or exceptions occur here.
     */
    [[nodiscard]] float processSample (float input) noexcept;

    [[nodiscard]] double getCurrentDelaySamples() const noexcept;
    [[nodiscard]] double getPlaybackRateFactor() const noexcept;
    [[nodiscard]] bool isTransitioning() const noexcept;

private:
    enum class ActiveTransition
    {
        none,
        smooth,
        bend
    };

    [[nodiscard]] double clampReadPosition (double position) const noexcept;
    [[nodiscard]] double clampGlideMilliseconds (double milliseconds) const noexcept;
    [[nodiscard]] std::uint64_t glideLengthInSamples (double milliseconds) const noexcept;
    [[nodiscard]] double transitionLambda() const noexcept;
    [[nodiscard]] double currentNominalDelay() const noexcept;
    [[nodiscard]] float readHead (double baseDelaySamples) const noexcept;
    [[nodiscard]] float interpolate (double finalDelaySamples) const noexcept;
    [[nodiscard]] std::size_t wrapIndex (std::int64_t index) const noexcept;
    void advanceTransition() noexcept;

    std::vector<float> buffer_;
    std::size_t writeIndex_ {};
    double sampleRate_ { 1.0 };
    double maximumDelaySamples_ { kMinimumDelaySamples };
    double maximumModulationOffsetSamples_ { kMaximumModulationSeconds };
    double modulationOffsetSamples_ {};

    double oldDelaySamples_ { kMinimumDelaySamples };
    double newDelaySamples_ { kMinimumDelaySamples };
    double bendStartDelaySamples_ { kMinimumDelaySamples };
    double bendTargetDelaySamples_ { kMinimumDelaySamples };
    double playbackRateFactor_ { 1.0 };

    std::uint64_t transitionPosition_ {};
    std::uint64_t transitionLength_ {};
    ActiveTransition activeTransition_ { ActiveTransition::none };
};

} // namespace shredspace
