#include "DSP/FractionalDelay.h"

#include <algorithm>
#include <cmath>

namespace shredspace
{

namespace
{
constexpr double kPi = 3.141592653589793238462643383279502884;
}

void FractionalDelay::prepare (double sampleRate, int maxBlockSize)
{
    static_cast<void> (maxBlockSize);

    sampleRate_ = std::isfinite (sampleRate) ? std::max (1.0, sampleRate) : 1.0;
    maximumDelaySamples_ = kMaximumDelaySeconds * sampleRate_;
    maximumModulationOffsetSamples_ = kMaximumModulationSeconds * sampleRate_;

    const auto delayStorage = static_cast<std::size_t> (
        std::ceil (8.25 * sampleRate_));
    buffer_.assign (delayStorage + 4, 0.0f);
    reset();
}

void FractionalDelay::reset() noexcept
{
    std::fill (buffer_.begin(), buffer_.end(), 0.0f);
    writeIndex_ = 0;
    modulationOffsetSamples_ = 0.0;
    oldDelaySamples_ = kMinimumDelaySamples;
    newDelaySamples_ = kMinimumDelaySamples;
    bendStartDelaySamples_ = kMinimumDelaySamples;
    bendTargetDelaySamples_ = kMinimumDelaySamples;
    playbackRateFactor_ = 1.0;
    transitionPosition_ = 0;
    transitionLength_ = 0;
    activeTransition_ = ActiveTransition::none;
}

void FractionalDelay::setDelaySamples (double delaySamples) noexcept
{
    const auto clampedDelay = clampReadPosition (delaySamples);
    oldDelaySamples_ = clampedDelay;
    newDelaySamples_ = clampedDelay;
    bendStartDelaySamples_ = clampedDelay;
    bendTargetDelaySamples_ = clampedDelay;
    playbackRateFactor_ = 1.0;
    transitionPosition_ = 0;
    transitionLength_ = 0;
    activeTransition_ = ActiveTransition::none;
}

void FractionalDelay::setSmoothDelaySamples (double targetDelaySamples,
                                             double glideMilliseconds) noexcept
{
    oldDelaySamples_ = currentNominalDelay();
    newDelaySamples_ = clampReadPosition (targetDelaySamples);
    transitionPosition_ = 0;
    transitionLength_ = glideLengthInSamples (glideMilliseconds);
    playbackRateFactor_ = 1.0;
    activeTransition_ = ActiveTransition::smooth;
}

void FractionalDelay::setBendDelaySamples (double targetDelaySamples,
                                           double glideMilliseconds) noexcept
{
    bendStartDelaySamples_ = currentNominalDelay();
    bendTargetDelaySamples_ = clampReadPosition (targetDelaySamples);
    transitionPosition_ = 0;
    transitionLength_ = glideLengthInSamples (glideMilliseconds);

    // For time measured in samples, p(t) = 1 - dD/dt. Decreasing delay
    // therefore raises pitch. Intentional Bend motion is exempt from any
    // modulation pitch cap.
    const auto delayPerSample = (bendTargetDelaySamples_ - bendStartDelaySamples_)
                              / static_cast<double> (transitionLength_);
    playbackRateFactor_ = 1.0 - delayPerSample;
    activeTransition_ = ActiveTransition::bend;
}

void FractionalDelay::setDelaySamples (double targetDelaySamples,
                                       double glideMilliseconds,
                                       TransitionMode mode) noexcept
{
    if (mode == TransitionMode::smooth)
        setSmoothDelaySamples (targetDelaySamples, glideMilliseconds);
    else
        setBendDelaySamples (targetDelaySamples, glideMilliseconds);
}

void FractionalDelay::setModulationOffsetSamples (double offsetSamples) noexcept
{
    modulationOffsetSamples_ =
        std::isfinite (offsetSamples)
            ? std::clamp (offsetSamples,
                          -maximumModulationOffsetSamples_,
                          maximumModulationOffsetSamples_)
            : 0.0;
}

float FractionalDelay::processSample (float input) noexcept
{
    if (buffer_.empty())
        return 0.0f;

    buffer_[writeIndex_] = input;

    float output = 0.0f;
    if (activeTransition_ == ActiveTransition::smooth)
    {
        const double lambda = transitionLambda();
        const float r_old = readHead (oldDelaySamples_);
        const float r_new = readHead (newDelaySamples_);
        output = static_cast<float> (
            std::cos (kPi * lambda / 2.0) * r_old
            + std::sin (kPi * lambda / 2.0) * r_new);
    }
    else if (activeTransition_ == ActiveTransition::bend)
    {
        const double lambda = transitionLambda();
        const double D_start = bendStartDelaySamples_;
        const double D_target = bendTargetDelaySamples_;
        const double D = D_start + lambda * (D_target - D_start);
        output = readHead (D);
    }
    else
    {
        output = readHead (newDelaySamples_);
    }

    writeIndex_ = (writeIndex_ + 1) % buffer_.size();
    advanceTransition();
    return output;
}

double FractionalDelay::getCurrentDelaySamples() const noexcept
{
    return currentNominalDelay();
}

double FractionalDelay::getPlaybackRateFactor() const noexcept
{
    return playbackRateFactor_;
}

bool FractionalDelay::isTransitioning() const noexcept
{
    return activeTransition_ != ActiveTransition::none;
}

double FractionalDelay::clampReadPosition (double position) const noexcept
{
    return std::isfinite (position)
        ? std::clamp (position, kMinimumDelaySamples, maximumDelaySamples_)
        : kMinimumDelaySamples;
}

double FractionalDelay::clampGlideMilliseconds (double milliseconds) const noexcept
{
    return std::isfinite (milliseconds)
        ? std::clamp (milliseconds,
                      kMinimumGlideMilliseconds,
                      kMaximumGlideMilliseconds)
        : kMinimumGlideMilliseconds;
}

std::uint64_t FractionalDelay::glideLengthInSamples (double milliseconds) const noexcept
{
    const auto clampedMilliseconds = clampGlideMilliseconds (milliseconds);
    return std::max<std::uint64_t> (
        1,
        static_cast<std::uint64_t> (
            std::ceil (clampedMilliseconds * 0.001 * sampleRate_)));
}

double FractionalDelay::transitionLambda() const noexcept
{
    if (transitionLength_ == 0)
        return 1.0;

    return std::min (1.0,
                     static_cast<double> (transitionPosition_)
                         / static_cast<double> (transitionLength_));
}

double FractionalDelay::currentNominalDelay() const noexcept
{
    const auto lambda = transitionLambda();

    if (activeTransition_ == ActiveTransition::smooth)
        return oldDelaySamples_ + lambda * (newDelaySamples_ - oldDelaySamples_);

    if (activeTransition_ == ActiveTransition::bend)
        return bendStartDelaySamples_
             + lambda * (bendTargetDelaySamples_ - bendStartDelaySamples_);

    return newDelaySamples_;
}

float FractionalDelay::readHead (double baseDelaySamples) const noexcept
{
    const auto finalReadPosition =
        clampReadPosition (baseDelaySamples + modulationOffsetSamples_);
    return interpolate (finalReadPosition);
}

float FractionalDelay::interpolate (double finalDelaySamples) const noexcept
{
    const double absolutePosition =
        static_cast<double> (writeIndex_) - finalDelaySamples;
    const double flooredPosition = std::floor (absolutePosition);
    const auto i = static_cast<std::int64_t> (flooredPosition);
    const double mu = absolutePosition - flooredPosition;

    const double L_minus1 = -mu*(mu-1)*(mu-2)/6;
    const double L_0      =  (mu+1)*(mu-1)*(mu-2)/2;
    const double L_1      = -(mu+1)*mu*(mu-2)/2;
    const double L_2      =  (mu+1)*mu*(mu-1)/6;

    const double xMinus1 = buffer_[wrapIndex (i - 1)];
    const double x0 = buffer_[wrapIndex (i)];
    const double x1 = buffer_[wrapIndex (i + 1)];
    const double x2 = buffer_[wrapIndex (i + 2)];

    return static_cast<float> (
        L_minus1 * xMinus1 + L_0 * x0 + L_1 * x1 + L_2 * x2);
}

std::size_t FractionalDelay::wrapIndex (std::int64_t index) const noexcept
{
    const auto size = static_cast<std::int64_t> (buffer_.size());
    const auto wrapped = ((index % size) + size) % size;
    return static_cast<std::size_t> (wrapped);
}

void FractionalDelay::advanceTransition() noexcept
{
    if (activeTransition_ == ActiveTransition::none)
        return;

    if (transitionPosition_ < transitionLength_)
        ++transitionPosition_;

    if (transitionPosition_ < transitionLength_)
        return;

    if (activeTransition_ == ActiveTransition::smooth)
    {
        // The new head becomes the sole head when lambda reaches one.
        oldDelaySamples_ = newDelaySamples_;
    }
    else
    {
        oldDelaySamples_ = bendTargetDelaySamples_;
        newDelaySamples_ = bendTargetDelaySamples_;
    }

    bendStartDelaySamples_ = newDelaySamples_;
    bendTargetDelaySamples_ = newDelaySamples_;
    playbackRateFactor_ = 1.0;
    activeTransition_ = ActiveTransition::none;
    transitionPosition_ = 0;
    transitionLength_ = 0;
}

} // namespace shredspace
