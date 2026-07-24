#include "DSP/PresenceGuard.h"

#include <algorithm>
#include <cmath>

namespace shredspace
{
namespace
{
constexpr float defaultThresholdDb = -24.0f;
constexpr float defaultKneeWidthDb = 12.0f;
constexpr float defaultPresenceFrequencyHz = 3000.0f;
constexpr float defaultMaxCutDb = 6.0f;
constexpr double detectorAttackSeconds = 0.005;
constexpr double detectorReleaseSeconds = 0.120;
constexpr double cutSmoothingSeconds = 0.020;
constexpr float detectorEpsilon = 1.0e-12f;
constexpr double detectorQ = 0.7071067811865476;
constexpr double detectorHalfBandRatio = 1.4142135623730951;
constexpr double peakingQ = 1.0;
} // namespace

PresenceGuard::PresenceGuard()
{
    thresholdDb_ = defaultThresholdDb;
    kneeWidthDb_ = defaultKneeWidthDb;
    presenceFrequencyHz_ = defaultPresenceFrequencyHz;
    maxCutDb_ = defaultMaxCutDb;
    effectiveMaxCutDb_ = defaultMaxCutDb;

    for (auto& smoother : smoothedCutDb_)
        smoother.setCurrentAndTargetValue (0.0f);

    recomputeDetectorCoefficients();
    recomputePeakingCoefficients();
}

void PresenceGuard::prepare (double sampleRate,
                             int maxBlockSize,
                             int numChannels)
{
    juce::ignoreUnused (maxBlockSize);

    sampleRate_ =
        (std::isfinite (sampleRate) && sampleRate > 0.0)
            ? sampleRate
            : 44100.0;
    numChannels_ = std::clamp (numChannels, 1, kMaxChannels);

    attackCoefficient_ = static_cast<float> (
        std::exp (-1.0 / (detectorAttackSeconds * sampleRate_)));
    releaseCoefficient_ = static_cast<float> (
        std::exp (-1.0 / (detectorReleaseSeconds * sampleRate_)));

    for (auto& smoother : smoothedCutDb_)
        smoother.reset (sampleRate_, cutSmoothingSeconds);

    reset();
}

void PresenceGuard::reset()
{
    for (int channel = 0; channel < kMaxChannels; ++channel)
    {
        detectorHighPassState_[channel] = {};
        detectorLowPassState_[channel] = {};
        peakingState_[channel] = {};
        energy2_[channel] = 0.0f;
        smoothedCutDb_[channel].setCurrentAndTargetValue (0.0f);
    }

    recomputeDetectorCoefficients();
    recomputePeakingCoefficients();
}

void PresenceGuard::setPresenceFrequency (float hz)
{
    if (std::isfinite (hz))
        presenceFrequencyHz_ = std::clamp (hz, 1500.0f, 6000.0f);
}

void PresenceGuard::setThresholdDb (float db)
{
    if (std::isfinite (db))
        thresholdDb_ = db;
}

void PresenceGuard::setKneeWidthDb (float db)
{
    if (std::isfinite (db))
        kneeWidthDb_ = std::max (db, detectorEpsilon);
}

void PresenceGuard::setMaxCutDb (float db)
{
    if (! std::isfinite (db))
        return;

    maxCutDb_ = std::clamp (db, 0.0f, 12.0f);
    effectiveMaxCutDb_ = maxCutDb_;
}

void PresenceGuard::setEffectiveMaxCutDb (float db)
{
    if (std::isfinite (db))
        effectiveMaxCutDb_ = std::clamp (db, 0.0f, 24.0f);
}

float PresenceGuard::processSample (int channel, float x) noexcept
{
    jassert (channel >= 0 && channel < numChannels_);
    if (channel < 0 || channel >= numChannels_)
        return x;

    float presence = processBiquad (
        x, detectorHighPassCoefficients_, detectorHighPassState_[channel]);
    presence = processBiquad (
        presence, detectorLowPassCoefficients_, detectorLowPassState_[channel]);

    const float presence2 = presence * presence;
    // Both sides are non-negative, so this is equivalent to comparing the
    // rectified detector input with sqrt (energy2_) without taking an extra
    // square root or changing the squared-state representation.
    const float envelopeCoefficient =
        presence2 > energy2_[channel]
            ? attackCoefficient_
            : releaseCoefficient_;

    energy2_[channel] =
        envelopeCoefficient * energy2_[channel]
        + (1.0f - envelopeCoefficient) * presence2;

    const float energy =
        std::sqrt (std::max (energy2_[channel], 0.0f) + detectorEpsilon);
    const float levelDb =
        20.0f * std::log10 (energy + detectorEpsilon);
    const float targetCutDb =
        computeCutDb (levelDb,
                      thresholdDb_,
                      kneeWidthDb_,
                      effectiveMaxCutDb_);
    smoothedCutDb_[channel].setTargetValue (targetCutDb);

    return processBiquad (
        x, peakingCoefficients_[channel], peakingState_[channel]);
}

void PresenceGuard::updateCoefficients()
{
    juce::ScopedNoDenormals noDenormals;

    for (auto& smoother : smoothedCutDb_)
        smoother.skip (RepeatToneProcessor::kControlRateDivider);

    recomputeDetectorCoefficients();
    recomputePeakingCoefficients();
}

float PresenceGuard::getCurrentCutDb (int channel) const noexcept
{
    jassert (channel >= 0 && channel < numChannels_);
    if (channel < 0 || channel >= numChannels_)
        return 0.0f;

    return smoothedCutDb_[channel].getCurrentValue();
}

float PresenceGuard::computeCutDb (float levelDb,
                                   float thresholdDb,
                                   float kneeWidthDb,
                                   float maxCutDb) noexcept
{
    if (levelDb <= thresholdDb || maxCutDb <= 0.0f)
        return 0.0f;

    const float safeKneeWidth =
        std::max (kneeWidthDb, detectorEpsilon);
    const float u =
        std::clamp ((levelDb - thresholdDb) / safeKneeWidth, 0.0f, 1.0f);
    const float smoothstep = 3.0f * u * u - 2.0f * u * u * u;
    return -std::max (maxCutDb, 0.0f) * smoothstep;
}

float PresenceGuard::processBiquad (
    float x,
    const BiquadCoefficients& coefficients,
    BiquadState& state) noexcept
{
    const double xd = static_cast<double> (x);
    const double yd = coefficients.b0 * xd + state.s1;
    state.s1 =
        coefficients.b1 * xd - coefficients.a1 * yd + state.s2;
    state.s2 = coefficients.b2 * xd - coefficients.a2 * yd;
    return static_cast<float> (yd);
}

void PresenceGuard::recomputeDetectorCoefficients()
{
    const double centreFrequency =
        static_cast<double> (presenceFrequencyHz_);

    detectorHighPassCoefficients_ =
        RepeatToneProcessor::makeHighPass (
            sampleRate_,
            centreFrequency / detectorHalfBandRatio,
            detectorQ);
    detectorLowPassCoefficients_ =
        RepeatToneProcessor::makeLowPass (
            sampleRate_,
            centreFrequency * detectorHalfBandRatio,
            detectorQ);
}

void PresenceGuard::recomputePeakingCoefficients()
{
    for (int channel = 0; channel < kMaxChannels; ++channel)
    {
        peakingCoefficients_[channel] =
            RepeatToneProcessor::makePeaking (
                sampleRate_,
                static_cast<double> (presenceFrequencyHz_),
                peakingQ,
                static_cast<double> (
                    smoothedCutDb_[channel].getCurrentValue()));
    }
}

} // namespace shredspace
