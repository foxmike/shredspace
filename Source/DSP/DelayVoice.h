#pragma once

#include "DSP/DelayTimeController.h"
#include "DSP/FeedbackController.h"
#include "DSP/FractionalDelay.h"

namespace shredspace
{

/**
 * One stereo delay voice.
 *
 * Delay-time resolution remains the caller's responsibility: this class
 * consumes a VoiceResult produced by DelayTimeController and never queries
 * host timing. The left and right channels share timing and feedback control,
 * but own independent delay-line storage and feedback samples.
 */
class DelayVoice
{
public:
    DelayVoice() = default;
    DelayVoice (const DelayVoice&) = delete;
    DelayVoice& operator= (const DelayVoice&) = delete;
    DelayVoice (DelayVoice&&) = delete;
    DelayVoice& operator= (DelayVoice&&) = delete;

    /**
     * Allocates both channels' maximum 8.25-second delay storage.
     * All allocation performed by this class is confined to this call.
     */
    void prepare (double sampleRate, int maxBlockSize);

    /** Clears both delay lines, the feedback controller, and loop state. */
    void reset() noexcept;

    /** Supplies the global T60 target to this voice's feedback controller. */
    void setT60Target (double T60GlobalSeconds) noexcept;

    /**
     * Processes an in-place stereo buffer.
     *
     * timeTarget is the result for this voice supplied by
     * DelayTimeController. glideMilliseconds controls a requested Smooth or
     * Bend transition, and tailScaleVoice scales the global T60 for this
     * voice. No allocation, resizing, locking, I/O, or string construction
     * occurs here.
     */
    void process (float* left,
                  float* right,
                  int numSamples,
                  const VoiceResult& timeTarget,
                  double glideMilliseconds,
                  double tailScaleVoice) noexcept;

private:
    void applyTimeTarget (const VoiceResult& timeTarget,
                          double glideMilliseconds) noexcept;

    // Ordered in-loop pass-through hooks. These deliberately do no DSP yet.
    [[nodiscard]] static float highPass (float sample) noexcept;
    [[nodiscard]] static float lowPass (float sample) noexcept;
    [[nodiscard]] static float presenceGuard (float sample) noexcept;
    [[nodiscard]] static float repeatTransientSoftener (float sample) noexcept;
    [[nodiscard]] static float mildSaturation (float sample) noexcept;
    [[nodiscard]] static float restrictedDiffusion (float sample) noexcept;
    [[nodiscard]] static float feedbackTap (float sample) noexcept;

    FractionalDelay leftDelay_;
    FractionalDelay rightDelay_;
    FeedbackController feedbackController_;

    double sampleRate_ { 1.0 };
    float previousLoopOutputLeft_ {};
    float previousLoopOutputRight_ {};
    bool hasTimeTarget_ {};
};

} // namespace shredspace
