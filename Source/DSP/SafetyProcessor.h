#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <vector>

namespace shredspace
{

/**
 * Applies the non-optional wet-path protections and supplies an optional
 * RMS-derived feedback reduction.
 *
 * prepare() owns all allocation. The remaining methods are real-time safe and
 * perform no allocation, locking, I/O, or large delay-buffer clearing.
 */
class SafetyProcessor
{
public:
    static constexpr float kMaximumFeedbackGain = 0.985f;
    static constexpr float kLimiterThreshold = 0.90f;

    SafetyProcessor() = default;

    /** Allocates channel state and calculates the detector coefficients. */
    void prepare (double sampleRate, int maxBlockSize, int numChannels);

    /** Clears filters, detector state, telemetry, and pending requests. */
    void reset() noexcept;

    /** Enables or bypasses the optional RMS-derived feedback reduction. */
    void setLimiterEnabled (bool shouldBeEnabled) noexcept;

    /**
     * Validates and DC-blocks an in-place wet buffer while advancing the RMS
     * detector. Invalid state mutes the complete current wet block.
     */
    void processBlock (juce::AudioBuffer<float>& wetBuffer) noexcept;

    /**
     * Applies the mandatory ceiling and, when enabled, the detector-derived
     * reduction to a requested feedback gain.
     */
    [[nodiscard]] float applyFeedbackCeiling (float requestedGain) noexcept;

    /** Reads the sticky intervention flag with acquire semantics. */
    [[nodiscard]] bool safetyInterventionOccurred() const noexcept;

    /**
     * Atomically consumes a request for the delay owner to clear its large
     * buffers at a bounded block boundary or incrementally.
     */
    [[nodiscard]] bool consumeBufferClearRequest() noexcept;

private:
    static constexpr double kDcBlockerR = 0.995;
    static constexpr double kAttackSeconds = 0.002;
    static constexpr double kReleaseSeconds = 0.250;
    static constexpr double kEnergyEpsilon = 1.0e-12;
    static constexpr double kDenormalThreshold = 1.0e-30;

    [[nodiscard]] static double flushDenormal (double value) noexcept;
    void resetLightweightState() noexcept;
    void publishInvalidState() noexcept;
    void muteAndPublishInvalidState (
        juce::AudioBuffer<float>& wetBuffer) noexcept;

    std::vector<double> previousInput_;
    std::vector<double> previousOutput_;

    double feedbackEnergy2_ {};
    double attackAlpha_ {};
    double releaseAlpha_ {};
    int maxBlockSize_ {};
    int preparedChannels_ {};
    bool limiterEnabled_ {};

    std::atomic<bool> safetyIntervention_ { false };
    std::atomic<bool> bufferClearRequested_ { false };
};

} // namespace shredspace
