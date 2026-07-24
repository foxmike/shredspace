/*
  ==============================================================================
  Parameter unit tests for ShredSpace. Uses ONLY juce::UnitTest; no third-party
  test frameworks are involved.

  Coverage:
    a. EXISTENCE       - every declared parameter ID resolves to a parameter.
    b. RANGES/DEFAULTS - representative ranges and defaults across all groups.
    c. INTERNAL ABSENCE - no layout parameter ID leaks forbidden internal state.
    d. STATE ROUND-TRIP  - getStateInformation/setStateInformation round-trip
                          equality, plus corrupt/empty block safety.
  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <map>
#include <vector>

#include "../Source/Parameters/ParameterIDs.h"
#include "../Source/Parameters/ParameterLayout.h"
#include "../Source/PluginProcessor.h"

namespace
{

constexpr float kTolerance = 1.0e-3f;

// Every parameter ID declared in Source/Parameters/ParameterIDs.h, enumerated
// explicitly per group so none can be silently skipped.
const char* const kAllParameterIds[] = {
    // GLOBAL
    shredspace::paramids::GLOBAL_INPUT_GAIN,
    shredspace::paramids::GLOBAL_OUTPUT_GAIN,
    shredspace::paramids::GLOBAL_MIX,
    shredspace::paramids::GLOBAL_BYPASS,
    shredspace::paramids::GLOBAL_BYPASS_MODE,
    shredspace::paramids::GLOBAL_MIX_MODE,
    shredspace::paramids::GLOBAL_SPILLOVER,
    shredspace::paramids::GLOBAL_SAFETY_LIMITER,
    // PERF
    shredspace::paramids::PERF_MACRO_TIME,
    shredspace::paramids::PERF_MACRO_SPACE,
    shredspace::paramids::PERF_MACRO_DEFINITION,
    shredspace::paramids::PERF_MACRO_BLOOM,
    shredspace::paramids::PERF_MACRO_TAIL,
    shredspace::paramids::PERF_MACRO_FOCUS,
    shredspace::paramids::PERF_MACRO_WIDTH,
    shredspace::paramids::PERF_MACRO_CHARACTER,
    shredspace::paramids::PERF_TAP,
    shredspace::paramids::PERF_HOLD,
    shredspace::paramids::PERF_HOLD_MODE,
    shredspace::paramids::PERF_ADAPTIVE_MODE,
    shredspace::paramids::PERF_ROUTING,
    shredspace::paramids::PERF_ROUTING_CASCADE,
    // TIME
    shredspace::paramids::TIME_MODE,
    shredspace::paramids::TIME_SYNC_DIVISION,
    shredspace::paramids::TIME_GLIDE,
    // STEREO
    shredspace::paramids::STEREO_LF_MONO,
    shredspace::paramids::STEREO_LF_MONO_FREQ,
    // SCENE
    shredspace::paramids::SCENE_SELECT,
    shredspace::paramids::SCENE_MORPH,
    // VOICE_A
    shredspace::paramids::VOICE_A_ENABLE,
    shredspace::paramids::VOICE_A_TIME_RATIO,
    shredspace::paramids::VOICE_A_FINE_OFFSET,
    shredspace::paramids::VOICE_A_LEVEL,
    shredspace::paramids::VOICE_A_TAIL_SCALE,
    shredspace::paramids::VOICE_A_ADAPTIVE_ROLE,
    shredspace::paramids::VOICE_A_PAN,
    shredspace::paramids::VOICE_A_WIDTH,
    shredspace::paramids::VOICE_A_STEREO_OFFSET,
    shredspace::paramids::VOICE_A_CROSS_FEEDBACK,
    shredspace::paramids::VOICE_A_MOD_PHASE,
    shredspace::paramids::VOICE_A_HPF,
    shredspace::paramids::VOICE_A_LPF,
    shredspace::paramids::VOICE_A_PRESENCE_FREQ,
    shredspace::paramids::VOICE_A_PRESENCE_GUARD,
    shredspace::paramids::VOICE_A_ATTACK_SOFTEN,
    shredspace::paramids::VOICE_A_MOD_RATE,
    shredspace::paramids::VOICE_A_MOD_DEPTH,
    shredspace::paramids::VOICE_A_DRIFT,
    shredspace::paramids::VOICE_A_DIFFUSION,
    shredspace::paramids::VOICE_A_SATURATION,
    // VOICE_B
    shredspace::paramids::VOICE_B_ENABLE,
    shredspace::paramids::VOICE_B_TIME_RATIO,
    shredspace::paramids::VOICE_B_FINE_OFFSET,
    shredspace::paramids::VOICE_B_LEVEL,
    shredspace::paramids::VOICE_B_TAIL_SCALE,
    shredspace::paramids::VOICE_B_ADAPTIVE_ROLE,
    shredspace::paramids::VOICE_B_PAN,
    shredspace::paramids::VOICE_B_WIDTH,
    shredspace::paramids::VOICE_B_STEREO_OFFSET,
    shredspace::paramids::VOICE_B_CROSS_FEEDBACK,
    shredspace::paramids::VOICE_B_MOD_PHASE,
    shredspace::paramids::VOICE_B_HPF,
    shredspace::paramids::VOICE_B_LPF,
    shredspace::paramids::VOICE_B_PRESENCE_FREQ,
    shredspace::paramids::VOICE_B_PRESENCE_GUARD,
    shredspace::paramids::VOICE_B_ATTACK_SOFTEN,
    shredspace::paramids::VOICE_B_MOD_RATE,
    shredspace::paramids::VOICE_B_MOD_DEPTH,
    shredspace::paramids::VOICE_B_DRIFT,
    shredspace::paramids::VOICE_B_DIFFUSION,
    shredspace::paramids::VOICE_B_SATURATION,
    // INTEL
    shredspace::paramids::INTEL_SENSITIVITY,
    shredspace::paramids::INTEL_INPUT_GATE,
    shredspace::paramids::INTEL_DENSITY_WINDOW,
    shredspace::paramids::INTEL_DENSITY_START,
    shredspace::paramids::INTEL_DENSITY_FULL,
    shredspace::paramids::INTEL_SUSTAIN_START,
    shredspace::paramids::INTEL_SUSTAIN_FULL,
    shredspace::paramids::INTEL_PHRASE_GAP,
    shredspace::paramids::INTEL_CONTRACTION_ATTACK,
    shredspace::paramids::INTEL_CONTRACTION_RELEASE,
    shredspace::paramids::INTEL_WET_REDUCTION,
    shredspace::paramids::INTEL_TAIL_REDUCTION,
    shredspace::paramids::INTEL_TONE_DARKENING,
    shredspace::paramids::INTEL_WIDTH_REDUCTION,
    shredspace::paramids::INTEL_ATTACK_SOFTEN_INCREASE,
    shredspace::paramids::INTEL_BLOOM_RISE,
    shredspace::paramids::INTEL_BLOOM_HOLD,
    shredspace::paramids::INTEL_BLOOM_RELEASE,
    shredspace::paramids::INTEL_WET_LIFT,
    shredspace::paramids::INTEL_TAIL_EXPANSION,
    shredspace::paramids::INTEL_TONE_OPENING,
    shredspace::paramids::INTEL_WIDTH_EXPANSION,
    shredspace::paramids::INTEL_DIFFUSION_INCREASE,
    shredspace::paramids::INTEL_TRANSIENT_CROSSOVER
};

// Substrings that must NEVER appear in any parameter ID: they denote internal
// runtime state that is forbidden from the parameter layout.
const char* const kForbiddenSubstrings[] = {
    "RUNNING_MEAN",
    "DEVIATION",
    "FFT",
    "ONSET",
    "HISTORY",
    "ACTIVITY",
    "DENSITY_VALUE",
    "SUSTAIN_VALUE",
    "PHRASE_GAP_VALUE",
    "DESTINATION_TABLE",
    "SCENE_NAME",
    "PROFILE_NAME",
    "PROFILE_DESC",
    "PROFILE_TAG",
    "MIGRATION",
    "TELEMETRY",
    "INTERVENTION",
    "BUFFER",
    "RESAMPLER"
};

juce::String toUpper (juce::String s)
{
    return s.toUpperCase();
}

float actualDefault (juce::AudioProcessorValueTreeState& apvts, const char* id)
{
    auto* param = apvts.getParameter (id);
    const auto range = apvts.getParameterRange (id);
    return range.convertFrom0to1 (param->getDefaultValue());
}

int choiceDefaultIndex (juce::AudioProcessorValueTreeState& apvts, const char* id)
{
    auto* param = apvts.getParameter (id);
    const auto range = apvts.getParameterRange (id);
    return juce::roundToInt (range.convertFrom0to1 (param->getDefaultValue()));
}

class ShredSpaceParameterTests final : public juce::UnitTest
{
public:
    ShredSpaceParameterTests()
        : juce::UnitTest ("ShredSpace Parameters", "ShredSpace/Parameters")
    {
    }

    void runTest() override
    {
        ShredSpaceAudioProcessor processor;
        auto& apvts = processor.getValueTreeState();

        beginTest ("Parameter ID existence");
        {
            for (const auto* id : kAllParameterIds)
            {
                auto* param = apvts.getParameter (id);
                expect (param != nullptr,
                        "Missing parameter for ID: " + juce::String (id));
            }
        }

        beginTest ("Parameter ranges and defaults");
        {
            // GLOBAL
            expectRangeAndDefault (apvts, shredspace::paramids::GLOBAL_INPUT_GAIN, -24.0f, 24.0f, 0.0f);
            expectRangeAndDefault (apvts, shredspace::paramids::GLOBAL_MIX,      0.0f,  100.0f, 22.0f);
            expectChoiceDefaultIndex (apvts, shredspace::paramids::GLOBAL_BYPASS_MODE, 1);

            // PERF
            expectRangeAndDefault (apvts, shredspace::paramids::PERF_MACRO_TIME, 20.0f, 2000.0f, 380.0f);
            expectRangeAndDefault (apvts, shredspace::paramids::PERF_MACRO_TAIL, 0.15f, 12.0f,  2.8f);
            expectChoiceDefaultIndex (apvts, shredspace::paramids::PERF_ADAPTIVE_MODE, 2);

            // TIME
            expectChoiceDefaultIndex (apvts, shredspace::paramids::TIME_SYNC_DIVISION, 6);
            expectChoiceNumEntries (apvts, shredspace::paramids::TIME_SYNC_DIVISION, 15);

            // STEREO
            expectRangeAndDefault (apvts, shredspace::paramids::STEREO_LF_MONO_FREQ, 60.0f, 500.0f, 180.0f);

            // SCENE
            expectRangeAndDefault (apvts, shredspace::paramids::SCENE_MORPH, 0.0f, 100.0f, 0.0f);

            // VOICE_A
            expectDefault (apvts, shredspace::paramids::VOICE_A_LEVEL,      -12.0f);
            expectDefault (apvts, shredspace::paramids::VOICE_A_TIME_RATIO,   0.5f);
            expectDefault (apvts, shredspace::paramids::VOICE_A_WIDTH,       60.0f);
            expectDefault (apvts, shredspace::paramids::VOICE_A_LPF,      6500.0f);

            // VOICE_B
            expectDefault (apvts, shredspace::paramids::VOICE_B_LEVEL,      -10.0f);
            expectDefault (apvts, shredspace::paramids::VOICE_B_TIME_RATIO,   1.0f);
            expectDefault (apvts, shredspace::paramids::VOICE_B_WIDTH,      130.0f);
            expectDefault (apvts, shredspace::paramids::VOICE_B_LPF,      5500.0f);

            // INTEL
            expectRangeAndDefault (apvts, shredspace::paramids::INTEL_SENSITIVITY,         0.0f,  100.0f, 55.0f);
            expectRangeAndDefault (apvts, shredspace::paramids::INTEL_INPUT_GATE,        -80.0f, -20.0f, -55.0f);
            expectRangeAndDefault (apvts, shredspace::paramids::INTEL_WET_REDUCTION,        0.0f,  18.0f,   8.0f);
            expectRangeAndDefault (apvts, shredspace::paramids::INTEL_BLOOM_RELEASE,      50.0f, 3000.0f, 800.0f);
            expectRangeAndDefault (apvts, shredspace::paramids::INTEL_TRANSIENT_CROSSOVER, 1000.0f, 6000.0f, 2500.0f);
        }

        beginTest ("Internal state name absence");
        {
            const auto state = apvts.state;
            std::vector<juce::String> layoutIds;
            layoutIds.reserve (static_cast<size_t> (state.getNumChildren()));

            for (int i = 0; i < state.getNumChildren(); ++i)
            {
                const auto child = state.getChild (i);
                layoutIds.push_back (child.getProperty ("id").toString());
            }

            expect (layoutIds.size() == std::size (kAllParameterIds),
                    "Layout parameter count mismatch: got " + juce::String ((int) layoutIds.size())
                    + " expected " + juce::String ((int) std::size (kAllParameterIds)));

            for (const auto& id : layoutIds)
            {
                const auto upper = toUpper (id);

                for (const auto* forbidden : kForbiddenSubstrings)
                {
                    expect (! upper.contains (juce::String (forbidden).toUpperCase()),
                            "Parameter ID \"" + id + "\" contains forbidden substring \""
                            + juce::String (forbidden) + "\"");
                }
            }
        }

        beginTest ("State round-trip");
        {
            ShredSpaceAudioProcessor sourceProcessor;
            auto& sourceApvts = sourceProcessor.getValueTreeState();

            // Change parameters across all eight groups away from their defaults.
            setParamFromValue (sourceApvts, shredspace::paramids::GLOBAL_INPUT_GAIN,      12.0f);
            setParamFromValue (sourceApvts, shredspace::paramids::PERF_MACRO_TIME,       1000.0f);
            setParamFromValue (sourceApvts, shredspace::paramids::TIME_GLIDE,             80.0f);
            setParamFromValue (sourceApvts, shredspace::paramids::STEREO_LF_MONO_FREQ,  300.0f);
            setParamFromValue (sourceApvts, shredspace::paramids::SCENE_MORPH,            70.0f);
            setParamFromValue (sourceApvts, shredspace::paramids::VOICE_A_LEVEL,        -30.0f);
            setParamFromValue (sourceApvts, shredspace::paramids::VOICE_B_WIDTH,        180.0f);
            setParamFromValue (sourceApvts, shredspace::paramids::INTEL_SENSITIVITY,      80.0f);
            setChoiceParam (sourceApvts, shredspace::paramids::TIME_SYNC_DIVISION, 10);

            juce::MemoryBlock stateBlock;
            sourceProcessor.getStateInformation (stateBlock);
            expect (stateBlock.getSize() > 0, "getStateInformation produced an empty block");

            ShredSpaceAudioProcessor restoredProcessor;
            restoredProcessor.setStateInformation (stateBlock.getData(),
                                                  static_cast<int> (stateBlock.getSize()));

            auto& restoredApvts = restoredProcessor.getValueTreeState();

            for (const auto* id : kAllParameterIds)
            {
                auto* sourceParam  = sourceApvts.getParameter (id);
                auto* restoredParam = restoredApvts.getParameter (id);
                expect (sourceParam != nullptr && restoredParam != nullptr,
                        "Missing parameter during round-trip for ID: " + juce::String (id));

                if (sourceParam == nullptr || restoredParam == nullptr)
                    continue;

                const auto sourceNorm   = sourceParam->getValue();
                const auto restoredNorm = restoredParam->getValue();
                expectWithin (restoredNorm, sourceNorm, kTolerance,
                              "Round-trip mismatch for ID " + juce::String (id)
                              + ": source=" + juce::String (sourceNorm)
                              + " restored=" + juce::String (restoredNorm));
            }

            // Corrupt / empty state blocks must leave the restored values intact.
            std::map<juce::String, float> baseline;
            for (const auto* id : kAllParameterIds)
            {
                auto* param = restoredApvts.getParameter (id);
                if (param != nullptr)
                    baseline[id] = param->getValue();
            }

            // Empty block.
            restoredProcessor.setStateInformation (nullptr, 0);

            // Corrupt block.
            const unsigned char garbage[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
            restoredProcessor.setStateInformation (garbage, (int) sizeof (garbage));

            for (const auto* id : kAllParameterIds)
            {
                auto* param = restoredApvts.getParameter (id);
                if (param == nullptr)
                    continue;

                const auto current = param->getValue();
                expectWithin (current, baseline[id], kTolerance,
                              "Corrupt/empty state altered value for ID " + juce::String (id)
                              + ": expected=" + juce::String (baseline[id])
                              + " got=" + juce::String (current));
            }
        }
    }

private:
    void expectWithin (float actual, float expected, float tolerance, const juce::String& message)
    {
        expect (std::abs (actual - expected) <= tolerance, message);
    }

    void expectRangeAndDefault (juce::AudioProcessorValueTreeState& apvts,
                                const char* id, float rangeStart, float rangeEnd, float def)
    {
        auto* param = apvts.getParameter (id);
        expect (param != nullptr, "Missing parameter for range check: " + juce::String (id));
        if (param == nullptr)
            return;

        const auto range = apvts.getParameterRange (id);
        expectWithin (range.start, rangeStart, kTolerance,
                      juce::String (id) + " range start");
        expectWithin (range.end,   rangeEnd,   kTolerance,
                      juce::String (id) + " range end");
        expectWithin (actualDefault (apvts, id), def, kTolerance,
                      juce::String (id) + " default value");
    }

    void expectDefault (juce::AudioProcessorValueTreeState& apvts, const char* id, float def)
    {
        auto* param = apvts.getParameter (id);
        expect (param != nullptr, "Missing parameter for default check: " + juce::String (id));
        if (param == nullptr)
            return;

        expectWithin (actualDefault (apvts, id), def, kTolerance,
                      juce::String (id) + " default value");
    }

    void expectChoiceDefaultIndex (juce::AudioProcessorValueTreeState& apvts,
                                   const char* id, int expectedIndex)
    {
        auto* param = apvts.getParameter (id);
        expect (param != nullptr, "Missing choice parameter: " + juce::String (id));
        if (param == nullptr)
            return;

        const auto index = choiceDefaultIndex (apvts, id);
        expect (index == expectedIndex,
                juce::String (id) + " choice default index: got " + juce::String (index)
                + " expected " + juce::String (expectedIndex));
    }

    void expectChoiceNumEntries (juce::AudioProcessorValueTreeState& apvts,
                                 const char* id, int expectedCount)
    {
        auto* param = apvts.getParameter (id);
        expect (param != nullptr, "Missing choice parameter: " + juce::String (id));
        if (param == nullptr)
            return;

        auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param);
        expect (choice != nullptr, juce::String (id) + " is not an AudioParameterChoice");
        if (choice == nullptr)
            return;

        expect (choice->choices.size() == expectedCount,
                juce::String (id) + " choice count: got " + juce::String (choice->choices.size())
                + " expected " + juce::String (expectedCount));
    }

    void setParamFromValue (juce::AudioProcessorValueTreeState& apvts,
                            const char* id, float value)
    {
        auto* param = apvts.getParameter (id);
        if (param == nullptr)
        {
            expect (false, "Cannot set missing parameter: " + juce::String (id));
            return;
        }

        const auto range = apvts.getParameterRange (id);
        param->setValueNotifyingHost (range.convertTo0to1 (value));
    }

    void setChoiceParam (juce::AudioProcessorValueTreeState& apvts,
                         const char* id, int index)
    {
        auto* param = apvts.getParameter (id);
        if (param == nullptr)
        {
            expect (false, "Cannot set missing choice parameter: " + juce::String (id));
            return;
        }

        const auto range = apvts.getParameterRange (id);
        param->setValueNotifyingHost (range.convertTo0to1 (static_cast<float> (index)));
    }
};

// Static instance registers the test with juce::UnitTestRunner.
static ShredSpaceParameterTests sShredSpaceParameterTestsInstance;

} // namespace
