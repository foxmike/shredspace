#include "ParameterLayout.h"
#include "ParameterIDs.h"

namespace shredspace
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    addGlobalParameters (layout);
    addPerformanceParameters (layout);
    addTimingParameters (layout);
    addStereoParameters (layout);
    addSceneParameters (layout);
    addVoiceParameters (layout);
    addIntelligenceParameters (layout);

    return layout;
}

void addGlobalParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::GLOBAL_INPUT_GAIN, shredspace::paramids::kStateSchemaVersion },
        "Input Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel (" dB")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::GLOBAL_OUTPUT_GAIN, shredspace::paramids::kStateSchemaVersion },
        "Output Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel (" dB")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::GLOBAL_MIX, shredspace::paramids::kStateSchemaVersion },
        "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        22.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { shredspace::paramids::GLOBAL_BYPASS, shredspace::paramids::kStateSchemaVersion },
        "Bypass",
        false
    ));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { shredspace::paramids::GLOBAL_BYPASS_MODE, shredspace::paramids::kStateSchemaVersion },
        "Bypass Mode",
        juce::StringArray { "Hard", "Trails" },
        1
    ));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { shredspace::paramids::GLOBAL_MIX_MODE, shredspace::paramids::kStateSchemaVersion },
        "Mix Mode",
        juce::StringArray { "Insert", "Send" },
        0
    ));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { shredspace::paramids::GLOBAL_SPILLOVER, shredspace::paramids::kStateSchemaVersion },
        "Spillover",
        true
    ));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { shredspace::paramids::GLOBAL_SAFETY_LIMITER, shredspace::paramids::kStateSchemaVersion },
        "Safety Limiter",
        true
    ));
}

void addPerformanceParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    {
        juce::NormalisableRange<float> range (20.0f, 2000.0f);
        range.setSkewForCentre (400.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { shredspace::paramids::PERF_MACRO_TIME, shredspace::paramids::kStateSchemaVersion },
            "Macro Time",
            range,
            380.0f,
            juce::AudioParameterFloatAttributes().withLabel (" ms")
        ));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::PERF_MACRO_SPACE, shredspace::paramids::kStateSchemaVersion },
        "Macro Space",
        juce::NormalisableRange<float> (0.0f, 100.0f),
        40.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::PERF_MACRO_DEFINITION, shredspace::paramids::kStateSchemaVersion },
        "Macro Definition",
        juce::NormalisableRange<float> (0.0f, 100.0f),
        65.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::PERF_MACRO_BLOOM, shredspace::paramids::kStateSchemaVersion },
        "Macro Bloom",
        juce::NormalisableRange<float> (0.0f, 100.0f),
        45.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    {
        juce::NormalisableRange<float> range (0.15f, 12.0f);
        range.setSkewForCentre (2.5f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { shredspace::paramids::PERF_MACRO_TAIL, shredspace::paramids::kStateSchemaVersion },
            "Macro Tail",
            range,
            2.8f,
            juce::AudioParameterFloatAttributes().withLabel (" s")
        ));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::PERF_MACRO_FOCUS, shredspace::paramids::kStateSchemaVersion },
        "Macro Focus",
        juce::NormalisableRange<float> (0.0f, 100.0f),
        55.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::PERF_MACRO_WIDTH, shredspace::paramids::kStateSchemaVersion },
        "Macro Width",
        juce::NormalisableRange<float> (0.0f, 100.0f),
        65.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::PERF_MACRO_CHARACTER, shredspace::paramids::kStateSchemaVersion },
        "Macro Character",
        juce::NormalisableRange<float> (0.0f, 100.0f),
        30.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { shredspace::paramids::PERF_TAP, shredspace::paramids::kStateSchemaVersion },
        "Tap Trigger",
        false
    ));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { shredspace::paramids::PERF_HOLD, shredspace::paramids::kStateSchemaVersion },
        "Hold Trigger",
        false
    ));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { shredspace::paramids::PERF_HOLD_MODE, shredspace::paramids::kStateSchemaVersion },
        "Hold Mode",
        juce::StringArray { "Momentary", "Latch" },
        0
    ));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { shredspace::paramids::PERF_ADAPTIVE_MODE, shredspace::paramids::kStateSchemaVersion },
        "Adaptive Mode",
        juce::StringArray { "Off", "Level", "Articulation" },
        2
    ));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { shredspace::paramids::PERF_ROUTING, shredspace::paramids::kStateSchemaVersion },
        "Routing",
        juce::StringArray { "Parallel", "A to B", "B to A" },
        0
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::PERF_ROUTING_CASCADE, shredspace::paramids::kStateSchemaVersion },
        "Routing Cascade",
        juce::NormalisableRange<float> (0.0f, 100.0f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));
}

void addTimingParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { shredspace::paramids::TIME_MODE, shredspace::paramids::kStateSchemaVersion },
        "Time Mode",
        juce::StringArray { "Milliseconds", "Host Sync", "Tap" },
        0
    ));

    // TIME_SYNC_DIVISION choice entries:
    // "1/1", "1/1 D", "1/1 T", "1/2", "1/2 D", "1/2 T", "1/4", "1/4 D", "1/4 T", "1/8", "1/8 D", "1/8 T", "1/16", "1/16 D", "1/16 T"
    // Note: Dotted ("D") entries mean x1.5 multiplier, and triplet ("T") entries mean x2/3 multiplier of the plain division.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { shredspace::paramids::TIME_SYNC_DIVISION, shredspace::paramids::kStateSchemaVersion },
        "Sync Division",
        juce::StringArray { "1/1", "1/1 D", "1/1 T", "1/2", "1/2 D", "1/2 T", "1/4", "1/4 D", "1/4 T", "1/8", "1/8 D", "1/8 T", "1/16", "1/16 D", "1/16 T" },
        6
    ));

    {
        juce::NormalisableRange<float> range (5.0f, 1000.0f);
        range.setSkewForCentre (150.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { shredspace::paramids::TIME_GLIDE, shredspace::paramids::kStateSchemaVersion },
            "Time Glide",
            range,
            120.0f,
            juce::AudioParameterFloatAttributes().withLabel (" ms")
        ));
    }
}

void addStereoParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { shredspace::paramids::STEREO_LF_MONO, shredspace::paramids::kStateSchemaVersion },
        "LF Mono",
        true
    ));

    {
        juce::NormalisableRange<float> range (60.0f, 500.0f);
        range.setSkewForCentre (180.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { shredspace::paramids::STEREO_LF_MONO_FREQ, shredspace::paramids::kStateSchemaVersion },
            "LF Mono Freq",
            range,
            180.0f,
            juce::AudioParameterFloatAttributes().withLabel (" Hz")
        ));
    }
}

void addSceneParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { shredspace::paramids::SCENE_SELECT, shredspace::paramids::kStateSchemaVersion },
        "Scene Select",
        juce::StringArray { "Scene 1", "Scene 2", "Scene 3", "Scene 4" },
        0
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::SCENE_MORPH, shredspace::paramids::kStateSchemaVersion },
        "Scene Morph",
        juce::NormalisableRange<float> (0.0f, 100.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));
}

