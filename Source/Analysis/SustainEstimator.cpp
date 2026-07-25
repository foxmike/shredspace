#include "Analysis/SustainEstimator.h"

#include <algorithm>
#include <cmath>

namespace shredspace
{

SustainEstimator::SustainEstimator() noexcept
{
    prepare();
}

void SustainEstimator::prepare() noexcept
{
    updateSampleCounts();
    reset();
}

void SustainEstimator::reset() noexcept
{
    activity_ = 0.0f;
    sustain_ = 0.0f;
    phraseGap_ = 0.0f;
    latchActive_ = false;
    latchTimerSamples_ = 0;
    holdTimer_ = 0;
}

void SustainEstimator::process (float inputLevelDb,
                                float timeSinceLastOnsetSeconds,
                                float density,
                                float smoothedOnsetActivity,
                                int numAnalysisSamples) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    if (numAnalysisSamples <= 0)
        return;

    // 1. Activity = smoothstep((inputLevel_dB - gate_dB) / gateWidth_dB)
    float activity = 0.0f;
    if (gateWidthDb_ > 0.0f)
    {
        activity = smoothstep ((inputLevelDb - gateDb_) / gateWidthDb_);
    }
    else
    {
        activity = (inputLevelDb >= gateDb_) ? 1.0f : 0.0f;
    }
    activity_ = activity;

    // 2. sustainTime = smoothstep((timeSinceLastOnset - sustainStart) / (sustainFull - sustainStart))
    float sustainTime = 0.0f;
    const float sustainRange = sustainFull_ - sustainStart_;
    if (sustainRange > 0.0f)
    {
        sustainTime = smoothstep ((timeSinceLastOnsetSeconds - sustainStart_) / sustainRange);
    }
    else
    {
        sustainTime = (timeSinceLastOnsetSeconds >= sustainStart_) ? 1.0f : 0.0f;
    }

    // 3. sustain = activity * sustainTime * (1 - density) * (1 - smoothedOnsetActivity)
    const float d = std::max (0.0f, std::min (density, 1.0f));
    const float o = std::max (0.0f, std::min (smoothedOnsetActivity, 1.0f));
    sustain_ = activity_ * sustainTime * (1.0f - d) * (1.0f - o);

    // 4. Update Phrase Gap sample-by-sample
    for (int sample = 0; sample < numAnalysisSamples; ++sample)
    {
        // (a) Activity latch
        if (activity_ > activityLatchThreshold_)
        {
            latchActive_ = true;
            latchTimerSamples_ = latchMemoryWindowSamples_;
        }
        else
        {
            if (latchTimerSamples_ > 0)
            {
                latchTimerSamples_--;
                if (latchTimerSamples_ == 0)
                {
                    latchActive_ = false;
                }
            }
        }

        // Raw gap condition:
        // (a) recent prior activity (latchActive_)
        // (b) envelope level below gate (inputLevelDb < gateDb_)
        // (c) no accepted onset has occurred for at least gapDuration_
        const bool rawGap = latchActive_
                            && (inputLevelDb < gateDb_)
                            && (timeSinceLastOnsetSeconds >= gapDuration_);

        if (rawGap)
        {
            holdTimer_ = holdSamples_;
            if (phraseGap_ < 1.0f)
            {
                const float step = 1.0f / static_cast<float> (attackSamples_);
                phraseGap_ = std::min (1.0f, phraseGap_ + step);
            }
        }
        else
        {
            if (holdTimer_ > 0)
            {
                holdTimer_--;
            }
            else
            {
                if (phraseGap_ > 0.0f)
                {
                    const float step = 1.0f / static_cast<float> (releaseSamples_);
                    phraseGap_ = std::max (0.0f, phraseGap_ - step);
                }
            }
        }
    }
}

void SustainEstimator::setLatchMemoryWindow (float seconds) noexcept
{
    latchMemoryWindow_ = std::max (0.0f, seconds);
    updateSampleCounts();
}

void SustainEstimator::setAttackTime (float seconds) noexcept
{
    attackTime_ = std::max (0.0f, seconds);
    updateSampleCounts();
}

void SustainEstimator::setHoldTime (float seconds) noexcept
{
    holdTime_ = std::max (0.0f, seconds);
    updateSampleCounts();
}

void SustainEstimator::setReleaseTime (float seconds) noexcept
{
    releaseTime_ = std::max (0.0f, seconds);
    updateSampleCounts();
}

void SustainEstimator::updateSampleCounts() noexcept
{
    latchMemoryWindowSamples_ = std::max (0, static_cast<int> (std::round (latchMemoryWindow_ * analysisSampleRate)));
    attackSamples_ = std::max (1, static_cast<int> (std::round (attackTime_ * analysisSampleRate)));
    holdSamples_ = std::max (0, static_cast<int> (std::round (holdTime_ * analysisSampleRate)));
    releaseSamples_ = std::max (1, static_cast<int> (std::round (releaseTime_ * analysisSampleRate)));
}

} // namespace shredspace
