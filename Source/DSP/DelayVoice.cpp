#include "DSP/DelayVoice.h"

#include <algorithm>
#include <cmath>

namespace shredspace
{

void DelayVoice::prepare (double sampleRate, int maxBlockSize)
{
    sampleRate_ =
        std::isfinite (sampleRate) ? std::max (1.0, sampleRate) : 1.0;

    // FractionalDelay::prepare owns all delay-buffer allocation.
    leftDelay_.prepare (sampleRate_, maxBlockSize);
    rightDelay_.prepare (sampleRate_, maxBlockSize);
    feedbackController_.prepare (sampleRate_);
    reset();
}

void DelayVoice::reset() noexcept
{
    leftDelay_.reset();
    rightDelay_.reset();
    feedbackController_.reset();
    previousLoopOutputLeft_ = 0.0f;
    previousLoopOutputRight_ = 0.0f;
    hasTimeTarget_ = false;
}

void DelayVoice::setT60Target (double T60GlobalSeconds) noexcept
{
    feedbackController_.setT60Target (T60GlobalSeconds);
}

void DelayVoice::process (float* left,
                          float* right,
                          int numSamples,
                          const VoiceResult& timeTarget,
                          double glideMilliseconds,
                          double tailScaleVoice) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    applyTimeTarget (timeTarget, glideMilliseconds);

    for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
    {
        const double delayTimeSeconds =
            leftDelay_.getCurrentDelaySamples() / sampleRate_;
        const float feedbackGain = static_cast<float> (
            feedbackController_.processSample (delayTimeSeconds,
                                               tailScaleVoice));

        // The feedback value always comes from the preceding loop output.
        const float feedbackLeft = previousLoopOutputLeft_ * feedbackGain;
        const float feedbackRight = previousLoopOutputRight_ * feedbackGain;

        // Required write path, verbatim: write = input + feedback.
        const float writeLeft = left[sampleIndex] + feedbackLeft;
        const float writeRight = right[sampleIndex] + feedbackRight;

        // 1. Fractional delay read (including active Smooth/Bend transition).
        float loopLeft = leftDelay_.processSample (writeLeft);
        float loopRight = rightDelay_.processSample (writeRight);

        // 2. HPF -- ordered pass-through hook for a later phase.
        loopLeft = highPass (loopLeft);
        loopRight = highPass (loopRight);

        // 3. LPF -- ordered pass-through hook for a later phase.
        loopLeft = lowPass (loopLeft);
        loopRight = lowPass (loopRight);

        // 4. Presence Guard -- ordered pass-through hook for a later phase.
        loopLeft = presenceGuard (loopLeft);
        loopRight = presenceGuard (loopRight);

        // 5. Repeat transient softener -- ordered pass-through hook.
        loopLeft = repeatTransientSoftener (loopLeft);
        loopRight = repeatTransientSoftener (loopRight);

        // 6. Mild saturation -- ordered pass-through hook for a later phase.
        loopLeft = mildSaturation (loopLeft);
        loopRight = mildSaturation (loopRight);

        // 7. Restricted diffusion -- ordered pass-through hook.
        loopLeft = restrictedDiffusion (loopLeft);
        loopRight = restrictedDiffusion (loopRight);

        // 8. Feedback tap -- final ordered pass-through hook.
        previousLoopOutputLeft_ = feedbackTap (loopLeft);
        previousLoopOutputRight_ = feedbackTap (loopRight);

        left[sampleIndex] = loopLeft;
        right[sampleIndex] = loopRight;
    }
}

void DelayVoice::applyTimeTarget (const VoiceResult& timeTarget,
                                  double glideMilliseconds) noexcept
{
    if (! hasTimeTarget_ && ! timeTarget.transitionRequired)
    {
        leftDelay_.setDelaySamples (timeTarget.targetDelaySamples);
        rightDelay_.setDelaySamples (timeTarget.targetDelaySamples);
        hasTimeTarget_ = true;
        return;
    }

    if (! timeTarget.transitionRequired)
        return;

    const auto mode =
        timeTarget.transitionMode == TransitionMode::smooth
            ? FractionalDelay::TransitionMode::smooth
            : FractionalDelay::TransitionMode::bend;

    leftDelay_.setDelaySamples (timeTarget.targetDelaySamples,
                                glideMilliseconds,
                                mode);
    rightDelay_.setDelaySamples (timeTarget.targetDelaySamples,
                                 glideMilliseconds,
                                 mode);
    hasTimeTarget_ = true;
}

float DelayVoice::highPass (float sample) noexcept
{
    return sample;
}

float DelayVoice::lowPass (float sample) noexcept
{
    return sample;
}

float DelayVoice::presenceGuard (float sample) noexcept
{
    return sample;
}

float DelayVoice::repeatTransientSoftener (float sample) noexcept
{
    return sample;
}

float DelayVoice::mildSaturation (float sample) noexcept
{
    return sample;
}

float DelayVoice::restrictedDiffusion (float sample) noexcept
{
    return sample;
}

float DelayVoice::feedbackTap (float sample) noexcept
{
    return sample;
}

} // namespace shredspace
