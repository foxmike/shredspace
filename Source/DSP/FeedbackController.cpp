#include "DSP/FeedbackController.h"

#include <algorithm>
#include <cmath>

namespace shredspace
{

void FeedbackController::prepare (double sampleRate) noexcept
{
    const auto safeSampleRate =
        std::isfinite (sampleRate) ? std::max (1.0, sampleRate) : 1.0;
    smoothingCoefficient_ =
        std::exp (-1.0 / (kT60SmoothingTimeSeconds * safeSampleRate));
    crossFeedbackSmoother_.reset (
        safeSampleRate, kCrossFeedbackSmoothingTimeSeconds);
    crossFeedbackSmoother_.setCurrentAndTargetValue (crossFeedbackTarget_);
}

void FeedbackController::reset (double T60GlobalSeconds) noexcept
{
    const auto safeT60 = sanitiseT60 (T60GlobalSeconds);
    smoothedLogT60_ = std::log (safeT60);
    targetLogT60_ = smoothedLogT60_;
    crossFeedbackSmoother_.setCurrentAndTargetValue (crossFeedbackTarget_);
}

void FeedbackController::setT60Target (double T60GlobalSeconds) noexcept
{
    targetLogT60_ = std::log (sanitiseT60 (T60GlobalSeconds));
}

void FeedbackController::setCrossFeedback (float q) noexcept
{
    crossFeedbackTarget_ =
        std::isfinite (q) ? std::clamp (q, 0.0f, 1.0f) : 0.0f;
    crossFeedbackSmoother_.setTargetValue (crossFeedbackTarget_);
}

double FeedbackController::processSample (double delayTimeSeconds,
                                          double tailScaleVoice) noexcept
{
    advanceSmoothing (1);
    return calculateGain (getSmoothedT60(),
                          tailScaleVoice,
                          delayTimeSeconds);
}

FeedbackController::StereoFeedback FeedbackController::processStereoSample (
    float zLeft,
    float zRight,
    double delayTimeSeconds,
    double tailScaleVoice) noexcept
{
    const auto gain = static_cast<float> (
        processSample (delayTimeSeconds, tailScaleVoice));
    const float q = crossFeedbackSmoother_.getNextValue();
    const float direct = 1.0f - q;

    return {
        gain * (direct * zLeft + q * zRight),
        gain * (q * zLeft + direct * zRight)
    };
}

double FeedbackController::processBlock (double delayTimeSeconds,
                                         double tailScaleVoice,
                                         int numSamples) noexcept
{
    advanceSmoothing (numSamples);
    return calculateGain (getSmoothedT60(),
                          tailScaleVoice,
                          delayTimeSeconds);
}

double FeedbackController::calculateGain (double T60GlobalSeconds,
                                          double tailScaleVoice,
                                          double delayTimeSeconds) noexcept
{
    // T60_voice = T60_global * tailScale_voice.
    const double T60_voice = T60GlobalSeconds * tailScaleVoice;
    const double T = delayTimeSeconds;

    if (! std::isfinite (T60_voice) || ! std::isfinite (T)
        || T60_voice <= 0.0 || T <= 0.0)
    {
        return applyFeedbackCeiling (0.0);
    }

    // g_requested = 10^(-3 * T / T60_voice).
    const double g_requested = std::pow (10.0, -3.0 * T / T60_voice);

    // g_final = min(g_requested, 0.985).
    return applyFeedbackCeiling (g_requested);
}

double FeedbackController::getSmoothedT60() const noexcept
{
    return std::exp (smoothedLogT60_);
}

double FeedbackController::sanitiseT60 (double T60Seconds) noexcept
{
    return std::isfinite (T60Seconds) && T60Seconds > 0.0
        ? std::max (T60Seconds, kMinimumT60Seconds)
        : kMinimumT60Seconds;
}

double FeedbackController::applyFeedbackCeiling (double gain) noexcept
{
    const auto finiteGain = std::isfinite (gain) ? gain : 0.0;
    return std::min (finiteGain, kMaxFeedbackGain);
}

void FeedbackController::advanceSmoothing (int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    const auto blockCoefficient =
        std::pow (smoothingCoefficient_, static_cast<double> (numSamples));
    smoothedLogT60_ =
        targetLogT60_
        + blockCoefficient * (smoothedLogT60_ - targetLogT60_);
}

} // namespace shredspace
