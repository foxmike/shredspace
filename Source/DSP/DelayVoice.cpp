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

    // The repeat-loop processors are stereo (left = channel 0, right = 1).
    repeatTone_.prepare (sampleRate_, maxBlockSize, 2);
    presenceGuard_.prepare (sampleRate_, maxBlockSize, 2);
    repeatSoftener_.prepare (sampleRate_, maxBlockSize, 2);

    reset();
}

void DelayVoice::reset() noexcept
{
    leftDelay_.reset();
    rightDelay_.reset();
    feedbackController_.reset();
    repeatTone_.reset();
    presenceGuard_.reset();
    repeatSoftener_.reset();
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

    // Control-rate / block boundary: advance smoothers and recompute filter
    // coefficients for all three repeat-loop processors exactly once per
    // processed block, never per sample.
    repeatTone_.updateCoefficients();
    presenceGuard_.updateCoefficients();
    repeatSoftener_.updateCoefficients();

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

        // 2. HPF (RepeatToneProcessor).
        loopLeft = repeatTone_.processHighPassSample (0, loopLeft);
        loopRight = repeatTone_.processHighPassSample (1, loopRight);

        // 3. LPF (RepeatToneProcessor).
        loopLeft = repeatTone_.processLowPassSample (0, loopLeft);
        loopRight = repeatTone_.processLowPassSample (1, loopRight);

        // 4. Presence Guard.
        loopLeft = presenceGuard_.processSample (0, loopLeft);
        loopRight = presenceGuard_.processSample (1, loopRight);

        // 5. Repeat transient softener.
        loopLeft = repeatSoftener_.processSample (0, loopLeft);
        loopRight = repeatSoftener_.processSample (1, loopRight);

        // 6. Mild saturation -- RESERVED SLOT for phase e.
        //    Pass-through placeholder: no implementation yet.
        // loopLeft = mildSaturation (loopLeft);
        // loopRight = mildSaturation (loopRight);

        // 7. Restricted diffusion -- RESERVED SLOT for phase e.
        //    Pass-through placeholder: no implementation yet.
        // loopLeft = restrictedDiffusion (loopLeft);
        // loopRight = restrictedDiffusion (loopRight);

        // 8. Feedback tap -- final loop output stored for the next sample.
        previousLoopOutputLeft_ = loopLeft;
        previousLoopOutputRight_ = loopRight;

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

} // namespace shredspace
