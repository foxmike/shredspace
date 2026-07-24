#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numbers>

namespace shredspace
{

/**
 * RoutingEngine implements the three static voice topologies (Parallel, A-into-B, B-into-A),
 * wet voice normalization, and global mixing.
 */
class RoutingEngine
{
public:
    enum class Topology
    {
        Parallel,
        AtoB,
        BtoA
    };

    enum class MixMode
    {
        Insert,
        Send
    };

    RoutingEngine() = default;
    ~RoutingEngine() = default;

    RoutingEngine (const RoutingEngine&) = delete;
    RoutingEngine& operator= (const RoutingEngine&) = delete;
    RoutingEngine (RoutingEngine&&) = delete;
    RoutingEngine& operator= (RoutingEngine&&) = delete;

    /**
     * Configures the smoothers and sizes scratch/staging buffers.
     * All heap allocation is restricted to this call.
     */
    void prepare (double sampleRate, int maxBlockSize, int numChannels);

    /**
     * Resets the smoothers and transition state.
     */
    void reset() noexcept;

    /**
     * Sets the active topology. Smoothly crossfades over 30 ms.
     */
    void setTopology (Topology topology) noexcept;

    /**
     * Sets the cascade parameter in [0, 1]. Smoothed at 30 ms.
     */
    void setCascade (float cascade) noexcept;

    /**
     * Sets the linear voice levels in [0, 1]. Smoothed at 30 ms.
     */
    void setVoiceLevels (float levelA, float levelB) noexcept;

    /**
     * Sets the global mix mode.
     */
    void setMixMode (MixMode mode) noexcept;

    /**
     * Sets the global mix parameter in [0, 1]. Smoothed at 20 ms.
     */
    void setMix (float mix) noexcept;

    /**
     * Prepares the input to Voice A based on the current topology/transition state.
     * This is called before processing Voice A.
     */
    void prepareVoiceAInput (const juce::AudioBuffer<float>& dryBuffer,
                             const juce::AudioBuffer<float>& voiceBOutputBuffer,
                             juce::AudioBuffer<float>& voiceAInputBuffer) noexcept;

    /**
     * Prepares the input to Voice B based on the current topology/transition state.
     * This is called before processing Voice B.
     */
    void prepareVoiceBInput (const juce::AudioBuffer<float>& dryBuffer,
                             const juce::AudioBuffer<float>& voiceAOutputBuffer,
                             juce::AudioBuffer<float>& voiceBInputBuffer) noexcept;

    /**
     * Processes the final mix. Combines wet voice contributions, applies wet normalization,
     * performs global mixing, and writes the output.
     */
    void process (const juce::AudioBuffer<float>& dryBuffer,
                  juce::AudioBuffer<float>& voiceAWetBuffer,
                  juce::AudioBuffer<float>& voiceBWetBuffer,
                  juce::AudioBuffer<float>& outputBuffer) noexcept;

    /**
     * Returns whether Voice A should be processed before Voice B.
     */
    [[nodiscard]] bool shouldProcessVoiceAFirst() const noexcept;

    /**
     * Returns the gain scaler for the voice inputs (used for trails bypass in f.4).
     */
    [[nodiscard]] float delayInputGain() const noexcept { return 1.0f; }

    // Debugging / testing getters
    [[nodiscard]] const std::vector<float>& getFadeOldRamp() const noexcept { return fadeOldRamp_; }
    [[nodiscard]] const std::vector<float>& getFadeNewRamp() const noexcept { return fadeNewRamp_; }
    [[nodiscard]] const std::vector<float>& getSmoothedCascade() const noexcept { return smoothedCascade_; }

private:
    enum class TransitionState
    {
        idle,
        fading
    };

    void updateBlockState (int numSamples) noexcept;

    double sampleRate_ { 44100.0 };
    int maxBlockSize_ { 256 };
    int numChannels_ { 2 };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoother_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> cascadeSmoother_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> levelASmoother_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> levelBSmoother_;

    Topology activeTopology_ { Topology::Parallel };
    Topology sourceTopology_ { Topology::Parallel };
    Topology targetTopology_ { Topology::Parallel };
    MixMode mixMode_ { MixMode::Insert };

    TransitionState transitionState_ { TransitionState::idle };
    int rampCounter_ { 0 };
    int totalRampSamples_ { 0 };

    bool smoothersUpdatedForBlock_ { false };

    // Cached smoothed parameters & ramp values for block processing
    std::vector<float> smoothedMix_;
    std::vector<float> smoothedCascade_;
    std::vector<float> smoothedLevelA_;
    std::vector<float> smoothedLevelB_;
    std::vector<float> fadeOldRamp_;
    std::vector<float> fadeNewRamp_;

    // Allocated scratch/staging buffers as required by f.1 instructions
    juce::AudioBuffer<float> crossfadeScratch_;
    juce::AudioBuffer<float> voiceAStaging_;
    juce::AudioBuffer<float> voiceBStaging_;
};

} // namespace shredspace
