#pragma once

#include "DSP/DelayTimeController.h"
#include "DSP/FeedbackController.h"
#include "DSP/FractionalDelay.h"
#include "DSP/ModulationEngine.h"
#include "DSP/RepeatToneProcessor.h"
#include "DSP/PresenceGuard.h"
#include "DSP/RepeatTransientSoftener.h"
#include "DSP/SoftSaturator.h"
#include "DSP/ShortDiffuser.h"

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

    /** Sets normalized stereo cross-feedback in [0, 1]. */
    void setCrossFeedback (float q) noexcept;

    /** Applies the current safety processor feedback ceiling. */
    void setFeedbackGainCeiling (float ceiling) noexcept;

    /** Sets the in-loop saturation amount in [0, 1]. */
    void setSaturationAmount (float s) noexcept;

    /** Sets the in-loop diffusion amount in [0, 0.40]. */
    void setDiffusionAmount (float d) noexcept;

    /** Configures the repeat-loop tone and dynamic shaping stages. */
    void setHighPassFrequency (float hz);
    void setLowPassFrequency (float hz);
    void setPresenceGuard (float frequencyHz, float maximumCutDb);
    void setAttackSoften (float amountPercent, float crossoverHz);

    /** Configures stereo delay-time motion and the right-channel offset. */
    void setModulation (float rateHz,
                        float depthMs,
                        float phaseDegrees,
                        float driftAmount,
                        float stereoOffsetMs) noexcept;

    /**
     * Engages or releases the dedicated Hold preservation loop.
     * Both transitions use a 30 ms equal-power crossfade.
     */
    void setHold (bool shouldHold) noexcept;

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
    [[nodiscard]] float processHoldSample (float input,
                                           int channel) noexcept;
    void advanceHoldCrossfade() noexcept;

    FractionalDelay leftDelay_;
    FractionalDelay rightDelay_;
    FeedbackController feedbackController_;
    ModulationEngine modulationEngine_;

    RepeatToneProcessor repeatTone_;
    PresenceGuard presenceGuard_;
    RepeatTransientSoftener repeatSoftener_;
    SoftSaturator leftSaturator_;
    SoftSaturator rightSaturator_;
    ShortDiffuser shortDiffuser_;

    static constexpr double kHoldCrossfadeSeconds = 0.030;
    static constexpr float kHoldFeedbackGain = 0.985f;
    static constexpr double kHoldDcBlockerR = 0.995;

    double sampleRate_ { 1.0 };
    double stereoOffsetSamples_ {};
    float previousNormalLoopOutputLeft_ {};
    float previousNormalLoopOutputRight_ {};
    float previousHoldLoopOutputLeft_ {};
    float previousHoldLoopOutputRight_ {};
    double holdPreviousInput_[2] {};
    double holdPreviousOutput_[2] {};
    double holdCrossfade_ {};
    double holdCrossfadeStep_ { 1.0 };
    bool holdRequested_ {};
    bool hasTimeTarget_ {};
};

} // namespace shredspace
