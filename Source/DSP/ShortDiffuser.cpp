#include "DSP/ShortDiffuser.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace shredspace
{
namespace
{
constexpr float defaultAmount = 0.0f;

float blendAmount (float z, float allpassOut, float d) noexcept
{
    const float angle =
        std::numbers::pi_v<float> * 0.5f * d; // pi * d / 2
    const float c = std::cos (angle);
    const float s = std::sin (angle);
    return c * z + s * allpassOut;
}
} // namespace

void ShortDiffuser::configureStage (Stage& stage,
                                    double milliseconds,
                                    float coeff) noexcept
{
    stage.coeff = coeff;

    const double samples = milliseconds * sampleRate_;
    int delay = static_cast<int> (std::round (samples));
    if (delay < 1)
        delay = 1;
    if (delay > stageCapacity)
        delay = stageCapacity;

    stage.delaySamples = delay;
    stage.writePos = 0;
    std::fill (stage.buffer.begin(), stage.buffer.end(), 0.0f);
}

void ShortDiffuser::prepare (double sampleRate, int maxBlockSize)
{
    juce::ignoreUnused (maxBlockSize);

    const double safeSampleRate =
        (std::isfinite (sampleRate) && sampleRate > 0.0)
            ? sampleRate
            : 44100.0;
    sampleRate_ = safeSampleRate;

    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < 3; ++i)
        {
            stages_[ch][i].buffer.assign (
                static_cast<std::size_t> (stageCapacity), 0.0f);
        }
    }

    for (int i = 0; i < 3; ++i)
    {
        configureStage (stages_[0][i], stageMsLeft[i], stageCoeffs[i]);
        configureStage (stages_[1][i], stageMsRight[i], stageCoeffs[i]);
    }

    smoothedAmount_.reset (safeSampleRate, amountSmoothingSeconds);
    reset();
}

void ShortDiffuser::reset() noexcept
{
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < 3; ++i)
        {
            stages_[ch][i].writePos = 0;
            std::fill (stages_[ch][i].buffer.begin(),
                       stages_[ch][i].buffer.end(),
                       0.0f);
        }
    }

    amount_ = defaultAmount;
    smoothedAmount_.setCurrentAndTargetValue (defaultAmount);
}

void ShortDiffuser::setAmount (float d) noexcept
{
    if (! std::isfinite (d))
    {
        smoothedAmount_.setTargetValue (defaultAmount);
        return;
    }

    const float clamped = std::clamp (d, 0.0f, static_cast<float> (maxAmount));
    amount_ = clamped;
    smoothedAmount_.setTargetValue (clamped);
}

float ShortDiffuser::runStage (Stage& stage, float x) noexcept
{
    // y[n] = -a*x[n] + b[n-M], with b[n] = x[n] + a*y[n] so that b[n-M]
    // expands to x[n-M] + a*y[n-M], matching the required difference
    // equation y[n] = -a*x[n] + x[n-M] + a*y[n-M].
    const float a = stage.coeff;
    const int delay = stage.delaySamples;
    const int idx = stage.writePos;

    const float delayed = stage.buffer[static_cast<std::size_t> (idx)];
    const float y = -a * x + delayed;
    stage.buffer[static_cast<std::size_t> (idx)] = x + a * y;

    int next = idx + 1;
    if (next >= delay)
        next = 0;
    stage.writePos = next;

    return y;
}

float ShortDiffuser::processSample (float z, int channelIndex) noexcept
{
    const int ch = (channelIndex == 0) ? 0 : 1;

    float ap = z;
    for (int i = 0; i < 3; ++i)
        ap = runStage (stages_[ch][i], ap);

    const float d = smoothedAmount_.getCurrentValue();
    return blendAmount (z, ap, d);
}

void ShortDiffuser::process (float* channel,
                             int channelIndex,
                             int numSamples) noexcept
{
    if (channel == nullptr || numSamples <= 0)
        return;

    const int ch = (channelIndex == 0) ? 0 : 1;

    if (! smoothedAmount_.isSmoothing())
    {
        const float d = smoothedAmount_.getCurrentValue();
        if (d <= 0.0f)
        {
            // Transparent path: leave the block untouched.
            return;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const float z = channel[i];
            float ap = z;
            for (int s = 0; s < 3; ++s)
                ap = runStage (stages_[ch][s], ap);
            channel[i] = blendAmount (z, ap, d);
        }
        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const float z = channel[i];
        float ap = z;
        for (int s = 0; s < 3; ++s)
            ap = runStage (stages_[ch][s], ap);
        const float d = smoothedAmount_.getNextValue();
        channel[i] = blendAmount (z, ap, d);
    }
}

} // namespace shredspace
