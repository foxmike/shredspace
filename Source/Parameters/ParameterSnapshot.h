#pragma once

#include <array>
#include <atomic>
#include <cstddef>

#include <juce_audio_processors/juce_audio_processors.h>

namespace shredspace
{

class ParameterSnapshot
{
public:
    static constexpr double kGainSmoothingTimeSeconds = 0.020;
    static constexpr double kMixSmoothingTimeSeconds = 0.020;
    static constexpr double kFilterFrequencySmoothingTimeSeconds = 0.050;
    static constexpr double kT60SmoothingTimeSeconds = 0.050;
    static constexpr double kFeedbackTargetSmoothingTimeSeconds = 0.050;
    static constexpr double kWidthSmoothingTimeSeconds = 0.050;
    static constexpr double kPanSmoothingTimeSeconds = 0.030;
    static constexpr double kNormalizedExpertControlSmoothingTimeSeconds = 0.030;

    struct VoiceValues
    {
        bool enabled {};

        // Raw delay-time targets. The Smooth/Bend engine owns their transitions.
        float timeRatio {};
        float fineOffsetMs {};
        float stereoOffsetMs {};

        float levelDb {};
        float levelGain {};
        float tailScale {};
        float adaptiveRolePercent {};
        float pan {};
        float widthPercent {};
        float crossFeedbackPercent {};
        float modulationPhaseDegrees {};
        float hpfHz {};
        float lpfHz {};
        float presenceFrequencyHz {};
        float presenceGuardDb {};
        float presenceGuardGain {};
        float attackSoftenPercent {};
        float modulationRateHz {};
        float modulationDepthMs {};
        float driftPercent {};
        float diffusionPercent {};
        float saturationPercent {};
    };

    struct IntelligenceValues
    {
        float sensitivityPercent {};
        float inputGateDb {};
        float inputGateGain {};
        float densityWindowMs {};
        float densityStartEventsPerSecond {};
        float densityFullEventsPerSecond {};
        float sustainStartMs {};
        float sustainFullMs {};
        float phraseGapMs {};
        float contractionAttackMs {};
        float contractionReleaseMs {};
        float wetReductionDb {};
        float wetReductionGain {};
        float tailReductionOctaves {};
        float toneDarkeningOctaves {};
        float widthReductionPercent {};
        float attackSoftenIncreasePercent {};
        float bloomRiseMs {};
        float bloomHoldMs {};
        float bloomReleaseMs {};
        float wetLiftDb {};
        float wetLiftGain {};
        float tailExpansionOctaves {};
        float toneOpeningOctaves {};
        float widthExpansionPercent {};
        float diffusionIncreasePercent {};
        float transientCrossoverHz {};
    };

    struct Values
    {
        float inputGainDb {};
        float inputGain {};
        float outputGainDb {};
        float outputGain {};
        float mixPercent {};
        bool bypass {};
        int bypassMode {};
        int mixMode {};
        bool spillover {};
        bool safetyLimiter {};

        // Raw delay-time targets. The Smooth/Bend engine owns their transitions.
        float macroTimeMs {};
        int timeMode {};
        int syncDivision {};
        float timeGlideMs {};

        float macroSpacePercent {};
        float macroDefinitionPercent {};
        float macroBloomPercent {};
        float macroTailSeconds {};
        float macroFocusPercent {};
        float macroWidthPercent {};
        float macroCharacterPercent {};
        bool tap {};
        bool hold {};
        int holdMode {};
        int adaptiveMode {};
        int routing {};
        float routingCascadePercent {};

        bool lfMono {};
        float lfMonoFrequencyHz {};
        int sceneSelect {};
        float sceneMorphPercent {};

        VoiceValues voiceA {};
        VoiceValues voiceB {};
        IntelligenceValues intelligence {};
    };

    void prepare (double sampleRate,
                  int maximumBlockSize,
                  juce::AudioProcessorValueTreeState& apvts);

    void updateFromParameters();

    /**
     * Advances all smoothers by numSamples and resolves the effective values.
     * Call once for each processed block after updateFromParameters().
     */
    void advance (int numSamples);

