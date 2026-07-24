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

void addVoiceParameters (juce::AudioProcessorValueTreeState::ParameterLayout& /*layout*/)
{
    // Populated by task b.3
}

void addIntelligenceParameters (juce::AudioProcessorValueTreeState::ParameterLayout& /*layout*/)
{
    // Populated by task b.4
}

} // namespace shredspace
