#pragma once

#include <cmath>
#include <cstdint>
#include <array>
#include <algorithm>

namespace shredspace
{

/**
 * Plain POD struct verbatim in field order and types.
 * Filled once per block by the processor and passes in.
 */
struct HostTiming
{
    double bpm;
    bool bpmValid;
    bool isPlaying;
    std::int64_t samplePosition;
};

enum class TransitionMode
{
    smooth,
    bend
};

struct VoiceSettings
{
    double timeRatio { 1.0 };
    double fineOffsetMs { 0.0 };
};

struct ControllerInput
{
    int timeMode { 0 };          // 0: Milliseconds, 1: Host Sync, 2: Tap
    int syncDivision { 6 };      // 0 to 14
    double macroTimeMs { 500.0 };
    double timeGlideMs { 120.0 };
    VoiceSettings voiceA;
    VoiceSettings voiceB;
    int sceneIndex { 0 };
    int profileIndex { 0 };
};

struct VoiceResult
{
    double targetDelaySamples { 0.0 };
    bool transitionRequired { false };
    TransitionMode transitionMode { TransitionMode::smooth };
};

/**
 * Manages delay-time mode resolution for the two voices.
 * All real-time methods are noexcept and perform no allocation,
 * locking, I/O, string construction, or unbounded loops.
 */
class DelayTimeController
{
public:
    DelayTimeController() noexcept;

    /** Configures sample rate for delay-time sample conversions. */
    void prepare (double sampleRate) noexcept;

    /** Resets the controller state (e.g. on playback start, reset, or voice reactivation). */
    void reset() noexcept;

    /**
     * Registers a tap event at the given timestamp in seconds.
     */
    void tap (double timestampSeconds) noexcept;

    /**
     * Resolves the target delay times and transition modes for both voices.
     */
    std::array<VoiceResult, 2> update (const ControllerInput& input,
                                       const HostTiming& timing) noexcept;

    /** Sets the default/preferred transition mode. */
    void setPreferredTransitionMode (TransitionMode mode) noexcept { preferredMode_ = mode; }

    /** Returns the median interval of the tap history in seconds. */
    [[nodiscard]] double getTapInterval() const noexcept;

private:
    void addInterval (double intervalSeconds) noexcept;

    double sampleRate_ { 44100.0 };
    double lastValidBpm_ { 120.0 };
    double lastBpm_ { -1.0 };
    int lastSceneIndex_ { -1 };
    int lastProfileIndex_ { -1 };
    int lastTimeMode_ { -1 };
    int lastSyncDivision_ { -1 };
    double lastMacroTimeMs_ { -1.0 };
    std::array<double, 2> lastVoiceRatio_ { -1.0, -1.0 };
    std::array<double, 2> lastVoiceFineOffsetMs_ { -1.0, -1.0 };

    // Tap tempo history
    std::array<double, 5> intervals_ {};
    int numIntervals_ { 0 };
    double lastTapTimeSeconds_ { -1.0 };

    TransitionMode preferredMode_ { TransitionMode::smooth };

    // Track last resolved targets in samples to check for changes
    std::array<double, 2> lastTargetSamples_ { 0.0, 0.0 };
    bool isFirstUpdate_ { true };
};

} // namespace shredspace
