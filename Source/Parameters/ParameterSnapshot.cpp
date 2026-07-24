#include "ParameterSnapshot.h"

#include "ParameterIDs.h"

#include <algorithm>
#include <cmath>

namespace shredspace
{
namespace
{
constexpr float kMacroTailMinimum = 0.15f;
constexpr float kLfMonoFrequencyMinimum = 60.0f;
constexpr float kTailScaleMinimum = 0.25f;
constexpr float kHpfMinimum = 40.0f;
constexpr float kLpfMinimum = 2000.0f;
constexpr float kPresenceFrequencyMinimum = 1500.0f;
constexpr float kModulationRateMinimum = 0.05f;
constexpr float kTransientCrossoverMinimum = 1000.0f;
} // namespace

#define SHREDSPACE_PARAMETER_IDS(X) \
    X (globalInputGain, GLOBAL_INPUT_GAIN) \
    X (globalOutputGain, GLOBAL_OUTPUT_GAIN) \
    X (globalMix, GLOBAL_MIX) \
    X (globalBypass, GLOBAL_BYPASS) \
    X (globalBypassMode, GLOBAL_BYPASS_MODE) \
    X (globalMixMode, GLOBAL_MIX_MODE) \
    X (globalSpillover, GLOBAL_SPILLOVER) \
    X (globalSafetyLimiter, GLOBAL_SAFETY_LIMITER) \
    X (perfMacroTime, PERF_MACRO_TIME) \
    X (perfMacroSpace, PERF_MACRO_SPACE) \
    X (perfMacroDefinition, PERF_MACRO_DEFINITION) \
    X (perfMacroBloom, PERF_MACRO_BLOOM) \
    X (perfMacroTail, PERF_MACRO_TAIL) \
    X (perfMacroFocus, PERF_MACRO_FOCUS) \
    X (perfMacroWidth, PERF_MACRO_WIDTH) \
    X (perfMacroCharacter, PERF_MACRO_CHARACTER) \
    X (perfTap, PERF_TAP) \
    X (perfHold, PERF_HOLD) \
    X (perfHoldMode, PERF_HOLD_MODE) \
    X (perfAdaptiveMode, PERF_ADAPTIVE_MODE) \
    X (perfRouting, PERF_ROUTING) \
    X (perfRoutingCascade, PERF_ROUTING_CASCADE) \
    X (timeMode, TIME_MODE) \
    X (timeSyncDivision, TIME_SYNC_DIVISION) \
    X (timeGlide, TIME_GLIDE) \
    X (stereoLfMono, STEREO_LF_MONO) \
    X (stereoLfMonoFrequency, STEREO_LF_MONO_FREQ) \
    X (sceneSelect, SCENE_SELECT) \
    X (sceneMorph, SCENE_MORPH) \
    X (voiceAEnable, VOICE_A_ENABLE) \
    X (voiceATimeRatio, VOICE_A_TIME_RATIO) \
    X (voiceAFineOffset, VOICE_A_FINE_OFFSET) \
    X (voiceALevel, VOICE_A_LEVEL) \
    X (voiceATailScale, VOICE_A_TAIL_SCALE) \
    X (voiceAAdaptiveRole, VOICE_A_ADAPTIVE_ROLE) \
    X (voiceAPan, VOICE_A_PAN) \
    X (voiceAWidth, VOICE_A_WIDTH) \
    X (voiceAStereoOffset, VOICE_A_STEREO_OFFSET) \
    X (voiceACrossFeedback, VOICE_A_CROSS_FEEDBACK) \
    X (voiceAModPhase, VOICE_A_MOD_PHASE) \
    X (voiceAHpf, VOICE_A_HPF) \
    X (voiceALpf, VOICE_A_LPF) \
    X (voiceAPresenceFrequency, VOICE_A_PRESENCE_FREQ) \
    X (voiceAPresenceGuard, VOICE_A_PRESENCE_GUARD) \
    X (voiceAAttackSoften, VOICE_A_ATTACK_SOFTEN) \
    X (voiceAModRate, VOICE_A_MOD_RATE) \
    X (voiceAModDepth, VOICE_A_MOD_DEPTH) \
    X (voiceADrift, VOICE_A_DRIFT) \
    X (voiceADiffusion, VOICE_A_DIFFUSION) \
    X (voiceASaturation, VOICE_A_SATURATION) \
    X (voiceBEnable, VOICE_B_ENABLE) \
    X (voiceBTimeRatio, VOICE_B_TIME_RATIO) \
    X (voiceBFineOffset, VOICE_B_FINE_OFFSET) \
    X (voiceBLevel, VOICE_B_LEVEL) \
    X (voiceBTailScale, VOICE_B_TAIL_SCALE) \
    X (voiceBAdaptiveRole, VOICE_B_ADAPTIVE_ROLE) \
    X (voiceBPan, VOICE_B_PAN) \
    X (voiceBWidth, VOICE_B_WIDTH) \
    X (voiceBStereoOffset, VOICE_B_STEREO_OFFSET) \
    X (voiceBCrossFeedback, VOICE_B_CROSS_FEEDBACK) \
    X (voiceBModPhase, VOICE_B_MOD_PHASE) \
    X (voiceBHpf, VOICE_B_HPF) \
    X (voiceBLpf, VOICE_B_LPF) \
    X (voiceBPresenceFrequency, VOICE_B_PRESENCE_FREQ) \
    X (voiceBPresenceGuard, VOICE_B_PRESENCE_GUARD) \
    X (voiceBAttackSoften, VOICE_B_ATTACK_SOFTEN) \
    X (voiceBModRate, VOICE_B_MOD_RATE) \
    X (voiceBModDepth, VOICE_B_MOD_DEPTH) \
    X (voiceBDrift, VOICE_B_DRIFT) \
    X (voiceBDiffusion, VOICE_B_DIFFUSION) \
    X (voiceBSaturation, VOICE_B_SATURATION) \
    X (intelSensitivity, INTEL_SENSITIVITY) \
    X (intelInputGate, INTEL_INPUT_GATE) \
    X (intelDensityWindow, INTEL_DENSITY_WINDOW) \
    X (intelDensityStart, INTEL_DENSITY_START) \
    X (intelDensityFull, INTEL_DENSITY_FULL) \
    X (intelSustainStart, INTEL_SUSTAIN_START) \
    X (intelSustainFull, INTEL_SUSTAIN_FULL) \
    X (intelPhraseGap, INTEL_PHRASE_GAP) \
    X (intelContractionAttack, INTEL_CONTRACTION_ATTACK) \
    X (intelContractionRelease, INTEL_CONTRACTION_RELEASE) \
    X (intelWetReduction, INTEL_WET_REDUCTION) \
    X (intelTailReduction, INTEL_TAIL_REDUCTION) \
    X (intelToneDarkening, INTEL_TONE_DARKENING) \
    X (intelWidthReduction, INTEL_WIDTH_REDUCTION) \
    X (intelAttackSoftenIncrease, INTEL_ATTACK_SOFTEN_INCREASE) \
    X (intelBloomRise, INTEL_BLOOM_RISE) \
    X (intelBloomHold, INTEL_BLOOM_HOLD) \
    X (intelBloomRelease, INTEL_BLOOM_RELEASE) \
    X (intelWetLift, INTEL_WET_LIFT) \
    X (intelTailExpansion, INTEL_TAIL_EXPANSION) \
    X (intelToneOpening, INTEL_TONE_OPENING) \
    X (intelWidthExpansion, INTEL_WIDTH_EXPANSION) \
    X (intelDiffusionIncrease, INTEL_DIFFUSION_INCREASE) \
    X (intelTransientCrossover, INTEL_TRANSIENT_CROSSOVER)

#define SHREDSPACE_LINEAR_VALUES(X) \
    X (inputGainDb, inputGainDb, globalInputGain, kGainSmoothingTimeSeconds) \
    X (outputGainDb, outputGainDb, globalOutputGain, kGainSmoothingTimeSeconds) \
    X (mix, mixPercent, globalMix, kMixSmoothingTimeSeconds) \
    X (macroSpace, macroSpacePercent, perfMacroSpace, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (macroDefinition, macroDefinitionPercent, perfMacroDefinition, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (macroBloom, macroBloomPercent, perfMacroBloom, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (macroFocus, macroFocusPercent, perfMacroFocus, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (macroWidth, macroWidthPercent, perfMacroWidth, kWidthSmoothingTimeSeconds) \
    X (macroCharacter, macroCharacterPercent, perfMacroCharacter, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (routingCascade, routingCascadePercent, perfRoutingCascade, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (sceneMorph, sceneMorphPercent, sceneMorph, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (voiceALevelDb, voiceA.levelDb, voiceALevel, kGainSmoothingTimeSeconds) \
    X (voiceAAdaptiveRole, voiceA.adaptiveRolePercent, voiceAAdaptiveRole, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (voiceAPan, voiceA.pan, voiceAPan, kPanSmoothingTimeSeconds) \
    X (voiceAWidth, voiceA.widthPercent, voiceAWidth, kWidthSmoothingTimeSeconds) \
    X (voiceACrossFeedback, voiceA.crossFeedbackPercent, voiceACrossFeedback, kFeedbackTargetSmoothingTimeSeconds) \
    X (voiceAModPhase, voiceA.modulationPhaseDegrees, voiceAModPhase, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (voiceAPresenceGuardDb, voiceA.presenceGuardDb, voiceAPresenceGuard, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (voiceAAttackSoften, voiceA.attackSoftenPercent, voiceAAttackSoften, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (voiceAModDepth, voiceA.modulationDepthMs, voiceAModDepth, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (voiceADrift, voiceA.driftPercent, voiceADrift, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (voiceADiffusion, voiceA.diffusionPercent, voiceADiffusion, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (voiceASaturation, voiceA.saturationPercent, voiceASaturation, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (voiceBLevelDb, voiceB.levelDb, voiceBLevel, kGainSmoothingTimeSeconds) \
    X (voiceBAdaptiveRole, voiceB.adaptiveRolePercent, voiceBAdaptiveRole, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (voiceBPan, voiceB.pan, voiceBPan, kPanSmoothingTimeSeconds) \
    X (voiceBWidth, voiceB.widthPercent, voiceBWidth, kWidthSmoothingTimeSeconds) \
    X (voiceBCrossFeedback, voiceB.crossFeedbackPercent, voiceBCrossFeedback, kFeedbackTargetSmoothingTimeSeconds) \
    X (voiceBModPhase, voiceB.modulationPhaseDegrees, voiceBModPhase, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (voiceBPresenceGuardDb, voiceB.presenceGuardDb, voiceBPresenceGuard, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (voiceBAttackSoften, voiceB.attackSoftenPercent, voiceBAttackSoften, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (voiceBModDepth, voiceB.modulationDepthMs, voiceBModDepth, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (voiceBDrift, voiceB.driftPercent, voiceBDrift, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (voiceBDiffusion, voiceB.diffusionPercent, voiceBDiffusion, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (voiceBSaturation, voiceB.saturationPercent, voiceBSaturation, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelSensitivity, intelligence.sensitivityPercent, intelSensitivity, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelInputGateDb, intelligence.inputGateDb, intelInputGate, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelDensityWindow, intelligence.densityWindowMs, intelDensityWindow, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelDensityStart, intelligence.densityStartEventsPerSecond, intelDensityStart, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelDensityFull, intelligence.densityFullEventsPerSecond, intelDensityFull, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelSustainStart, intelligence.sustainStartMs, intelSustainStart, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelSustainFull, intelligence.sustainFullMs, intelSustainFull, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelPhraseGap, intelligence.phraseGapMs, intelPhraseGap, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelContractionAttack, intelligence.contractionAttackMs, intelContractionAttack, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelContractionRelease, intelligence.contractionReleaseMs, intelContractionRelease, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelWetReductionDb, intelligence.wetReductionDb, intelWetReduction, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelTailReduction, intelligence.tailReductionOctaves, intelTailReduction, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelToneDarkening, intelligence.toneDarkeningOctaves, intelToneDarkening, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelWidthReduction, intelligence.widthReductionPercent, intelWidthReduction, kWidthSmoothingTimeSeconds) \
    X (intelAttackSoftenIncrease, intelligence.attackSoftenIncreasePercent, intelAttackSoftenIncrease, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelBloomRise, intelligence.bloomRiseMs, intelBloomRise, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelBloomHold, intelligence.bloomHoldMs, intelBloomHold, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelBloomRelease, intelligence.bloomReleaseMs, intelBloomRelease, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelWetLiftDb, intelligence.wetLiftDb, intelWetLift, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelTailExpansion, intelligence.tailExpansionOctaves, intelTailExpansion, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelToneOpening, intelligence.toneOpeningOctaves, intelToneOpening, kNormalizedExpertControlSmoothingTimeSeconds) \
    X (intelWidthExpansion, intelligence.widthExpansionPercent, intelWidthExpansion, kWidthSmoothingTimeSeconds) \
    X (intelDiffusionIncrease, intelligence.diffusionIncreasePercent, intelDiffusionIncrease, kNormalizedExpertControlSmoothingTimeSeconds)

#define SHREDSPACE_MULTIPLICATIVE_VALUES(X) \
    X (macroTail, macroTailSeconds, perfMacroTail, kT60SmoothingTimeSeconds, kMacroTailMinimum) \
    X (lfMonoFrequency, lfMonoFrequencyHz, stereoLfMonoFrequency, kFilterFrequencySmoothingTimeSeconds, kLfMonoFrequencyMinimum) \
    X (voiceATailScale, voiceA.tailScale, voiceATailScale, kT60SmoothingTimeSeconds, kTailScaleMinimum) \
    X (voiceAHpf, voiceA.hpfHz, voiceAHpf, kFilterFrequencySmoothingTimeSeconds, kHpfMinimum) \
    X (voiceALpf, voiceA.lpfHz, voiceALpf, kFilterFrequencySmoothingTimeSeconds, kLpfMinimum) \
    X (voiceAPresenceFrequency, voiceA.presenceFrequencyHz, voiceAPresenceFrequency, kFilterFrequencySmoothingTimeSeconds, kPresenceFrequencyMinimum) \
    X (voiceAModRate, voiceA.modulationRateHz, voiceAModRate, kNormalizedExpertControlSmoothingTimeSeconds, kModulationRateMinimum) \
    X (voiceBTailScale, voiceB.tailScale, voiceBTailScale, kT60SmoothingTimeSeconds, kTailScaleMinimum) \
    X (voiceBHpf, voiceB.hpfHz, voiceBHpf, kFilterFrequencySmoothingTimeSeconds, kHpfMinimum) \
    X (voiceBLpf, voiceB.lpfHz, voiceBLpf, kFilterFrequencySmoothingTimeSeconds, kLpfMinimum) \
    X (voiceBPresenceFrequency, voiceB.presenceFrequencyHz, voiceBPresenceFrequency, kFilterFrequencySmoothingTimeSeconds, kPresenceFrequencyMinimum) \
    X (voiceBModRate, voiceB.modulationRateHz, voiceBModRate, kNormalizedExpertControlSmoothingTimeSeconds, kModulationRateMinimum) \
    X (intelTransientCrossover, intelligence.transientCrossoverHz, intelTransientCrossover, kFilterFrequencySmoothingTimeSeconds, kTransientCrossoverMinimum)

void ParameterSnapshot::prepare (double sampleRate,
                                 int maximumBlockSize,
                                 juce::AudioProcessorValueTreeState& apvts)
{
    juce::ignoreUnused (maximumBlockSize);

#define CACHE_PARAMETER(member, id) \
    rawParameters_[index (Parameter::member)] = apvts.getRawParameterValue (paramids::id);
    SHREDSPACE_PARAMETER_IDS (CACHE_PARAMETER)
#undef CACHE_PARAMETER

#if JUCE_DEBUG
    for (auto* parameter : rawParameters_)
        jassert (parameter != nullptr);
#endif

#define PREPARE_LINEAR(member, destination, parameter, seconds) \
    linear (LinearValue::member).reset (sampleRate, seconds); \
    linear (LinearValue::member).setCurrentAndTargetValue (read (Parameter::parameter));
    SHREDSPACE_LINEAR_VALUES (PREPARE_LINEAR)
#undef PREPARE_LINEAR

#define PREPARE_MULTIPLICATIVE(member, destination, parameter, seconds, minimum) \
    multiplicative (MultiplicativeValue::member).reset (sampleRate, seconds); \
    multiplicative (MultiplicativeValue::member).setCurrentAndTargetValue (std::max (minimum, read (Parameter::parameter)));
    SHREDSPACE_MULTIPLICATIVE_VALUES (PREPARE_MULTIPLICATIVE)
#undef PREPARE_MULTIPLICATIVE

    updateFromParameters();
    advance (0);
}

void ParameterSnapshot::updateFromParameters()
{
    // Real-time contract: cached atomic reads and target updates only; no
    // allocation, locks, I/O, strings, tree traversal, GUI calls, or resizing.
#define UPDATE_LINEAR(member, destination, parameter, seconds) \
    linear (LinearValue::member).setTargetValue (read (Parameter::parameter));
    SHREDSPACE_LINEAR_VALUES (UPDATE_LINEAR)
#undef UPDATE_LINEAR

#define UPDATE_MULTIPLICATIVE(member, destination, parameter, seconds, minimum) \
    multiplicative (MultiplicativeValue::member).setTargetValue (std::max (minimum, read (Parameter::parameter)));
    SHREDSPACE_MULTIPLICATIVE_VALUES (UPDATE_MULTIPLICATIVE)
#undef UPDATE_MULTIPLICATIVE

    values_.bypass = read (Parameter::globalBypass) >= 0.5f;
    values_.bypassMode = static_cast<int> (read (Parameter::globalBypassMode));
    values_.mixMode = static_cast<int> (read (Parameter::globalMixMode));
    values_.spillover = read (Parameter::globalSpillover) >= 0.5f;
    values_.safetyLimiter = read (Parameter::globalSafetyLimiter) >= 0.5f;

    values_.tap = read (Parameter::perfTap) >= 0.5f;
    values_.hold = read (Parameter::perfHold) >= 0.5f;
    values_.holdMode = static_cast<int> (read (Parameter::perfHoldMode));
    values_.adaptiveMode = static_cast<int> (read (Parameter::perfAdaptiveMode));
    values_.routing = static_cast<int> (read (Parameter::perfRouting));
    values_.lfMono = read (Parameter::stereoLfMono) >= 0.5f;
    values_.sceneSelect = static_cast<int> (read (Parameter::sceneSelect));
    values_.voiceA.enabled = read (Parameter::voiceAEnable) >= 0.5f;
    values_.voiceB.enabled = read (Parameter::voiceBEnable) >= 0.5f;

    // Delay time is deliberately unsmoothed here. The Smooth/Bend transition
    // engine owns macro time, ratios, fine/stereo offsets, glide and division.
    values_.macroTimeMs = read (Parameter::perfMacroTime);
    values_.timeMode = static_cast<int> (read (Parameter::timeMode));
    values_.syncDivision = static_cast<int> (read (Parameter::timeSyncDivision));
    values_.timeGlideMs = read (Parameter::timeGlide);
    values_.voiceA.timeRatio = read (Parameter::voiceATimeRatio);
    values_.voiceA.fineOffsetMs = read (Parameter::voiceAFineOffset);
    values_.voiceA.stereoOffsetMs = read (Parameter::voiceAStereoOffset);
    values_.voiceB.timeRatio = read (Parameter::voiceBTimeRatio);
    values_.voiceB.fineOffsetMs = read (Parameter::voiceBFineOffset);
    values_.voiceB.stereoOffsetMs = read (Parameter::voiceBStereoOffset);
}

void ParameterSnapshot::advance (int numSamples)
{
    // Real-time contract: bounded in-place smoother advancement only; no
    // allocation, locks, I/O, strings, GUI calls, resizing, or heap destruction.
    jassert (numSamples >= 0);
    const auto samplesToAdvance = std::max (0, numSamples);

#define ADVANCE_LINEAR(member, destination, parameter, seconds) \
    values_.destination = linear (LinearValue::member).skip (samplesToAdvance);
    SHREDSPACE_LINEAR_VALUES (ADVANCE_LINEAR)
#undef ADVANCE_LINEAR

#define ADVANCE_MULTIPLICATIVE(member, destination, parameter, seconds, minimum) \
    values_.destination = multiplicative (MultiplicativeValue::member).skip (samplesToAdvance);
    SHREDSPACE_MULTIPLICATIVE_VALUES (ADVANCE_MULTIPLICATIVE)
#undef ADVANCE_MULTIPLICATIVE

    // Every dB-domain value remains linear-smoothed in dB until this point.
    values_.inputGain = std::pow (10.0f, values_.inputGainDb / 20.0f);
    values_.outputGain = std::pow (10.0f, values_.outputGainDb / 20.0f);
    values_.voiceA.levelGain = std::pow (10.0f, values_.voiceA.levelDb / 20.0f);
    values_.voiceA.presenceGuardGain = std::pow (10.0f, values_.voiceA.presenceGuardDb / 20.0f);
    values_.voiceB.levelGain = std::pow (10.0f, values_.voiceB.levelDb / 20.0f);
    values_.voiceB.presenceGuardGain = std::pow (10.0f, values_.voiceB.presenceGuardDb / 20.0f);
    values_.intelligence.inputGateGain = std::pow (10.0f, values_.intelligence.inputGateDb / 20.0f);
    values_.intelligence.wetReductionGain = std::pow (10.0f, values_.intelligence.wetReductionDb / 20.0f);
    values_.intelligence.wetLiftGain = std::pow (10.0f, values_.intelligence.wetLiftDb / 20.0f);
}

float ParameterSnapshot::read (Parameter parameter) const noexcept
{
    return rawParameters_[index (parameter)]->load (std::memory_order_relaxed);
}

ParameterSnapshot::LinearSmoother& ParameterSnapshot::linear (LinearValue value) noexcept
{
    return linearSmoothers_[index (value)];
}

ParameterSnapshot::MultiplicativeSmoother& ParameterSnapshot::multiplicative (MultiplicativeValue value) noexcept
{
    return multiplicativeSmoothers_[index (value)];
}

#undef SHREDSPACE_MULTIPLICATIVE_VALUES
#undef SHREDSPACE_LINEAR_VALUES
#undef SHREDSPACE_PARAMETER_IDS

} // namespace shredspace
