#include "DSP/RepeatTransientSoftener.h"

#include <algorithm>
#include <cmath>

namespace shredspace
{

namespace
{
constexpr double fastEnvelopeTauSeconds = 0.0015;
constexpr double slowEnvelopeTauSeconds = 0.035;
constexpr float envelopeEpsilon = 1.0e-6f;
constexpr double crossoverSmoothingSeconds = 0.05;
constexpr double butterworthQ = 0.70710678;
}

RepeatTransientSoftener::RepeatTransientSoftener()
{
    crossoverFrequency_.setCurrentAndTargetValue (kDefaultCrossoverHz);
    recomputeCoefficients();
}

void RepeatTransientSoftener::prepare (double sampleRate,
                                       int maxBlockSize,
                                       int numChannels)
{
    juce::ignoreUnused (maxBlockSize);

    sampleRate_ =
        std::isfinite (sampleRate) ? std::max (1.0, sampleRate) : 44100.0;
    numChannels_ = std::clamp (numChannels, 1, 2);

    fastAlpha_ = static_cast<float> (
        std::exp (-1.0 / (fastEnvelopeTauSeconds * sampleRate_)));
    slowAlpha_ = static_cast<float> (
        std::exp (-1.0 / (slowEnvelopeTauSeconds * sampleRate_)));

    crossoverFrequency_.reset (sampleRate_, crossoverSmoothingSeconds);
    reset();
}

void RepeatTransientSoftener::reset()
{
    for (int channel = 0; channel < 2; ++channel)
    {
        highPassState_[channel] = {};
        fastEnvelope_[channel] = 0.0f;
        slowEnvelope_[channel] = 0.0f;
        currentAttenuationDb_[channel] = 0.0f;
    }

    crossoverFrequency_.setCurrentAndTargetValue (
        crossoverFrequency_.getTargetValue());
    recomputeCoefficients();
}

void RepeatTransientSoftener::setCrossoverFrequency (float hz)
{
    const float frequency =
        std::isfinite (hz) ? hz : kDefaultCrossoverHz;
    crossoverFrequency_.setTargetValue (
        std::clamp (frequency, kMinCrossoverHz, kMaxCrossoverHz));
}

void RepeatTransientSoftener::setAmountPercent (float percent)
{
    const float finitePercent = std::isfinite (percent) ? percent : 0.0f;
    setMaxAttenuationDb (
        kMaximumAmountDb
        * std::clamp (finitePercent, 0.0f, 100.0f) / 100.0f);
}

void RepeatTransientSoftener::setMaxAttenuationDb (float db)
{
    const float finiteDb = std::isfinite (db) ? db : 0.0f;
    maxAttenuationDb_ = std::clamp (finiteDb, 0.0f, kMaximumAmountDb);
}

float RepeatTransientSoftener::processSample (int channel, float x)
{
    jassert (channel >= 0 && channel < numChannels_);

    auto& state = highPassState_[channel];
    const double input = static_cast<double> (x);
    const double high = highPassCoefficients_.b0 * input + state.s1;
    state.s1 = highPassCoefficients_.b1 * input
               - highPassCoefficients_.a1 * high + state.s2;
    state.s2 = highPassCoefficients_.b2 * input
               - highPassCoefficients_.a2 * high;

    const float zHigh = static_cast<float> (high);
    const float zLow = x - zHigh;
    const float highMagnitude = std::abs (zHigh);

    fastEnvelope_[channel] =
        fastAlpha_ * fastEnvelope_[channel]
        + (1.0f - fastAlpha_) * highMagnitude;
    slowEnvelope_[channel] =
        slowAlpha_ * slowEnvelope_[channel]
        + (1.0f - slowAlpha_) * highMagnitude;

    const float attenuationDb =
        computeAttenuationDb (fastEnvelope_[channel],
                              slowEnvelope_[channel],
                              maxAttenuationDb_);
    currentAttenuationDb_[channel] = attenuationDb;

    const float gain = attenuationDb == 0.0f
                           ? 1.0f
                           : std::pow (10.0f, attenuationDb / 20.0f);
    return zLow + gain * zHigh;
}

void RepeatTransientSoftener::updateCoefficients()
{
    crossoverFrequency_.skip (RepeatToneProcessor::kControlRateDivider);
    recomputeCoefficients();
}

float RepeatTransientSoftener::getCurrentAttenuationDb (int channel) const
{
    jassert (channel >= 0 && channel < numChannels_);
    return currentAttenuationDb_[channel];
}

float RepeatTransientSoftener::computeAttenuationDb (
    float fastEnv,
    float slowEnv,
    float maxAttenuationDb)
{
    const float transient =
        std::max (0.0f, (fastEnv - slowEnv) / (slowEnv + envelopeEpsilon));
    return -maxAttenuationDb * std::clamp (transient, 0.0f, 1.0f);
}

void RepeatTransientSoftener::recomputeCoefficients()
{
    highPassCoefficients_ = RepeatToneProcessor::makeHighPass (
        sampleRate_,
        static_cast<double> (crossoverFrequency_.getCurrentValue()),
        butterworthQ);
}

} // namespace shredspace