static void addOneVoice (juce::AudioProcessorValueTreeState::ParameterLayout& layout, bool isVoiceB)
{
    const char* voicePrefix = isVoiceB ? "Voice B " : "Voice A ";

    const char* enableId = isVoiceB ? shredspace::paramids::VOICE_B_ENABLE : shredspace::paramids::VOICE_A_ENABLE;
    const char* timeRatioId = isVoiceB ? shredspace::paramids::VOICE_B_TIME_RATIO : shredspace::paramids::VOICE_A_TIME_RATIO;
    const char* fineOffsetId = isVoiceB ? shredspace::paramids::VOICE_B_FINE_OFFSET : shredspace::paramids::VOICE_A_FINE_OFFSET;
    const char* levelId = isVoiceB ? shredspace::paramids::VOICE_B_LEVEL : shredspace::paramids::VOICE_A_LEVEL;
    const char* tailScaleId = isVoiceB ? shredspace::paramids::VOICE_B_TAIL_SCALE : shredspace::paramids::VOICE_A_TAIL_SCALE;
    const char* adaptiveRoleId = isVoiceB ? shredspace::paramids::VOICE_B_ADAPTIVE_ROLE : shredspace::paramids::VOICE_A_ADAPTIVE_ROLE;
    const char* panId = isVoiceB ? shredspace::paramids::VOICE_B_PAN : shredspace::paramids::VOICE_A_PAN;
    const char* widthId = isVoiceB ? shredspace::paramids::VOICE_B_WIDTH : shredspace::paramids::VOICE_A_WIDTH;
    const char* stereoOffsetId = isVoiceB ? shredspace::paramids::VOICE_B_STEREO_OFFSET : shredspace::paramids::VOICE_A_STEREO_OFFSET;
    const char* crossFeedbackId = isVoiceB ? shredspace::paramids::VOICE_B_CROSS_FEEDBACK : shredspace::paramids::VOICE_A_CROSS_FEEDBACK;
    const char* modPhaseId = isVoiceB ? shredspace::paramids::VOICE_B_MOD_PHASE : shredspace::paramids::VOICE_A_MOD_PHASE;
    const char* hpfId = isVoiceB ? shredspace::paramids::VOICE_B_HPF : shredspace::paramids::VOICE_A_HPF;
    const char* lpfId = isVoiceB ? shredspace::paramids::VOICE_B_LPF : shredspace::paramids::VOICE_A_LPF;
    const char* presenceFreqId = isVoiceB ? shredspace::paramids::VOICE_B_PRESENCE_FREQ : shredspace::paramids::VOICE_A_PRESENCE_FREQ;
    const char* presenceGuardId = isVoiceB ? shredspace::paramids::VOICE_B_PRESENCE_GUARD : shredspace::paramids::VOICE_A_PRESENCE_GUARD;
    const char* attackSoftenId = isVoiceB ? shredspace::paramids::VOICE_B_ATTACK_SOFTEN : shredspace::paramids::VOICE_A_ATTACK_SOFTEN;
    const char* modRateId = isVoiceB ? shredspace::paramids::VOICE_B_MOD_RATE : shredspace::paramids::VOICE_A_MOD_RATE;
    const char* modDepthId = isVoiceB ? shredspace::paramids::VOICE_B_MOD_DEPTH : shredspace::paramids::VOICE_A_MOD_DEPTH;
    const char* driftId = isVoiceB ? shredspace::paramids::VOICE_B_DRIFT : shredspace::paramids::VOICE_A_DRIFT;
    const char* diffusionId = isVoiceB ? shredspace::paramids::VOICE_B_DIFFUSION : shredspace::paramids::VOICE_A_DIFFUSION;
    const char* saturationId = isVoiceB ? shredspace::paramids::VOICE_B_SATURATION : shredspace::paramids::VOICE_A_SATURATION;

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { enableId, shredspace::paramids::kStateSchemaVersion },
        juce::String (voicePrefix) + "Enable",
        true
    ));

    {
        juce::NormalisableRange<float> range (0.25f, 4.0f);
        range.setSkewForCentre (1.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { timeRatioId, shredspace::paramids::kStateSchemaVersion },
            juce::String (voicePrefix) + "Time Ratio",
            range,
            isVoiceB ? 1.0f : 0.5f
        ));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { fineOffsetId, shredspace::paramids::kStateSchemaVersion },
        juce::String (voicePrefix) + "Fine Offset",
        juce::NormalisableRange<float> (-50.0f, 50.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel (" ms")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { levelId, shredspace::paramids::kStateSchemaVersion },
        juce::String (voicePrefix) + "Level",
        juce::NormalisableRange<float> (-60.0f, 0.0f),
        isVoiceB ? -10.0f : -12.0f,
        juce::AudioParameterFloatAttributes().withLabel (" dB")
    ));

    {
        juce::NormalisableRange<float> range (0.25f, 4.0f);
        range.setSkewForCentre (1.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { tailScaleId, shredspace::paramids::kStateSchemaVersion },
            juce::String (voicePrefix) + "Tail Scale",
            range,
            isVoiceB ? 1.2f : 0.65f
        ));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { adaptiveRoleId, shredspace::paramids::kStateSchemaVersion },
        juce::String (voicePrefix) + "Adaptive Role",
        juce::NormalisableRange<float> (0.0f, 100.0f),
        isVoiceB ? 85.0f : 40.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { panId, shredspace::paramids::kStateSchemaVersion },
        juce::String (voicePrefix) + "Pan",
        juce::NormalisableRange<float> (-100.0f, 100.0f),
        isVoiceB ? 20.0f : -20.0f
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { widthId, shredspace::paramids::kStateSchemaVersion },
        juce::String (voicePrefix) + "Width",
        juce::NormalisableRange<float> (0.0f, 200.0f),
        isVoiceB ? 130.0f : 60.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { stereoOffsetId, shredspace::paramids::kStateSchemaVersion },
        juce::String (voicePrefix) + "Stereo Offset",
        juce::NormalisableRange<float> (0.0f, 30.0f),
        isVoiceB ? 11.0f : 3.0f,
        juce::AudioParameterFloatAttributes().withLabel (" ms")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { crossFeedbackId, shredspace::paramids::kStateSchemaVersion },
        juce::String (voicePrefix) + "Cross Feedback",
        juce::NormalisableRange<float> (0.0f, 100.0f),
        isVoiceB ? 35.0f : 15.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { modPhaseId, shredspace::paramids::kStateSchemaVersion },
        juce::String (voicePrefix) + "Mod Phase",
        juce::NormalisableRange<float> (0.0f, 180.0f),
        isVoiceB ? 180.0f : 90.0f,
        juce::AudioParameterFloatAttributes().withLabel (" deg")
    ));

    {
        juce::NormalisableRange<float> range (40.0f, 1500.0f);
        range.setSkewForCentre (250.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { hpfId, shredspace::paramids::kStateSchemaVersion },
            juce::String (voicePrefix) + "HPF",
            range,
            isVoiceB ? 220.0f : 180.0f,
            juce::AudioParameterFloatAttributes().withLabel (" Hz")
        ));
    }

    {
        juce::NormalisableRange<float> range (2000.0f, 20000.0f);
        range.setSkewForCentre (6000.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { lpfId, shredspace::paramids::kStateSchemaVersion },
            juce::String (voicePrefix) + "LPF",
            range,
            isVoiceB ? 5500.0f : 6500.0f,
            juce::AudioParameterFloatAttributes().withLabel (" Hz")
        ));
    }

    {
        juce::NormalisableRange<float> range (1500.0f, 6000.0f);
        range.setSkewForCentre (3000.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { presenceFreqId, shredspace::paramids::kStateSchemaVersion },
            juce::String (voicePrefix) + "Presence Freq",
            range,
            3200.0f,
            juce::AudioParameterFloatAttributes().withLabel (" Hz")
        ));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { presenceGuardId, shredspace::paramids::kStateSchemaVersion },
        juce::String (voicePrefix) + "Presence Guard",
        juce::NormalisableRange<float> (0.0f, 12.0f),
        isVoiceB ? 5.0f : 4.0f,
        juce::AudioParameterFloatAttributes().withLabel (" dB")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { attackSoftenId, shredspace::paramids::kStateSchemaVersion },
        juce::String (voicePrefix) + "Attack Soften",
        juce::NormalisableRange<float> (0.0f, 100.0f),
        isVoiceB ? 40.0f : 30.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    {
        juce::NormalisableRange<float> range (0.05f, 5.0f);
        range.setSkewForCentre (0.5f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { modRateId, shredspace::paramids::kStateSchemaVersion },
            juce::String (voicePrefix) + "Mod Rate",
            range,
            isVoiceB ? 0.23f : 0.35f,
            juce::AudioParameterFloatAttributes().withLabel (" Hz")
        ));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { modDepthId, shredspace::paramids::kStateSchemaVersion },
        juce::String (voicePrefix) + "Mod Depth",
        juce::NormalisableRange<float> (0.0f, 8.0f),
        isVoiceB ? 0.7f : 0.4f,
        juce::AudioParameterFloatAttributes().withLabel (" ms")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driftId, shredspace::paramids::kStateSchemaVersion },
        juce::String (voicePrefix) + "Drift",
        juce::NormalisableRange<float> (0.0f, 100.0f),
        isVoiceB ? 15.0f : 10.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { diffusionId, shredspace::paramids::kStateSchemaVersion },
        juce::String (voicePrefix) + "Diffusion",
        juce::NormalisableRange<float> (0.0f, 40.0f),
        isVoiceB ? 12.0f : 8.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { saturationId, shredspace::paramids::kStateSchemaVersion },
        juce::String (voicePrefix) + "Saturation",
        juce::NormalisableRange<float> (0.0f, 100.0f),
        isVoiceB ? 10.0f : 8.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));
}

void addVoiceParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    addOneVoice (layout, false);
    addOneVoice (layout, true);
}

void addIntelligenceParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    // =====================================================================
    // Detector
    // =====================================================================
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_SENSITIVITY, shredspace::paramids::kStateSchemaVersion },
        "Sensitivity",
        juce::NormalisableRange<float> (0.0f, 100.0f),
        55.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_INPUT_GATE, shredspace::paramids::kStateSchemaVersion },
        "Input Gate",
        juce::NormalisableRange<float> (-80.0f, -20.0f),
        -55.0f,
        juce::AudioParameterFloatAttributes().withLabel (" dB")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_DENSITY_WINDOW, shredspace::paramids::kStateSchemaVersion },
        "Density Window",
        juce::NormalisableRange<float> (200.0f, 1000.0f),
        500.0f,
        juce::AudioParameterFloatAttributes().withLabel (" ms")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_DENSITY_START, shredspace::paramids::kStateSchemaVersion },
        "Density Start",
        juce::NormalisableRange<float> (1.0f, 8.0f),
        3.0f,
        juce::AudioParameterFloatAttributes().withLabel (" ev/s")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_DENSITY_FULL, shredspace::paramids::kStateSchemaVersion },
        "Density Full",
        juce::NormalisableRange<float> (6.0f, 20.0f),
        12.0f,
        juce::AudioParameterFloatAttributes().withLabel (" ev/s")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_SUSTAIN_START, shredspace::paramids::kStateSchemaVersion },
        "Sustain Start",
        juce::NormalisableRange<float> (50.0f, 800.0f),
        180.0f,
        juce::AudioParameterFloatAttributes().withLabel (" ms")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_SUSTAIN_FULL, shredspace::paramids::kStateSchemaVersion },
        "Sustain Full",
        juce::NormalisableRange<float> (150.0f, 2000.0f),
        650.0f,
        juce::AudioParameterFloatAttributes().withLabel (" ms")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_PHRASE_GAP, shredspace::paramids::kStateSchemaVersion },
        "Phrase Gap",
        juce::NormalisableRange<float> (20.0f, 500.0f),
        120.0f,
        juce::AudioParameterFloatAttributes().withLabel (" ms")
    ));

    // =====================================================================
    // Contraction
    // =====================================================================
    {
        juce::NormalisableRange<float> range (1.0f, 100.0f);
        range.setSkewForCentre (15.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { shredspace::paramids::INTEL_CONTRACTION_ATTACK, shredspace::paramids::kStateSchemaVersion },
            "Contraction Attack",
            range,
            8.0f,
            juce::AudioParameterFloatAttributes().withLabel (" ms")
        ));
    }

    {
        juce::NormalisableRange<float> range (20.0f, 1000.0f);
        range.setSkewForCentre (250.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { shredspace::paramids::INTEL_CONTRACTION_RELEASE, shredspace::paramids::kStateSchemaVersion },
            "Contraction Release",
            range,
            220.0f,
            juce::AudioParameterFloatAttributes().withLabel (" ms")
        ));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_WET_REDUCTION, shredspace::paramids::kStateSchemaVersion },
        "Wet Reduction",
        juce::NormalisableRange<float> (0.0f, 18.0f),
        8.0f,
        juce::AudioParameterFloatAttributes().withLabel (" dB")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_TAIL_REDUCTION, shredspace::paramids::kStateSchemaVersion },
        "Tail Reduction",
        juce::NormalisableRange<float> (0.0f, 2.0f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel (" oct")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_TONE_DARKENING, shredspace::paramids::kStateSchemaVersion },
        "Tone Darkening",
        juce::NormalisableRange<float> (0.0f, 2.0f),
        0.75f,
        juce::AudioParameterFloatAttributes().withLabel (" oct")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_WIDTH_REDUCTION, shredspace::paramids::kStateSchemaVersion },
        "Width Reduction",
        juce::NormalisableRange<float> (0.0f, 100.0f),
        35.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_ATTACK_SOFTEN_INCREASE, shredspace::paramids::kStateSchemaVersion },
        "Attack Soften Increase",
        juce::NormalisableRange<float> (0.0f, 100.0f),
        45.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    // =====================================================================
    // Bloom
    // =====================================================================
    {
        juce::NormalisableRange<float> range (20.0f, 1500.0f);
        range.setSkewForCentre (300.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { shredspace::paramids::INTEL_BLOOM_RISE, shredspace::paramids::kStateSchemaVersion },
            "Bloom Rise",
            range,
            220.0f,
            juce::AudioParameterFloatAttributes().withLabel (" ms")
        ));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_BLOOM_HOLD, shredspace::paramids::kStateSchemaVersion },
        "Bloom Hold",
        juce::NormalisableRange<float> (0.0f, 2000.0f),
        350.0f,
        juce::AudioParameterFloatAttributes().withLabel (" ms")
    ));

    {
        juce::NormalisableRange<float> range (50.0f, 3000.0f);
        range.setSkewForCentre (700.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { shredspace::paramids::INTEL_BLOOM_RELEASE, shredspace::paramids::kStateSchemaVersion },
            "Bloom Release",
            range,
            800.0f,
            juce::AudioParameterFloatAttributes().withLabel (" ms")
        ));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_WET_LIFT, shredspace::paramids::kStateSchemaVersion },
        "Wet Lift",
        juce::NormalisableRange<float> (0.0f, 12.0f),
        4.0f,
        juce::AudioParameterFloatAttributes().withLabel (" dB")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_TAIL_EXPANSION, shredspace::paramids::kStateSchemaVersion },
        "Tail Expansion",
        juce::NormalisableRange<float> (0.0f, 2.0f),
        0.75f,
        juce::AudioParameterFloatAttributes().withLabel (" oct")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_TONE_OPENING, shredspace::paramids::kStateSchemaVersion },
        "Tone Opening",
        juce::NormalisableRange<float> (0.0f, 1.0f),
        0.35f,
        juce::AudioParameterFloatAttributes().withLabel (" oct")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_WIDTH_EXPANSION, shredspace::paramids::kStateSchemaVersion },
        "Width Expansion",
        juce::NormalisableRange<float> (0.0f, 100.0f),
        30.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shredspace::paramids::INTEL_DIFFUSION_INCREASE, shredspace::paramids::kStateSchemaVersion },
        "Diffusion Increase",
        juce::NormalisableRange<float> (0.0f, 30.0f),
        12.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    // =====================================================================
    // Shared
    // =====================================================================
    {
        juce::NormalisableRange<float> range (1000.0f, 6000.0f);
        range.setSkewForCentre (2500.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { shredspace::paramids::INTEL_TRANSIENT_CROSSOVER, shredspace::paramids::kStateSchemaVersion },
            "Transient Crossover",
            range,
            2500.0f,
            juce::AudioParameterFloatAttributes().withLabel (" Hz")
        ));
    }
}

} // namespace shredspace
