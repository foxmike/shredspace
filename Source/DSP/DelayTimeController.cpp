#include "DelayTimeController.h"

#include <cmath>
#include <cstdint>
#include <array>
#include <algorithm>

namespace shredspace
{

namespace
{
    constexpr double getSyncDivisionRatio (int index) noexcept
    {
        switch (index)
        {
            case 0:  return 4.0;              // 1/1
            case 1:  return 4.0 * 1.5;        // 1/1 D
            case 2:  return 4.0 * 2.0 / 3.0;  // 1/1 T
            case 3:  return 2.0;              // 1/2
            case 4:  return 2.0 * 1.5;        // 1/2 D
            case 5:  return 2.0 * 2.0 / 3.0;  // 1/2 T
            case 6:  return 1.0;              // 1/4
            case 7:  return 1.0 * 1.5;        // 1/4 D
            case 8:  return 1.0 * 2.0 / 3.0;  // 1/4 T
            case 9:  return 0.5;              // 1/8
            case 10: return 0.5 * 1.5;        // 1/8 D
            case 11: return 0.5 * 2.0 / 3.0;  // 1/8 T
            case 12: return 0.25;             // 1/16
            case 13: return 0.25 * 1.5;       // 1/16 D
            case 14: return 0.25 * 2.0 / 3.0; // 1/16 T
            default: return 1.0;              // Fallback to Quarter
        }
    }
} // namespace

DelayTimeController::DelayTimeController() noexcept
{
    reset();
}

void DelayTimeController::prepare (double sampleRate) noexcept
{
    sampleRate_ = std::isfinite (sampleRate) ? std::max (1.0, sampleRate) : 44100.0;
}

void DelayTimeController::reset() noexcept
{
    lastValidBpm_ = 120.0;
    lastBpm_ = -1.0;
    lastSceneIndex_ = -1;
    lastProfileIndex_ = -1;
    lastTimeMode_ = -1;
    lastSyncDivision_ = -1;
    lastMacroTimeMs_ = -1.0;
    lastVoiceRatio_.fill (-1.0);
    lastVoiceFineOffsetMs_.fill (-1.0);

    intervals_.fill (0.0);
    numIntervals_ = 0;
    lastTapTimeSeconds_ = -1.0;

    lastTargetSamples_.fill (0.0);
    isFirstUpdate_ = true;
}

void DelayTimeController::tap (double timestampSeconds) noexcept
{
    if (! std::isfinite (timestampSeconds) || timestampSeconds < 0.0)
        return;

    if (lastTapTimeSeconds_ >= 0.0)
    {
        const double interval = timestampSeconds - lastTapTimeSeconds_;
        // Reject intervals outside [100 ms, 3000 ms]
        if (interval >= 0.1 && interval <= 3.0)
        {
            addInterval (interval);
        }
    }
    lastTapTimeSeconds_ = timestampSeconds;
}

void DelayTimeController::addInterval (double intervalSeconds) noexcept
{
    if (numIntervals_ < 5)
    {
        intervals_[numIntervals_] = intervalSeconds;
        numIntervals_++;
    }
    else
    {
        // Shift left to discard the oldest interval (FIFO)
        for (int i = 0; i < 4; ++i)
        {
            intervals_[i] = intervals_[i + 1];
        }
        intervals_[4] = intervalSeconds;
    }
}

double DelayTimeController::getTapInterval() const noexcept
{
    if (numIntervals_ == 0)
    {
        // Fallback to 120 BPM interval (500 ms)
        return 0.5;
    }

    std::array<double, 5> temp;
    for (int i = 0; i < numIntervals_; ++i)
    {
        temp[i] = intervals_[i];
    }

    std::sort (temp.begin(), temp.begin() + numIntervals_);

    if (numIntervals_ % 2 == 1)
    {
        return temp[numIntervals_ / 2];
    }
    else
    {
        return 0.5 * (temp[numIntervals_ / 2 - 1] + temp[numIntervals_ / 2]);
    }
}

std::array<VoiceResult, 2> DelayTimeController::update (const ControllerInput& input,
                                                        const HostTiming& timing) noexcept
{
    // 1. Resolve BPM
    double bpm = lastValidBpm_;
    if (timing.bpmValid && std::isfinite (timing.bpm) && timing.bpm > 0.0)
    {
        bpm = timing.bpm;
        lastValidBpm_ = timing.bpm;
    }

    // Check if BPM jumped
    bool bpmJumped = false;
    if (timing.bpmValid && lastBpm_ >= 0.0 && std::abs (timing.bpm - lastBpm_) > 0.001)
    {
        bpmJumped = true;
    }
    if (timing.bpmValid)
    {
        lastBpm_ = timing.bpm;
    }

    // 2. Resolve T_base
    double T_base = 0.5;
    if (input.timeMode == 0)      // Milliseconds
    {
        const double macroTimeMs = std::isfinite (input.macroTimeMs) ? std::max (0.0, input.macroTimeMs) : 0.0;
        T_base = macroTimeMs / 1000.0;
    }
    else if (input.timeMode == 1) // Host Sync
    {
        const double T_quarter = 60.0 / bpm;
        const double ratio_div = getSyncDivisionRatio (input.syncDivision);
        T_base = ratio_div * T_quarter;
    }
    else if (input.timeMode == 2) // Tap
    {
        T_base = getTapInterval();
    }

    // 3. Resolve targets for both voices (T_voice = T_base * ratio + fineOffset)
    std::array<double, 2> targetSeconds {};
    std::array<double, 2> targetSamples {};

    const double safeSampleRate = std::isfinite (sampleRate_) ? std::max (1.0, sampleRate_) : 44100.0;
    const double minDelaySeconds = 8.0 / safeSampleRate;
    const double maxDelaySeconds = 8.25;

    // Voice A target
    {
        const double ratio = std::isfinite (input.voiceA.timeRatio) ? std::max (0.0, input.voiceA.timeRatio) : 1.0;
        const double fineOffsetMs = std::isfinite (input.voiceA.fineOffsetMs) ? input.voiceA.fineOffsetMs : 0.0;
        double T_voice = T_base * ratio + (fineOffsetMs / 1000.0);
        T_voice = std::clamp (T_voice, minDelaySeconds, maxDelaySeconds);
        targetSeconds[0] = T_voice;
        targetSamples[0] = T_voice * safeSampleRate;
    }

    // Voice B target
    {
        const double ratio = std::isfinite (input.voiceB.timeRatio) ? std::max (0.0, input.voiceB.timeRatio) : 1.0;
        const double fineOffsetMs = std::isfinite (input.voiceB.fineOffsetMs) ? input.voiceB.fineOffsetMs : 0.0;
        double T_voice = T_base * ratio + (fineOffsetMs / 1000.0);
        T_voice = std::clamp (T_voice, minDelaySeconds, maxDelaySeconds);
        targetSeconds[1] = T_voice;
        targetSamples[1] = T_voice * safeSampleRate;
    }

    // Check if external trigger events occurred (scene change, profile change, mode change, etc.)
    const bool sceneChanged = (lastSceneIndex_ >= 0 && input.sceneIndex != lastSceneIndex_);
    const bool profileChanged = (lastProfileIndex_ >= 0 && input.profileIndex != lastProfileIndex_);
    const bool modeChanged = (lastTimeMode_ >= 0 && input.timeMode != lastTimeMode_);
    const bool divisionChanged = (lastSyncDivision_ >= 0 && input.syncDivision != lastSyncDivision_);

    const bool triggerLargeTransition = bpmJumped || sceneChanged || profileChanged || modeChanged || divisionChanged;

    std::array<VoiceResult, 2> results;

    if (isFirstUpdate_)
    {
        // First update after init/reset: output targets immediately, no transition
        for (int v = 0; v < 2; ++v)
        {
            results[v].targetDelaySamples = targetSamples[v];
            results[v].transitionRequired = false;
            results[v].transitionMode = preferredMode_;
            lastTargetSamples_[v] = targetSamples[v];
        }

        // Store state
        lastSceneIndex_ = input.sceneIndex;
        lastProfileIndex_ = input.profileIndex;
        lastTimeMode_ = input.timeMode;
        lastSyncDivision_ = input.syncDivision;
        lastMacroTimeMs_ = input.macroTimeMs;
        lastVoiceRatio_[0] = input.voiceA.timeRatio;
        lastVoiceFineOffsetMs_[0] = input.voiceA.fineOffsetMs;
        lastVoiceRatio_[1] = input.voiceB.timeRatio;
        lastVoiceFineOffsetMs_[1] = input.voiceB.fineOffsetMs;
        isFirstUpdate_ = false;

        return results;
    }

    // Evaluate results and transition requests per voice
    for (int v = 0; v < 2; ++v)
    {
        results[v].targetDelaySamples = targetSamples[v];

        const double diffSamples = std::abs (targetSamples[v] - lastTargetSamples_[v]);
        const double diffSeconds = diffSamples / safeSampleRate;

        // If target changed or an external event triggered a transition, request a transition
        if (diffSamples > 1e-5 || triggerLargeTransition)
        {
            results[v].transitionRequired = true;

            // Determine if transition should be rendered as Smooth or Bend
            // Large changes, including tempo jumps, scene changes, profile changes,
            // and parameter jumps exceeding 10 ms are classified as large.
            const bool isLargeChange = triggerLargeTransition || (diffSeconds > 0.010);

            if (isLargeChange)
            {
                // Large change defaults to smooth (crossfade) to prevent pitch sweep artifacts,
                // or respects the preferred setting.
                results[v].transitionMode = preferredMode_;
            }
            else
            {
                // Small changes can use pitch bend by default (or the preferredMode_)
                // Let's default to bend for small changes (tape style) to align with standard behavior,
                // unless preferredMode_ overrides it.
                results[v].transitionMode = (preferredMode_ == TransitionMode::smooth) ? TransitionMode::bend : TransitionMode::smooth;
            }
            lastTargetSamples_[v] = targetSamples[v];
        }
        else
        {
            results[v].transitionRequired = false;
            results[v].transitionMode = preferredMode_;
        }
    }

    // Update state tracking
    lastSceneIndex_ = input.sceneIndex;
    lastProfileIndex_ = input.profileIndex;
    lastTimeMode_ = input.timeMode;
    lastSyncDivision_ = input.syncDivision;
    lastMacroTimeMs_ = input.macroTimeMs;
    lastVoiceRatio_[0] = input.voiceA.timeRatio;
    lastVoiceFineOffsetMs_[0] = input.voiceA.fineOffsetMs;
    lastVoiceRatio_[1] = input.voiceB.timeRatio;
    lastVoiceFineOffsetMs_[1] = input.voiceB.fineOffsetMs;

    return results;
}

} // namespace shredspace
