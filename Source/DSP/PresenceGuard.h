#pragma once

#include "DSP/RepeatToneProcessor.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace shredspace
{

/**
 * Dynamically attenuates accumulating presence-band energy.
 *
 * Detector and peaking-filter state is fixed at two channels. Parameter
 * setters and updateCoefficients() are intended to be called at the control
 * rate; processSample() performs no allocation or coefficient construction.
 */
class PresenceGuard
{
public:
    PresenceGuard();

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void setPresenceFrequency (float hz);
    void setThresholdDb (float db);
    void setKneeWidthDb (float db);
    void setMaxCutDb (float db);
    void setEffectiveMaxCutDb (float db);

    float processSample (int channel, float x) noexcept;
    void updateCoefficients();

    float getCurrentCutDb (int channel) const noexcept;

    static float computeCutDb (float levelDb,
                               float thresholdDb,
                               float kneeWidthDb,
                               float maxCutDb) noexcept;

private:
    static float processBiquad (float x,
                                const BiquadCoefficients& coefficients,
                                BiquadState& state) noexcept;
    void recomputeDetectorCoefficients();
    void recomputePeakingCoefficients();

    static constexpr int kMaxChannels = 2;

    double sampleRate_ { 44100.0 };
    int numChannels_ { 2 };

    float presenceFrequencyHz_ { 3000.0f };
    float thresholdDb_ { -24.0f };
    float kneeWidthDb_ { 12.0f };
    float maxCutDb_ { 6.0f };
    float effectiveMaxCutDb_ { 6.0f };

    float attackCoefficient_ {};
    float releaseCoefficient_ {};

    BiquadCoefficients detectorHighPassCoefficients_;
    BiquadCoefficients detectorLowPassCoefficients_;
    BiquadCoefficients peakingCoefficients_[kMaxChannels];

    BiquadState detectorHighPassState_[kMaxChannels];
    BiquadState detectorLowPassState_[kMaxChannels];
    BiquadState peakingState_[kMaxChannels];
    float energy2_[kMaxChannels] {};

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        smoothedCutDb_[kMaxChannels];
};

} // namespace shredspace