    [[nodiscard]] const Values& values() const noexcept { return values_; }

private:
    enum class Parameter : std::size_t
    {
        globalInputGain,
        globalOutputGain,
        globalMix,
        globalBypass,
        globalBypassMode,
        globalMixMode,
        globalSpillover,
        globalSafetyLimiter,
        perfMacroTime,
        perfMacroSpace,
        perfMacroDefinition,
        perfMacroBloom,
        perfMacroTail,
        perfMacroFocus,
        perfMacroWidth,
        perfMacroCharacter,
        perfTap,
        perfHold,
        perfHoldMode,
        perfAdaptiveMode,
        perfRouting,
        perfRoutingCascade,
        timeMode,
        timeSyncDivision,
        timeGlide,
        stereoLfMono,
        stereoLfMonoFrequency,
        sceneSelect,
        sceneMorph,
        voiceAEnable,
        voiceATimeRatio,
        voiceAFineOffset,
        voiceALevel,
        voiceATailScale,
        voiceAAdaptiveRole,
        voiceAPan,
        voiceAWidth,
        voiceAStereoOffset,
        voiceACrossFeedback,
        voiceAModPhase,
        voiceAHpf,
        voiceALpf,
        voiceAPresenceFrequency,
        voiceAPresenceGuard,
        voiceAAttackSoften,
        voiceAModRate,
        voiceAModDepth,
        voiceADrift,
        voiceADiffusion,
        voiceASaturation,
        voiceBEnable,
        voiceBTimeRatio,
        voiceBFineOffset,
        voiceBLevel,
        voiceBTailScale,
        voiceBAdaptiveRole,
        voiceBPan,
        voiceBWidth,
        voiceBStereoOffset,
        voiceBCrossFeedback,
        voiceBModPhase,
        voiceBHpf,
        voiceBLpf,
        voiceBPresenceFrequency,
        voiceBPresenceGuard,
        voiceBAttackSoften,
        voiceBModRate,
        voiceBModDepth,
        voiceBDrift,
        voiceBDiffusion,
        voiceBSaturation,
        intelSensitivity,
        intelInputGate,
        intelDensityWindow,
        intelDensityStart,
        intelDensityFull,
        intelSustainStart,
        intelSustainFull,
        intelPhraseGap,
        intelContractionAttack,
        intelContractionRelease,
        intelWetReduction,
        intelTailReduction,
        intelToneDarkening,
        intelWidthReduction,
        intelAttackSoftenIncrease,
        intelBloomRise,
        intelBloomHold,
        intelBloomRelease,
        intelWetLift,
        intelTailExpansion,
        intelToneOpening,
        intelWidthExpansion,
        intelDiffusionIncrease,
        intelTransientCrossover,
        count
    };

    enum class LinearValue : std::size_t
    {
        inputGainDb,
        outputGainDb,
        mix,
        macroSpace,
        macroDefinition,
        macroBloom,
        macroFocus,
        macroWidth,
        macroCharacter,
        routingCascade,
        sceneMorph,
        voiceALevelDb,
        voiceAAdaptiveRole,
        voiceAPan,
        voiceAWidth,
        voiceACrossFeedback,
        voiceAModPhase,
        voiceAPresenceGuardDb,
        voiceAAttackSoften,
        voiceAModDepth,
        voiceADrift,
        voiceADiffusion,
        voiceASaturation,
        voiceBLevelDb,
        voiceBAdaptiveRole,
        voiceBPan,
        voiceBWidth,
        voiceBCrossFeedback,
        voiceBModPhase,
        voiceBPresenceGuardDb,
        voiceBAttackSoften,
        voiceBModDepth,
        voiceBDrift,
        voiceBDiffusion,
        voiceBSaturation,
        intelSensitivity,
        intelInputGateDb,
        intelDensityWindow,
        intelDensityStart,
        intelDensityFull,
        intelSustainStart,
        intelSustainFull,
        intelPhraseGap,
        intelContractionAttack,
        intelContractionRelease,
        intelWetReductionDb,
        intelTailReduction,
        intelToneDarkening,
        intelWidthReduction,
        intelAttackSoftenIncrease,
        intelBloomRise,
        intelBloomHold,
        intelBloomRelease,
        intelWetLiftDb,
        intelTailExpansion,
        intelToneOpening,
        intelWidthExpansion,
        intelDiffusionIncrease,
        count
    };

    enum class MultiplicativeValue : std::size_t
    {
        macroTail,
        lfMonoFrequency,
        voiceATailScale,
        voiceAHpf,
        voiceALpf,
        voiceAPresenceFrequency,
        voiceAModRate,
        voiceBTailScale,
        voiceBHpf,
        voiceBLpf,
        voiceBPresenceFrequency,
        voiceBModRate,
        intelTransientCrossover,
        count
    };

    using LinearSmoother = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;
    using MultiplicativeSmoother = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>;

    static constexpr std::size_t index (Parameter value) noexcept
    {
        return static_cast<std::size_t> (value);
    }

    static constexpr std::size_t index (LinearValue value) noexcept
    {
        return static_cast<std::size_t> (value);
    }

    static constexpr std::size_t index (MultiplicativeValue value) noexcept
    {
        return static_cast<std::size_t> (value);
    }

    [[nodiscard]] float read (Parameter parameter) const noexcept;
    [[nodiscard]] LinearSmoother& linear (LinearValue value) noexcept;
    [[nodiscard]] MultiplicativeSmoother& multiplicative (MultiplicativeValue value) noexcept;

    std::array<std::atomic<float>*, static_cast<std::size_t> (Parameter::count)> rawParameters_ {};
    std::array<LinearSmoother, static_cast<std::size_t> (LinearValue::count)> linearSmoothers_ {};
    std::array<MultiplicativeSmoother, static_cast<std::size_t> (MultiplicativeValue::count)> multiplicativeSmoothers_ {};
    Values values_ {};
};

} // namespace shredspace
