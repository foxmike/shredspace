#include "DSP/DelayVoice.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace shredspace
{

namespace
{
constexpr double kPiOverTwo = 1.57079632679489661923132169163975144;
constexpr double kDenormalThreshold = 1.0e-30;
}

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
    leftSaturator_.prepare (sampleRate_, maxBlockSize);
    rightSaturator_.prepare (sampleRate_, maxBlockSize);
    shortDiffuser_.prepare (sampleRate_, maxBlockSize);

    holdCrossfadeStep_ =
        1.0 / std::max (1.0, kHoldCrossfadeSeconds * sampleRate_);
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
    leftSaturator_.reset();
    rightSaturator_.reset();
    shortDiffuser_.reset();
    previousNormalLoopOutputLeft_ = 0.0f;
    previousNormalLoopOutputRight_ = 0.0f;
    previousHoldLoopOutputLeft_ = 0.0f;
    previousHoldLoopOutputRight_ = 0.0f;
    holdPreviousInput_[0] = 0.0;
    holdPreviousInput_[1] = 0.0;
    holdPreviousOutput_[0] = 0.0;
    holdPreviousOutput_[1] = 0.0;
    holdCrossfade_ = 0.0;
    holdRequested_ = false;
    hasTimeTarget_ = false;
}

void DelayVoice::setT60Target (double T60GlobalSeconds) noexcept
{
    feedbackController_.setT60Target (T60GlobalSeconds);
}

void DelayVoice::setSaturationAmount (float s) noexcept
{
    leftSaturator_.setAmount (s);
    rightSaturator_.setAmount (s);
}

void DelayVoice::setDiffusionAmount (float d) noexcept
{
    shortDiffuser_.setAmount (d);
}

void DelayVoice::setHold (bool shouldHold) noexcept
{
    holdRequested_ = shouldHold;
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
        const float normalFeedbackGain = static_cast<float> (
            feedbackController_.processSample (delayTimeSeconds,
                                               tailScaleVoice));

        const double normalWeight =
            std::cos (kPiOverTwo * holdCrossfade_);
        const double holdWeight =
            std::sin (kPiOverTwo * holdCrossfade_);

        // The fixed Hold gain is itself the unconditional hard ceiling.
        const float normalFeedbackLeft =
            previousNormalLoopOutputLeft_ * normalFeedbackGain;
        const float normalFeedbackRight =
            previousNormalLoopOutputRight_ * normalFeedbackGain;
        const float holdFeedbackLeft =
            previousHoldLoopOutputLeft_ * kHoldFeedbackGain;
        const float holdFeedbackRight =
            previousHoldLoopOutputRight_ * kHoldFeedbackGain;

        const float feedbackLeft = static_cast<float> (
            normalWeight * normalFeedbackLeft
            + holdWeight * holdFeedbackLeft);
        const float feedbackRight = static_cast<float> (
            normalWeight * normalFeedbackRight
            + holdWeight * holdFeedbackRight);

        // New material is excluded as soon as Hold is engaged.
        const float inputLeft = holdRequested_ ? 0.0f : left[sampleIndex];
        const float inputRight = holdRequested_ ? 0.0f : right[sampleIndex];
        const float writeLeft = inputLeft + feedbackLeft;
        const float writeRight = inputRight + feedbackRight;

        // 1. Fractional delay read (including active Smooth/Bend transition).
        const float delayReadLeft = leftDelay_.processSample (writeLeft);
        const float delayReadRight = rightDelay_.processSample (writeRight);

        float normalLoopLeft = 0.0f;
        float normalLoopRight = 0.0f;
        if (holdCrossfade_ < 1.0)
        {
            normalLoopLeft = delayReadLeft;
            normalLoopRight = delayReadRight;

            // 2. HPF (RepeatToneProcessor).
            normalLoopLeft =
                repeatTone_.processHighPassSample (0, normalLoopLeft);
            normalLoopRight =
                repeatTone_.processHighPassSample (1, normalLoopRight);

            // 3. LPF (RepeatToneProcessor).
            normalLoopLeft =
                repeatTone_.processLowPassSample (0, normalLoopLeft);
            normalLoopRight =
                repeatTone_.processLowPassSample (1, normalLoopRight);

            // 4. Presence Guard.
            normalLoopLeft =
                presenceGuard_.processSample (0, normalLoopLeft);
            normalLoopRight =
                presenceGuard_.processSample (1, normalLoopRight);

            // 5. Repeat transient softener.
            normalLoopLeft =
                repeatSoftener_.processSample (0, normalLoopLeft);
            normalLoopRight =
                repeatSoftener_.processSample (1, normalLoopRight);

            // 6. Mild saturation (unity small-signal slope; no makeup gain).
            normalLoopLeft = leftSaturator_.processSample (normalLoopLeft);
            normalLoopRight = rightSaturator_.processSample (normalLoopRight);

            // 7. Restricted diffusion.
            normalLoopLeft =
                shortDiffuser_.processSample (normalLoopLeft, 0);
            normalLoopRight =
                shortDiffuser_.processSample (normalLoopRight, 1);

            // 8. Feedback tap -- final normal-loop output for next sample.
            previousNormalLoopOutputLeft_ = normalLoopLeft;
            previousNormalLoopOutputRight_ = normalLoopRight;
        }

        float holdLoopLeft = 0.0f;
        float holdLoopRight = 0.0f;
        if (holdCrossfade_ > 0.0)
        {
            // Dedicated Hold path: no tone, guard, transient, saturation, or
            // diffusion processing is invoked here.
            holdLoopLeft = processHoldSample (delayReadLeft, 0);
            holdLoopRight = processHoldSample (delayReadRight, 1);
            previousHoldLoopOutputLeft_ = holdLoopLeft;
            previousHoldLoopOutputRight_ = holdLoopRight;
        }

        left[sampleIndex] = static_cast<float> (
            normalWeight * normalLoopLeft + holdWeight * holdLoopLeft);
        right[sampleIndex] = static_cast<float> (
            normalWeight * normalLoopRight + holdWeight * holdLoopRight);

        advanceHoldCrossfade();
    }
}

