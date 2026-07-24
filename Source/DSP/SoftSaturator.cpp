#include "DSP/SoftSaturator.h"

#include <algorithm>
#include <cmath>

namespace shredspace
{
namespace
{
constexpr float defaultAmount = 0.0f;
} // namespace

void SoftSaturator::prepare (double sampleRate, int maxBlockSize)
{
    juce::ignoreUnused (maxBlockSize);

    const double safeSampleRate =
        (std::isfinite (sampleRate) && sampleRate > 0.0)
            ? sampleRate
            : 44100.0;

    smoothedAmount_.reset (safeSampleRate, amountSmoothingSeconds);
    reset();
}

void SoftSaturator::reset() noexcept
{
    amount_ = defaultAmount;
    smoothedAmount_.setCurrentAndTargetValue (defaultAmount);
}

void SoftSaturator::setAmount (float s) noexcept
{
    if (! std::isfinite (s))
    {
        smoothedAmount_.setTargetValue (defaultAmount);
        return;
    }

    const float clamped = std::clamp (s, 0.0f, 1.0f);
    amount_ = clamped;
    smoothedAmount_.setTargetValue (clamped);
}

float SoftSaturator::processSample (float x) noexcept
{
    const float s = smoothedAmount_.getCurrentValue();

    if (s <= 0.0f)
        return x;

    const float D = 1.0f + maxDriveScale * s;
    const float shaped =
        static_cast<float> (std::tanh (static_cast<double> (D * x)) / D);
    return (1.0f - s) * x + s * shaped;
}

void SoftSaturator::process (float* channel, int numSamples) noexcept
{
    if (channel == nullptr || numSamples <= 0)
        return;

    if (! smoothedAmount_.isSmoothing())
    {
        const float s = smoothedAmount_.getCurrentValue();
        if (s <= 0.0f)
        {
            // Transparent path: leave the block untouched.
            return;
        }

        const float D = 1.0f + maxDriveScale * s;
        const double Dd = static_cast<double> (D);
        const float oneMinusS = 1.0f - s;

        for (int i = 0; i < numSamples; ++i)
        {
            const float x = channel[i];
            const float shaped =
                static_cast<float> (std::tanh (Dd * static_cast<double> (x)) / Dd);
            channel[i] = oneMinusS * x + s * shaped;
        }
        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const float x = channel[i];
        const float s = smoothedAmount_.getNextValue();
        const float D = 1.0f + maxDriveScale * s;
        const float shaped =
            static_cast<float> (std::tanh (static_cast<double> (D * x)) / D);
        channel[i] = (1.0f - s) * x + s * shaped;
    }
}

} // namespace shredspace
