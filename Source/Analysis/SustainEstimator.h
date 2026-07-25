#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace shredspace
{

/**
 * Sustain confidence estimator and phrase-gap detector.
 *
 * This module computes the continuous sustain confidence and monitors the
 * input path for phrase gaps. All time-domain variables and attack/hold/release
 * counters are calibrated to the fixed 48 kHz analysis rate.
 */
class SustainEstimator
{
public:
    static constexpr double analysisSampleRate = 48000.0;

    SustainEstimator() noexcept;
    ~SustainEstimator() = default;

    SustainEstimator (const SustainEstimator&) = delete;
    SustainEstimator& operator= (const SustainEstimator&) = delete;
    SustainEstimator (SustainEstimator&&) = delete;
    SustainEstimator& operator= (SustainEstimator&&) = delete;

    /**
     * Precomputes AHR sample periods and resets the state variables.
     * Real-time safe.
     */
    void prepare() noexcept;

    /**
     * Resets the running states (activity, sustain, gap envelopes, latch).
     * Real-time safe.
     */
    void reset() noexcept;

    /**
     * Updates the sustain and phrase gap state over a block of analysis samples.
     *
     * @param inputLevelDb The current envelope level in dB.
     * @param timeSinceLastOnsetSeconds Elapsed time since the last accepted onset in seconds.
     * @param density Note density metric in [0, 1].
     * @param smoothedOnsetActivity Smoothed onset impulses in [0, 1].
     * @param numAnalysisSamples Number of samples in the current analysis block.
     */
    void process (float inputLevelDb,
                  float timeSinceLastOnsetSeconds,
                  float density,
                  float smoothedOnsetActivity,
                  int numAnalysisSamples) noexcept;

    // ---- Getters for continuous output metrics ----

    [[nodiscard]] float getActivity() const noexcept { return activity_; }
    [[nodiscard]] float getSustain() const noexcept { return sustain_; }
    [[nodiscard]] float getPhraseGap() const noexcept { return phraseGap_; }

    // ---- Parameter Accessors ----

    void setGateDb (float gateDb) noexcept { gateDb_ = gateDb; }
    [[nodiscard]] float getGateDb() const noexcept { return gateDb_; }

    void setGateWidthDb (float gateWidthDb) noexcept { gateWidthDb_ = gateWidthDb; }
    [[nodiscard]] float getGateWidthDb() const noexcept { return gateWidthDb_; }

    void setSustainStart (float sustainStart) noexcept { sustainStart_ = sustainStart; }
    [[nodiscard]] float getSustainStart() const noexcept { return sustainStart_; }

    void setSustainFull (float sustainFull) noexcept { sustainFull_ = sustainFull; }
    [[nodiscard]] float getSustainFull() const noexcept { return sustainFull_; }

    void setActivityLatchThreshold (float threshold) noexcept { activityLatchThreshold_ = threshold; }
    [[nodiscard]] float getActivityLatchThreshold() const noexcept { return activityLatchThreshold_; }

    void setLatchMemoryWindow (float seconds) noexcept;
    [[nodiscard]] float getLatchMemoryWindow() const noexcept { return latchMemoryWindow_; }

    void setGapDuration (float seconds) noexcept { gapDuration_ = seconds; }
    [[nodiscard]] float getGapDuration() const noexcept { return gapDuration_; }

    void setAttackTime (float seconds) noexcept;
    [[nodiscard]] float getAttackTime() const noexcept { return attackTime_; }

    void setHoldTime (float seconds) noexcept;
    [[nodiscard]] float getHoldTime() const noexcept { return holdTime_; }

    void setReleaseTime (float seconds) noexcept;
    [[nodiscard]] float getReleaseTime() const noexcept { return releaseTime_; }

private:
    /** Evaluates smoothstep for t in [0, 1]. */
    static float smoothstep (float t) noexcept
    {
        t = std::max (0.0f, std::min (t, 1.0f));
        return t * t * (3.0f - 2.0f * t);
    }

    /** Helper to recalculate internal sample counts from current seconds parameters. */
    void updateSampleCounts() noexcept;

    // ---- Settings (configurable parameters) ----
    float gateDb_ { -45.0f };
    float gateWidthDb_ { 12.0f };
    float sustainStart_ { 0.15f };
    float sustainFull_ { 0.60f };
    float activityLatchThreshold_ { 0.5f };
    float latchMemoryWindow_ { 4.0f };
    float gapDuration_ { 0.35f };
    float attackTime_ { 0.05f };
    float holdTime_ { 0.20f };
    float releaseTime_ { 0.30f };

    // ---- Precomputed sample counts ----
    int latchMemoryWindowSamples_ { 192000 }; // 4.0 * 48000
    int attackSamples_ { 2400 };              // 0.05 * 48000
    int holdSamples_ { 9600 };                // 0.20 * 48000
    int releaseSamples_ { 14400 };            // 0.30 * 48000

    // ---- Dynamic state variables ----
    float activity_ { 0.0f };
    float sustain_ { 0.0f };
    float phraseGap_ { 0.0f };

    bool latchActive_ { false };
    int latchTimerSamples_ { 0 };
    int holdTimer_ { 0 };
};

} // namespace shredspace
