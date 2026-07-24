#pragma once

#include <string_view>

// =============================================================================
// FORBIDDEN INTERNAL STATE:
// The following internal state is FORBIDDEN from ever appearing in APVTS or in
// this header: detector running means and deviations, FFT history, onset
// timestamps, runtime activity/density/sustain/phrase-gap values, macro
// destination tables, scene names, profile names/descriptions/tags, migration
// metadata, telemetry values, safety intervention state, delay-buffer contents,
// resampler state.
// =============================================================================

namespace shredspace::paramids {

inline constexpr int kStateSchemaVersion = 1;
inline constexpr const char* kStateRootType = "ShredSpaceState";
inline constexpr const char* kApvtsType = "PARAMETERS";
inline constexpr const char* kSchemaVersionProperty = "schemaVersion";

// =============================================================================
// GLOBAL
// =============================================================================
inline constexpr const char* GLOBAL_INPUT_GAIN = "GLOBAL_INPUT_GAIN";
inline constexpr const char* kGLOBAL_INPUT_GAIN = "GLOBAL_INPUT_GAIN";
inline constexpr const char* kGlobalInputGain = "GLOBAL_INPUT_GAIN";

inline constexpr const char* GLOBAL_OUTPUT_GAIN = "GLOBAL_OUTPUT_GAIN";
inline constexpr const char* kGLOBAL_OUTPUT_GAIN = "GLOBAL_OUTPUT_GAIN";
inline constexpr const char* kGlobalOutputGain = "GLOBAL_OUTPUT_GAIN";

inline constexpr const char* GLOBAL_MIX = "GLOBAL_MIX";
inline constexpr const char* kGLOBAL_MIX = "GLOBAL_MIX";
inline constexpr const char* kGlobalMix = "GLOBAL_MIX";

inline constexpr const char* GLOBAL_BYPASS = "GLOBAL_BYPASS";
inline constexpr const char* kGLOBAL_BYPASS = "GLOBAL_BYPASS";
inline constexpr const char* kGlobalBypass = "GLOBAL_BYPASS";

inline constexpr const char* GLOBAL_BYPASS_MODE = "GLOBAL_BYPASS_MODE";
inline constexpr const char* kGLOBAL_BYPASS_MODE = "GLOBAL_BYPASS_MODE";
inline constexpr const char* kGlobalBypassMode = "GLOBAL_BYPASS_MODE";

inline constexpr const char* GLOBAL_MIX_MODE = "GLOBAL_MIX_MODE";
inline constexpr const char* kGLOBAL_MIX_MODE = "GLOBAL_MIX_MODE";
inline constexpr const char* kGlobalMixMode = "GLOBAL_MIX_MODE";

inline constexpr const char* GLOBAL_SPILLOVER = "GLOBAL_SPILLOVER";
inline constexpr const char* kGLOBAL_SPILLOVER = "GLOBAL_SPILLOVER";
inline constexpr const char* kGlobalSpillover = "GLOBAL_SPILLOVER";

inline constexpr const char* GLOBAL_SAFETY_LIMITER = "GLOBAL_SAFETY_LIMITER";
inline constexpr const char* kGLOBAL_SAFETY_LIMITER = "GLOBAL_SAFETY_LIMITER";
inline constexpr const char* kGlobalSafetyLimiter = "GLOBAL_SAFETY_LIMITER";

// =============================================================================
// PERF
// =============================================================================
inline constexpr const char* PERF_MACRO_TIME = "PERF_MACRO_TIME";
inline constexpr const char* kPERF_MACRO_TIME = "PERF_MACRO_TIME";
inline constexpr const char* kPerfMacroTime = "PERF_MACRO_TIME";

inline constexpr const char* PERF_MACRO_SPACE = "PERF_MACRO_SPACE";
inline constexpr const char* kPERF_MACRO_SPACE = "PERF_MACRO_SPACE";
inline constexpr const char* kPerfMacroSpace = "PERF_MACRO_SPACE";

inline constexpr const char* PERF_MACRO_DEFINITION = "PERF_MACRO_DEFINITION";
inline constexpr const char* kPERF_MACRO_DEFINITION = "PERF_MACRO_DEFINITION";
inline constexpr const char* kPerfMacroDefinition = "PERF_MACRO_DEFINITION";

inline constexpr const char* PERF_MACRO_BLOOM = "PERF_MACRO_BLOOM";
inline constexpr const char* kPERF_MACRO_BLOOM = "PERF_MACRO_BLOOM";
inline constexpr const char* kPerfMacroBloom = "PERF_MACRO_BLOOM";

inline constexpr const char* PERF_MACRO_TAIL = "PERF_MACRO_TAIL";
inline constexpr const char* kPERF_MACRO_TAIL = "PERF_MACRO_TAIL";
inline constexpr const char* kPerfMacroTail = "PERF_MACRO_TAIL";

inline constexpr const char* PERF_MACRO_FOCUS = "PERF_MACRO_FOCUS";
inline constexpr const char* kPERF_MACRO_FOCUS = "PERF_MACRO_FOCUS";
inline constexpr const char* kPerfMacroFocus = "PERF_MACRO_FOCUS";

inline constexpr const char* PERF_MACRO_WIDTH = "PERF_MACRO_WIDTH";
inline constexpr const char* kPERF_MACRO_WIDTH = "PERF_MACRO_WIDTH";
inline constexpr const char* kPerfMacroWidth = "PERF_MACRO_WIDTH";

inline constexpr const char* PERF_MACRO_CHARACTER = "PERF_MACRO_CHARACTER";
inline constexpr const char* kPERF_MACRO_CHARACTER = "PERF_MACRO_CHARACTER";
inline constexpr const char* kPerfMacroCharacter = "PERF_MACRO_CHARACTER";

inline constexpr const char* PERF_TAP = "PERF_TAP";
inline constexpr const char* kPERF_TAP = "PERF_TAP";
inline constexpr const char* kPerfTap = "PERF_TAP";

inline constexpr const char* PERF_HOLD = "PERF_HOLD";
inline constexpr const char* kPERF_HOLD = "PERF_HOLD";
inline constexpr const char* kPerfHold = "PERF_HOLD";

inline constexpr const char* PERF_HOLD_MODE = "PERF_HOLD_MODE";
inline constexpr const char* kPERF_HOLD_MODE = "PERF_HOLD_MODE";
inline constexpr const char* kPerfHoldMode = "PERF_HOLD_MODE";

inline constexpr const char* PERF_ADAPTIVE_MODE = "PERF_ADAPTIVE_MODE";
inline constexpr const char* kPERF_ADAPTIVE_MODE = "PERF_ADAPTIVE_MODE";
inline constexpr const char* kPerfAdaptiveMode = "PERF_ADAPTIVE_MODE";

inline constexpr const char* PERF_ROUTING = "PERF_ROUTING";
inline constexpr const char* kPERF_ROUTING = "PERF_ROUTING";
inline constexpr const char* kPerfRouting = "PERF_ROUTING";

inline constexpr const char* PERF_ROUTING_CASCADE = "PERF_ROUTING_CASCADE";
inline constexpr const char* kPERF_ROUTING_CASCADE = "PERF_ROUTING_CASCADE";
inline constexpr const char* kPerfRoutingCascade = "PERF_ROUTING_CASCADE";

// =============================================================================
// TIME
// =============================================================================
inline constexpr const char* TIME_MODE = "TIME_MODE";
inline constexpr const char* kTIME_MODE = "TIME_MODE";
inline constexpr const char* kTimeMode = "TIME_MODE";

inline constexpr const char* TIME_SYNC_DIVISION = "TIME_SYNC_DIVISION";
inline constexpr const char* kTIME_SYNC_DIVISION = "TIME_SYNC_DIVISION";
inline constexpr const char* kTimeSyncDivision = "TIME_SYNC_DIVISION";

inline constexpr const char* TIME_GLIDE = "TIME_GLIDE";
inline constexpr const char* kTIME_GLIDE = "TIME_GLIDE";
inline constexpr const char* kTimeGlide = "TIME_GLIDE";

// =============================================================================
// STEREO
// =============================================================================
inline constexpr const char* STEREO_LF_MONO = "STEREO_LF_MONO";
inline constexpr const char* kSTEREO_LF_MONO = "STEREO_LF_MONO";
inline constexpr const char* kStereoLfMono = "STEREO_LF_MONO";

inline constexpr const char* STEREO_LF_MONO_FREQ = "STEREO_LF_MONO_FREQ";
inline constexpr const char* kSTEREO_LF_MONO_FREQ = "STEREO_LF_MONO_FREQ";
inline constexpr const char* kStereoLfMonoFreq = "STEREO_LF_MONO_FREQ";

// =============================================================================
// SCENE
// =============================================================================
inline constexpr const char* SCENE_SELECT = "SCENE_SELECT";
inline constexpr const char* kSCENE_SELECT = "SCENE_SELECT";
inline constexpr const char* kSceneSelect = "SCENE_SELECT";

inline constexpr const char* SCENE_MORPH = "SCENE_MORPH";
inline constexpr const char* kSCENE_MORPH = "SCENE_MORPH";
inline constexpr const char* kSceneMorph = "SCENE_MORPH";

// =============================================================================
// VOICE_A
// =============================================================================
inline constexpr const char* VOICE_A_ENABLE = "VOICE_A_ENABLE";
inline constexpr const char* kVOICE_A_ENABLE = "VOICE_A_ENABLE";
inline constexpr const char* kVoiceAEnable = "VOICE_A_ENABLE";

inline constexpr const char* VOICE_A_TIME_RATIO = "VOICE_A_TIME_RATIO";
inline constexpr const char* kVOICE_A_TIME_RATIO = "VOICE_A_TIME_RATIO";
inline constexpr const char* kVoiceATimeRatio = "VOICE_A_TIME_RATIO";

inline constexpr const char* VOICE_A_FINE_OFFSET = "VOICE_A_FINE_OFFSET";
inline constexpr const char* kVOICE_A_FINE_OFFSET = "VOICE_A_FINE_OFFSET";
inline constexpr const char* kVoiceAFineOffset = "VOICE_A_FINE_OFFSET";

inline constexpr const char* VOICE_A_LEVEL = "VOICE_A_LEVEL";
inline constexpr const char* kVOICE_A_LEVEL = "VOICE_A_LEVEL";
inline constexpr const char* kVoiceALevel = "VOICE_A_LEVEL";

inline constexpr const char* VOICE_A_TAIL_SCALE = "VOICE_A_TAIL_SCALE";
inline constexpr const char* kVOICE_A_TAIL_SCALE = "VOICE_A_TAIL_SCALE";
inline constexpr const char* kVoiceATailScale = "VOICE_A_TAIL_SCALE";

inline constexpr const char* VOICE_A_ADAPTIVE_ROLE = "VOICE_A_ADAPTIVE_ROLE";
inline constexpr const char* kVOICE_A_ADAPTIVE_ROLE = "VOICE_A_ADAPTIVE_ROLE";
inline constexpr const char* kVoiceAAdaptiveRole = "VOICE_A_ADAPTIVE_ROLE";

inline constexpr const char* VOICE_A_PAN = "VOICE_A_PAN";
inline constexpr const char* kVOICE_A_PAN = "VOICE_A_PAN";
inline constexpr const char* kVoiceAPan = "VOICE_A_PAN";

inline constexpr const char* VOICE_A_WIDTH = "VOICE_A_WIDTH";
inline constexpr const char* kVOICE_A_WIDTH = "VOICE_A_WIDTH";
inline constexpr const char* kVoiceAWidth = "VOICE_A_WIDTH";

inline constexpr const char* VOICE_A_STEREO_OFFSET = "VOICE_A_STEREO_OFFSET";
inline constexpr const char* kVOICE_A_STEREO_OFFSET = "VOICE_A_STEREO_OFFSET";
inline constexpr const char* kVoiceAStereoOffset = "VOICE_A_STEREO_OFFSET";

inline constexpr const char* VOICE_A_CROSS_FEEDBACK = "VOICE_A_CROSS_FEEDBACK";
inline constexpr const char* kVOICE_A_CROSS_FEEDBACK = "VOICE_A_CROSS_FEEDBACK";
inline constexpr const char* kVoiceACrossFeedback = "VOICE_A_CROSS_FEEDBACK";

inline constexpr const char* VOICE_A_MOD_PHASE = "VOICE_A_MOD_PHASE";
inline constexpr const char* kVOICE_A_MOD_PHASE = "VOICE_A_MOD_PHASE";
inline constexpr const char* kVoiceAModPhase = "VOICE_A_MOD_PHASE";

inline constexpr const char* VOICE_A_HPF = "VOICE_A_HPF";
inline constexpr const char* kVOICE_A_HPF = "VOICE_A_HPF";
inline constexpr const char* kVoiceAHpf = "VOICE_A_HPF";

inline constexpr const char* VOICE_A_LPF = "VOICE_A_LPF";
inline constexpr const char* kVOICE_A_LPF = "VOICE_A_LPF";
inline constexpr const char* kVoiceALpf = "VOICE_A_LPF";

inline constexpr const char* VOICE_A_PRESENCE_FREQ = "VOICE_A_PRESENCE_FREQ";
inline constexpr const char* kVOICE_A_PRESENCE_FREQ = "VOICE_A_PRESENCE_FREQ";
inline constexpr const char* kVoiceAPresenceFreq = "VOICE_A_PRESENCE_FREQ";

inline constexpr const char* VOICE_A_PRESENCE_GUARD = "VOICE_A_PRESENCE_GUARD";
inline constexpr const char* kVOICE_A_PRESENCE_GUARD = "VOICE_A_PRESENCE_GUARD";
inline constexpr const char* kVoiceAPresenceGuard = "VOICE_A_PRESENCE_GUARD";

inline constexpr const char* VOICE_A_ATTACK_SOFTEN = "VOICE_A_ATTACK_SOFTEN";
inline constexpr const char* kVOICE_A_ATTACK_SOFTEN = "VOICE_A_ATTACK_SOFTEN";
inline constexpr const char* kVoiceAAttackSoften = "VOICE_A_ATTACK_SOFTEN";

inline constexpr const char* VOICE_A_MOD_RATE = "VOICE_A_MOD_RATE";
inline constexpr const char* kVOICE_A_MOD_RATE = "VOICE_A_MOD_RATE";
inline constexpr const char* kVoiceAModRate = "VOICE_A_MOD_RATE";

inline constexpr const char* VOICE_A_MOD_DEPTH = "VOICE_A_MOD_DEPTH";
inline constexpr const char* kVOICE_A_MOD_DEPTH = "VOICE_A_MOD_DEPTH";
inline constexpr const char* kVoiceAModDepth = "VOICE_A_MOD_DEPTH";

inline constexpr const char* VOICE_A_DRIFT = "VOICE_A_DRIFT";
inline constexpr const char* kVOICE_A_DRIFT = "VOICE_A_DRIFT";
inline constexpr const char* kVoiceADrift = "VOICE_A_DRIFT";

inline constexpr const char* VOICE_A_DIFFUSION = "VOICE_A_DIFFUSION";
inline constexpr const char* kVOICE_A_DIFFUSION = "VOICE_A_DIFFUSION";
inline constexpr const char* kVoiceADiffusion = "VOICE_A_DIFFUSION";

inline constexpr const char* VOICE_A_SATURATION = "VOICE_A_SATURATION";
inline constexpr const char* kVOICE_A_SATURATION = "VOICE_A_SATURATION";
inline constexpr const char* kVoiceASaturation = "VOICE_A_SATURATION";

// =============================================================================
// VOICE_B
// =============================================================================
inline constexpr const char* VOICE_B_ENABLE = "VOICE_B_ENABLE";
inline constexpr const char* kVOICE_B_ENABLE = "VOICE_B_ENABLE";
inline constexpr const char* kVoiceBEnable = "VOICE_B_ENABLE";

inline constexpr const char* VOICE_B_TIME_RATIO = "VOICE_B_TIME_RATIO";
inline constexpr const char* kVOICE_B_TIME_RATIO = "VOICE_B_TIME_RATIO";
inline constexpr const char* kVoiceBTimeRatio = "VOICE_B_TIME_RATIO";

inline constexpr const char* VOICE_B_FINE_OFFSET = "VOICE_B_FINE_OFFSET";
inline constexpr const char* kVOICE_B_FINE_OFFSET = "VOICE_B_FINE_OFFSET";
inline constexpr const char* kVoiceBFineOffset = "VOICE_B_FINE_OFFSET";

inline constexpr const char* VOICE_B_LEVEL = "VOICE_B_LEVEL";
inline constexpr const char* kVOICE_B_LEVEL = "VOICE_B_LEVEL";
inline constexpr const char* kVoiceBLevel = "VOICE_B_LEVEL";

inline constexpr const char* VOICE_B_TAIL_SCALE = "VOICE_B_TAIL_SCALE";
inline constexpr const char* kVOICE_B_TAIL_SCALE = "VOICE_B_TAIL_SCALE";
inline constexpr const char* kVoiceBTailScale = "VOICE_B_TAIL_SCALE";

inline constexpr const char* VOICE_B_ADAPTIVE_ROLE = "VOICE_B_ADAPTIVE_ROLE";
inline constexpr const char* kVOICE_B_ADAPTIVE_ROLE = "VOICE_B_ADAPTIVE_ROLE";
inline constexpr const char* kVoiceBAdaptiveRole = "VOICE_B_ADAPTIVE_ROLE";

inline constexpr const char* VOICE_B_PAN = "VOICE_B_PAN";
inline constexpr const char* kVOICE_B_PAN = "VOICE_B_PAN";
inline constexpr const char* kVoiceBPan = "VOICE_B_PAN";

inline constexpr const char* VOICE_B_WIDTH = "VOICE_B_WIDTH";
inline constexpr const char* kVOICE_B_WIDTH = "VOICE_B_WIDTH";
inline constexpr const char* kVoiceBWidth = "VOICE_B_WIDTH";

inline constexpr const char* VOICE_B_STEREO_OFFSET = "VOICE_B_STEREO_OFFSET";
inline constexpr const char* kVOICE_B_STEREO_OFFSET = "VOICE_B_STEREO_OFFSET";
inline constexpr const char* kVoiceBStereoOffset = "VOICE_B_STEREO_OFFSET";

inline constexpr const char* VOICE_B_CROSS_FEEDBACK = "VOICE_B_CROSS_FEEDBACK";
inline constexpr const char* kVOICE_B_CROSS_FEEDBACK = "VOICE_B_CROSS_FEEDBACK";
inline constexpr const char* kVoiceBCrossFeedback = "VOICE_B_CROSS_FEEDBACK";

inline constexpr const char* VOICE_B_MOD_PHASE = "VOICE_B_MOD_PHASE";
inline constexpr const char* kVOICE_B_MOD_PHASE = "VOICE_B_MOD_PHASE";
inline constexpr const char* kVoiceBModPhase = "VOICE_B_MOD_PHASE";

inline constexpr const char* VOICE_B_HPF = "VOICE_B_HPF";
inline constexpr const char* kVOICE_B_HPF = "VOICE_B_HPF";
inline constexpr const char* kVoiceBHpf = "VOICE_B_HPF";

inline constexpr const char* VOICE_B_LPF = "VOICE_B_LPF";
inline constexpr const char* kVOICE_B_LPF = "VOICE_B_LPF";
inline constexpr const char* kVoiceBLpf = "VOICE_B_LPF";

inline constexpr const char* VOICE_B_PRESENCE_FREQ = "VOICE_B_PRESENCE_FREQ";
inline constexpr const char* kVOICE_B_PRESENCE_FREQ = "VOICE_B_PRESENCE_FREQ";
inline constexpr const char* kVoiceBPresenceFreq = "VOICE_B_PRESENCE_FREQ";

inline constexpr const char* VOICE_B_PRESENCE_GUARD = "VOICE_B_PRESENCE_GUARD";
inline constexpr const char* kVOICE_B_PRESENCE_GUARD = "VOICE_B_PRESENCE_GUARD";
inline constexpr const char* kVoiceBPresenceGuard = "VOICE_B_PRESENCE_GUARD";

inline constexpr const char* VOICE_B_ATTACK_SOFTEN = "VOICE_B_ATTACK_SOFTEN";
inline constexpr const char* kVOICE_B_ATTACK_SOFTEN = "VOICE_B_ATTACK_SOFTEN";
inline constexpr const char* kVoiceBAttackSoften = "VOICE_B_ATTACK_SOFTEN";

inline constexpr const char* VOICE_B_MOD_RATE = "VOICE_B_MOD_RATE";
inline constexpr const char* kVOICE_B_MOD_RATE = "VOICE_B_MOD_RATE";
inline constexpr const char* kVoiceBModRate = "VOICE_B_MOD_RATE";

inline constexpr const char* VOICE_B_MOD_DEPTH = "VOICE_B_MOD_DEPTH";
inline constexpr const char* kVOICE_B_MOD_DEPTH = "VOICE_B_MOD_DEPTH";
inline constexpr const char* kVoiceBModDepth = "VOICE_B_MOD_DEPTH";

inline constexpr const char* VOICE_B_DRIFT = "VOICE_B_DRIFT";
inline constexpr const char* kVOICE_B_DRIFT = "VOICE_B_DRIFT";
inline constexpr const char* kVoiceBDrift = "VOICE_B_DRIFT";

inline constexpr const char* VOICE_B_DIFFUSION = "VOICE_B_DIFFUSION";
inline constexpr const char* kVOICE_B_DIFFUSION = "VOICE_B_DIFFUSION";
inline constexpr const char* kVoiceBDiffusion = "VOICE_B_DIFFUSION";

inline constexpr const char* VOICE_B_SATURATION = "VOICE_B_SATURATION";
inline constexpr const char* kVOICE_B_SATURATION = "VOICE_B_SATURATION";
inline constexpr const char* kVoiceBSaturation = "VOICE_B_SATURATION";

// =============================================================================
// INTEL
// =============================================================================
inline constexpr const char* INTEL_SENSITIVITY = "INTEL_SENSITIVITY";
inline constexpr const char* kINTEL_SENSITIVITY = "INTEL_SENSITIVITY";
inline constexpr const char* kIntelSensitivity = "INTEL_SENSITIVITY";

inline constexpr const char* INTEL_INPUT_GATE = "INTEL_INPUT_GATE";
inline constexpr const char* kINTEL_INPUT_GATE = "INTEL_INPUT_GATE";
inline constexpr const char* kIntelInputGate = "INTEL_INPUT_GATE";

inline constexpr const char* INTEL_DENSITY_WINDOW = "INTEL_DENSITY_WINDOW";
inline constexpr const char* kINTEL_DENSITY_WINDOW = "INTEL_DENSITY_WINDOW";
inline constexpr const char* kIntelDensityWindow = "INTEL_DENSITY_WINDOW";

inline constexpr const char* INTEL_DENSITY_START = "INTEL_DENSITY_START";
inline constexpr const char* kINTEL_DENSITY_START = "INTEL_DENSITY_START";
inline constexpr const char* kIntelDensityStart = "INTEL_DENSITY_START";

inline constexpr const char* INTEL_DENSITY_FULL = "INTEL_DENSITY_FULL";
inline constexpr const char* kINTEL_DENSITY_FULL = "INTEL_DENSITY_FULL";
inline constexpr const char* kIntelDensityFull = "INTEL_DENSITY_FULL";

inline constexpr const char* INTEL_SUSTAIN_START = "INTEL_SUSTAIN_START";
inline constexpr const char* kINTEL_SUSTAIN_START = "INTEL_SUSTAIN_START";
inline constexpr const char* kIntelSustainStart = "INTEL_SUSTAIN_START";

inline constexpr const char* INTEL_SUSTAIN_FULL = "INTEL_SUSTAIN_FULL";
inline constexpr const char* kINTEL_SUSTAIN_FULL = "INTEL_SUSTAIN_FULL";
inline constexpr const char* kIntelSustainFull = "INTEL_SUSTAIN_FULL";

inline constexpr const char* INTEL_PHRASE_GAP = "INTEL_PHRASE_GAP";
inline constexpr const char* kINTEL_PHRASE_GAP = "INTEL_PHRASE_GAP";
inline constexpr const char* kIntelPhraseGap = "INTEL_PHRASE_GAP";

inline constexpr const char* INTEL_CONTRACTION_ATTACK = "INTEL_CONTRACTION_ATTACK";
inline constexpr const char* kINTEL_CONTRACTION_ATTACK = "INTEL_CONTRACTION_ATTACK";
inline constexpr const char* kIntelContractionAttack = "INTEL_CONTRACTION_ATTACK";

inline constexpr const char* INTEL_CONTRACTION_RELEASE = "INTEL_CONTRACTION_RELEASE";
inline constexpr const char* kINTEL_CONTRACTION_RELEASE = "INTEL_CONTRACTION_RELEASE";
inline constexpr const char* kIntelContractionRelease = "INTEL_CONTRACTION_RELEASE";

inline constexpr const char* INTEL_WET_REDUCTION = "INTEL_WET_REDUCTION";
inline constexpr const char* kINTEL_WET_REDUCTION = "INTEL_WET_REDUCTION";
inline constexpr const char* kIntelWetReduction = "INTEL_WET_REDUCTION";

inline constexpr const char* INTEL_TAIL_REDUCTION = "INTEL_TAIL_REDUCTION";
inline constexpr const char* kINTEL_TAIL_REDUCTION = "INTEL_TAIL_REDUCTION";
inline constexpr const char* kIntelTailReduction = "INTEL_TAIL_REDUCTION";

inline constexpr const char* INTEL_TONE_DARKENING = "INTEL_TONE_DARKENING";
inline constexpr const char* kINTEL_TONE_DARKENING = "INTEL_TONE_DARKENING";
inline constexpr const char* kIntelToneDarkening = "INTEL_TONE_DARKENING";

inline constexpr const char* INTEL_WIDTH_REDUCTION = "INTEL_WIDTH_REDUCTION";
inline constexpr const char* kINTEL_WIDTH_REDUCTION = "INTEL_WIDTH_REDUCTION";
inline constexpr const char* kIntelWidthReduction = "INTEL_WIDTH_REDUCTION";

inline constexpr const char* INTEL_ATTACK_SOFTEN_INCREASE = "INTEL_ATTACK_SOFTEN_INCREASE";
inline constexpr const char* kINTEL_ATTACK_SOFTEN_INCREASE = "INTEL_ATTACK_SOFTEN_INCREASE";
inline constexpr const char* kIntelAttackSoftenIncrease = "INTEL_ATTACK_SOFTEN_INCREASE";

inline constexpr const char* INTEL_BLOOM_RISE = "INTEL_BLOOM_RISE";
inline constexpr const char* kINTEL_BLOOM_RISE = "INTEL_BLOOM_RISE";
inline constexpr const char* kIntelBloomRise = "INTEL_BLOOM_RISE";

inline constexpr const char* INTEL_BLOOM_HOLD = "INTEL_BLOOM_HOLD";
inline constexpr const char* kINTEL_BLOOM_HOLD = "INTEL_BLOOM_HOLD";
inline constexpr const char* kIntelBloomHold = "INTEL_BLOOM_HOLD";

inline constexpr const char* INTEL_BLOOM_RELEASE = "INTEL_BLOOM_RELEASE";
inline constexpr const char* kINTEL_BLOOM_RELEASE = "INTEL_BLOOM_RELEASE";
inline constexpr const char* kIntelBloomRelease = "INTEL_BLOOM_RELEASE";

inline constexpr const char* INTEL_WET_LIFT = "INTEL_WET_LIFT";
inline constexpr const char* kINTEL_WET_LIFT = "INTEL_WET_LIFT";
inline constexpr const char* kIntelWetLift = "INTEL_WET_LIFT";

inline constexpr const char* INTEL_TAIL_EXPANSION = "INTEL_TAIL_EXPANSION";
inline constexpr const char* kINTEL_TAIL_EXPANSION = "INTEL_TAIL_EXPANSION";
inline constexpr const char* kIntelTailExpansion = "INTEL_TAIL_EXPANSION";

inline constexpr const char* INTEL_TONE_OPENING = "INTEL_TONE_OPENING";
inline constexpr const char* kINTEL_TONE_OPENING = "INTEL_TONE_OPENING";
inline constexpr const char* kIntelToneOpening = "INTEL_TONE_OPENING";

inline constexpr const char* INTEL_WIDTH_EXPANSION = "INTEL_WIDTH_EXPANSION";
inline constexpr const char* kINTEL_WIDTH_EXPANSION = "INTEL_WIDTH_EXPANSION";
inline constexpr const char* kIntelWidthExpansion = "INTEL_WIDTH_EXPANSION";

inline constexpr const char* INTEL_DIFFUSION_INCREASE = "INTEL_DIFFUSION_INCREASE";
inline constexpr const char* kINTEL_DIFFUSION_INCREASE = "INTEL_DIFFUSION_INCREASE";
inline constexpr const char* kIntelDiffusionIncrease = "INTEL_DIFFUSION_INCREASE";

inline constexpr const char* INTEL_TRANSIENT_CROSSOVER = "INTEL_TRANSIENT_CROSSOVER";
inline constexpr const char* kINTEL_TRANSIENT_CROSSOVER = "INTEL_TRANSIENT_CROSSOVER";
inline constexpr const char* kIntelTransientCrossover = "INTEL_TRANSIENT_CROSSOVER";

} // namespace shredspace::paramids
