#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>

namespace shredspace
{

/**
 * Generates stereo delay-time modulation for one delay voice.
 *
 * Rate and depth changes are smoothed in their required domains. The
 * process-path methods perform no allocation, locking, I/O, string
 * construction, or buffer resizing.
 */
class ModulationEngine
{
public:
    static constexpr double kMinimumRateHz = 0.05;
    static constexpr double kMaximumRateHz = 5.0;
    static constexpr double kMaximumDepthSeconds = 0.008;
    static constexpr double kMinimumDelaySamples = 8.0;
    static constexpr double kMaximumDelaySeconds = 8.25;

    ModulationEngine() = default;
    ModulationEngine (const ModulationEngine&) = delete;
    ModulationEngine& operator= (const ModulationEngine&) = delete;
    ModulationEngine (ModulationEngine&&) = delete;
    ModulationEngine& operator= (ModulationEngine&&) = delete;

    /** Configures smoothing, drift filtering, and all fixed-size state. */
    void prepare (double sampleRate, int maxBlockSize);

    /** Clears phase and drift state and snaps the smoothers to their targets. */
    void reset() noexcept;

    void setRate (float rateHz) noexcept;
    void setDepth (float depthMilliseconds) noexcept;
    void setPhaseOffset (float phaseOffsetDegrees) noexcept;
    void setDriftAmount (float driftAmountSeconds) noexcept;
    void setBendExempt (bool shouldBeExempt) noexcept;

    /** Advances one sample and returns left/right modulation offsets in samples. */
    [[nodiscard]] std::array<double, 2> processSample() noexcept;

    /** Returns the most recently generated offset for a channel. */
    [[nodiscard]] double getModulationOffsetSamples (int channel) const noexcept;

    /** Adds an offset to a base delay and clamps the resulting read position. */
    [[nodiscard]] double applyToBaseDelay (double baseDelaySamples,
                                           double modulationOffsetSamples) const noexcept;

private:
    [[nodiscard]] double calculateEffectiveDepth (double depthUserSeconds,
                                                   double rateHz) const noexcept;
    [[nodiscard]] double clampDelayPosition (double delaySamples) const noexcept;
    static void flushDenormal (double& value) noexcept;

    double sampleRate_ { 1.0 };
    double maximumDelaySamples_ { kMinimumDelaySamples };
    double alphaDrift_ {};
    double phase_ {};
    double phaseOffsetRadians_ {};
    float rateTargetHz_ { 0.5f };
    float depthTargetSeconds_ {};
    float driftAmountSeconds_ {};
    bool bendExempt_ {};

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>
        rateHz_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        depthSeconds_;
    juce::Random random_;

    std::array<double, 2> drift_ {};
    std::array<double, 2> modulationOffsetSamples_ {};
};

} // namespace shredspace
