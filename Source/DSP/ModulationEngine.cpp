#include "DSP/ModulationEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace shredspace
{

namespace
{
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kDepthSmoothingTimeSeconds = 0.030;
constexpr double kRateSmoothingTimeSeconds = 0.050;
constexpr double kDriftCutoffHz = 0.08;
constexpr double kDegreesToRadians = kPi / 180.0;
}

void ModulationEngine::prepare (double sampleRate, int maxBlockSize)
{
    static_cast<void> (maxBlockSize);

    sampleRate_ =
        std::isfinite (sampleRate) ? std::max (1.0, sampleRate) : 1.0;
    maximumDelaySamples_ = kMaximumDelaySeconds * sampleRate_;
    alphaDrift_ = std::exp (-2.0 * kPi * kDriftCutoffHz / sampleRate_);

    rateHz_.reset (sampleRate_, kRateSmoothingTimeSeconds);
    depthSeconds_.reset (sampleRate_, kDepthSmoothingTimeSeconds);
    random_.setSeedRandomly();
    reset();
}

void ModulationEngine::reset() noexcept
{
    rateHz_.setCurrentAndTargetValue (rateTargetHz_);
    depthSeconds_.setCurrentAndTargetValue (depthTargetSeconds_);
    phase_ = 0.0;
    drift_.fill (0.0);
    modulationOffsetSamples_.fill (0.0);
}

void ModulationEngine::setRate (float rateHz) noexcept
{
    rateTargetHz_ =
        std::isfinite (rateHz)
            ? std::clamp (rateHz,
                          static_cast<float> (kMinimumRateHz),
                          static_cast<float> (kMaximumRateHz))
            : static_cast<float> (kMinimumRateHz);
    rateHz_.setTargetValue (rateTargetHz_);
}

void ModulationEngine::setDepth (float depthMilliseconds) noexcept
{
    const auto clampedMilliseconds =
        std::isfinite (depthMilliseconds)
            ? std::clamp (depthMilliseconds,
                          0.0f,
                          static_cast<float> (
                              kMaximumDepthSeconds * 1000.0))
            : 0.0f;
    depthTargetSeconds_ = clampedMilliseconds * 0.001f;
    depthSeconds_.setTargetValue (depthTargetSeconds_);
}

void ModulationEngine::setPhaseOffset (float phaseOffsetDegrees) noexcept
{
    const auto clampedDegrees =
        std::isfinite (phaseOffsetDegrees)
            ? std::clamp (phaseOffsetDegrees, 0.0f, 180.0f)
            : 0.0f;
    phaseOffsetRadians_ =
        static_cast<double> (clampedDegrees) * kDegreesToRadians;
}

void ModulationEngine::setDriftAmount (float driftAmountSeconds) noexcept
{
    driftAmountSeconds_ =
        std::isfinite (driftAmountSeconds)
            ? std::clamp (driftAmountSeconds,
                          0.0f,
                          static_cast<float> (kMaximumDepthSeconds))
            : 0.0f;
}

void ModulationEngine::setBendExempt (bool shouldBeExempt) noexcept
{
    bendExempt_ = shouldBeExempt;
}

std::array<double, 2> ModulationEngine::processSample() noexcept
{
    const double rateHz = static_cast<double> (rateHz_.getNextValue());
    const double depthUserSeconds =
        static_cast<double> (depthSeconds_.getNextValue());
    const double depthEffective =
        calculateEffectiveDepth (depthUserSeconds, rateHz);
    const double oneMinusAlphaDrift = 1.0 - alphaDrift_;

    for (std::size_t channel = 0; channel < drift_.size(); ++channel)
    {
        const double noise =
            2.0 * static_cast<double> (random_.nextFloat()) - 1.0;
        drift_[channel] =
            alphaDrift_ * drift_[channel] + oneMinusAlphaDrift * noise;
        flushDenormal (drift_[channel]);

        const double channelPhase =
            phase_ + (channel == 0u ? 0.0 : phaseOffsetRadians_);
        const double lfoSeconds =
            depthEffective * std::sin (channelPhase);
        const double totalSeconds =
            lfoSeconds
            + static_cast<double> (driftAmountSeconds_) * drift_[channel];
        modulationOffsetSamples_[channel] = totalSeconds * sampleRate_;
    }

    phase_ += 2.0 * kPi * rateHz / sampleRate_;
    if (phase_ >= 2.0 * kPi)
        phase_ -= 2.0 * kPi;

    return modulationOffsetSamples_;
}

double ModulationEngine::getModulationOffsetSamples (int channel) const noexcept
{
    if (channel < 0 || channel >= 2)
        return 0.0;

    return modulationOffsetSamples_[static_cast<std::size_t> (channel)];
}

double ModulationEngine::applyToBaseDelay (
    double baseDelaySamples,
    double modulationOffsetSamples) const noexcept
{
    if (! std::isfinite (baseDelaySamples)
        || ! std::isfinite (modulationOffsetSamples))
    {
        return kMinimumDelaySamples;
    }

    return clampDelayPosition (baseDelaySamples + modulationOffsetSamples);
}

double ModulationEngine::calculateEffectiveDepth (
    double depthUserSeconds,
    double rateHz) const noexcept
{
    if (bendExempt_)
        return depthUserSeconds;

    if (! std::isfinite (rateHz)
        || rateHz <= std::numeric_limits<double>::epsilon())
    {
        return depthUserSeconds;
    }

    const double peakRateDeviation =
        2.0 * kPi * rateHz * depthUserSeconds;
    const double maxRateDeviation = std::pow (2.0, 25.0 / 1200.0) - 1.0;
    if (peakRateDeviation <= maxRateDeviation)
        return depthUserSeconds;

    const double maxDepthForRate =
        maxRateDeviation / (2.0 * kPi * rateHz);
    return std::min (depthUserSeconds, maxDepthForRate);
}

double ModulationEngine::clampDelayPosition (double delaySamples) const noexcept
{
    return std::isfinite (delaySamples)
        ? std::clamp (delaySamples,
                      kMinimumDelaySamples,
                      maximumDelaySamples_)
        : kMinimumDelaySamples;
}

void ModulationEngine::flushDenormal (double& value) noexcept
{
    if (std::abs (value) < std::numeric_limits<double>::min())
        value = 0.0;
}

} // namespace shredspace
