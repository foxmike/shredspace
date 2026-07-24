#pragma once

#include "DSP/RepeatToneProcessor.h"

namespace shredspace
{

/**
 * Softens repeat transients by attenuating only the transient high band.
 *
 * A single high-pass filter extracts the high band. The complementary low
 * band is formed by subtraction so that zero attenuation reconstructs the
 * input.
 */
class RepeatTransientSoftener
{
public:
    RepeatTransientSoftener();

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void setCrossoverFrequency (float hz);
    void setAmountPercent (float percent);
    void setMaxAttenuationDb (float db);

    float processSample (int channel, float x);

    /** Advances crossover smoothing at the shared bounded control rate. */
    void updateCoefficients();

    /** Returns the most recently applied attenuation for a channel. */
    float getCurrentAttenuationDb (int channel) const;

    /** Pure transient-envelope to attenuation mapping used by processing. */
    static float computeAttenuationDb (float fastEnv,
                                       float slowEnv,
                                       float maxAttenuationDb);

private:
    static constexpr float kDefaultCrossoverHz = 2500.0f;
    static constexpr float kMinCrossoverHz = 1000.0f;
    static constexpr float kMaxCrossoverHz = 6000.0f;
    static constexpr float kMaximumAmountDb = 12.0f;

    void recomputeCoefficients();

    double sampleRate_ { 44100.0 };
    int numChannels_ { 2 };
    float fastAlpha_ {};
    float slowAlpha_ {};
    float maxAttenuationDb_ {};

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>
        crossoverFrequency_;

    BiquadCoefficients highPassCoefficients_;
    BiquadState highPassState_[2];
    float fastEnvelope_[2] {};
    float slowEnvelope_[2] {};
    float currentAttenuationDb_[2] {};
};

} // namespace shredspace