float DelayVoice::processHoldSample (float input, int channel) noexcept
{
    const auto index = static_cast<std::size_t> (channel);
    const double finiteInput = static_cast<double> (input);
    if (! std::isfinite (finiteInput))
    {
        holdPreviousInput_[index] = 0.0;
        holdPreviousOutput_[index] = 0.0;
        return 0.0f;
    }

    const double dcOutput =
        finiteInput - holdPreviousInput_[index]
        + kHoldDcBlockerR * holdPreviousOutput_[index];
    if (! std::isfinite (dcOutput)
        || std::abs (dcOutput)
               > static_cast<double> (std::numeric_limits<float>::max()))
    {
        holdPreviousInput_[index] = 0.0;
        holdPreviousOutput_[index] = 0.0;
        return 0.0f;
    }

    holdPreviousInput_[index] =
        std::abs (finiteInput) < kDenormalThreshold ? 0.0 : finiteInput;
    holdPreviousOutput_[index] =
        std::abs (dcOutput) < kDenormalThreshold ? 0.0 : dcOutput;
    return static_cast<float> (holdPreviousOutput_[index]);
}

void DelayVoice::advanceHoldCrossfade() noexcept
{
    if (holdRequested_)
        holdCrossfade_ = std::min (1.0, holdCrossfade_ + holdCrossfadeStep_);
    else
        holdCrossfade_ = std::max (0.0, holdCrossfade_ - holdCrossfadeStep_);
}

void DelayVoice::applyTimeTarget (const VoiceResult& timeTarget,
                                  double glideMilliseconds) noexcept
{
    // FractionalDelay clamps every actual read head as well; clamping the
    // requested target here makes the invariant explicit at the voice edge.
    const double maximumDelaySamples =
        FractionalDelay::kMaximumDelaySeconds * sampleRate_;
    const double clampedTarget =
        std::isfinite (timeTarget.targetDelaySamples)
            ? std::clamp (timeTarget.targetDelaySamples,
                          FractionalDelay::kMinimumDelaySamples,
                          maximumDelaySamples)
            : FractionalDelay::kMinimumDelaySamples;

    if (! hasTimeTarget_ && ! timeTarget.transitionRequired)
    {
        leftDelay_.setDelaySamples (clampedTarget);
        rightDelay_.setDelaySamples (clampedTarget);
        hasTimeTarget_ = true;
        return;
    }

    if (! timeTarget.transitionRequired)
        return;

    const auto mode =
        timeTarget.transitionMode == TransitionMode::smooth
            ? FractionalDelay::TransitionMode::smooth
            : FractionalDelay::TransitionMode::bend;

    leftDelay_.setDelaySamples (clampedTarget,
                                glideMilliseconds,
                                mode);
    rightDelay_.setDelaySamples (clampedTarget,
                                 glideMilliseconds,
                                 mode);
    hasTimeTarget_ = true;
}

} // namespace shredspace
