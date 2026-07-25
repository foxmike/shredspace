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

#include "../Source/DSP/DelayTimeController.cpp"

namespace
{

class ShredSpaceDelayTimeControllerTests final : public juce::UnitTest
{
public:
    ShredSpaceDelayTimeControllerTests()
        : juce::UnitTest ("DelayTimeController", "ShredSpace DSP")
    {}

    void runTest() override
    {
        runHostTimingPodTest();
        runMillisecondsModeTest();
        runHostSyncModeTest();
        runTapTempoTest();
        runTransitionRequestsTest();
    }

private:
    void runHostTimingPodTest()
    {
        beginTest ("HostTiming POD Layout");

        // Verify verbatim fields exist and compile as expected
        shredspace::HostTiming timing {};
        timing.bpm = 120.0;
        timing.bpmValid = true;
        timing.isPlaying = false;
        timing.samplePosition = 12345;

        expect (std::abs (timing.bpm - 120.0) <= 1e-5);
        expect (timing.bpmValid);
        expect (! timing.isPlaying);
        expect (timing.samplePosition == 12345);
    }

    void runMillisecondsModeTest()
    {
        beginTest ("Milliseconds Mode and Clamping");

        shredspace::DelayTimeController controller;
        controller.prepare (44100.0);

        shredspace::ControllerInput input {};
        input.timeMode = 0; // Milliseconds
        input.macroTimeMs = 500.0;
        input.voiceA.timeRatio = 1.0;
        input.voiceA.fineOffsetMs = 0.0;
        input.voiceB.timeRatio = 0.5;
        input.voiceB.fineOffsetMs = 10.0;

        shredspace::HostTiming timing { 120.0, true, false, 0 };

        // Check resolved values
        auto results = controller.update (input, timing);

        // Voice A: T_voice = 0.5 * 1.0 + 0.0 = 0.5s -> 22050 samples
        expect (std::abs (results[0].targetDelaySamples - 22050.0) <= 1e-2);

        // Voice B: T_voice = 0.5 * 0.5 + 0.010 = 0.26s -> 11466 samples
        expect (std::abs (results[1].targetDelaySamples - 11466.0) <= 1e-2);

        // Check clamping at minimum (8.0 samples)
        input.macroTimeMs = 0.0;
        input.voiceA.timeRatio = 0.0;
        input.voiceA.fineOffsetMs = -100.0; // will result in negative delay before clamp
        results = controller.update (input, timing);
        expect (std::abs (results[0].targetDelaySamples - 8.0) <= 1e-5);

        // Check clamping at maximum (8.25 seconds)
        input.macroTimeMs = 9000.0; // 9 seconds
        input.voiceA.timeRatio = 1.5;
        input.voiceA.fineOffsetMs = 1000.0;
        results = controller.update (input, timing);
        expect (std::abs (results[0].targetDelaySamples - 8.25 * 44100.0) <= 1e-2);
    }

    void runHostSyncModeTest()
    {
        beginTest ("HostSync Mode and BPM Fallback");

        shredspace::DelayTimeController controller;
        controller.prepare (48000.0);

        shredspace::ControllerInput input {};
        input.timeMode = 1; // Host Sync
        input.syncDivision = 6; // "1/4" (1.0 quarter note ratio)
        input.voiceA.timeRatio = 1.0;
        input.voiceA.fineOffsetMs = 0.0;
        input.voiceB.timeRatio = 2.0;
        input.voiceB.fineOffsetMs = 0.0;

        // 1. Initial invalid BPM -> falls back to 120 BPM
        shredspace::HostTiming timing { 0.0, false, false, 0 };
        auto results = controller.update (input, timing);
        // T_quarter at 120 BPM = 0.5s. Sync division ratio for 1/4 = 1.0.
        // Voice A: T_voice = 0.5 * 1.0 * 1.0 = 0.5s -> 24000 samples
        expect (std::abs (results[0].targetDelaySamples - 24000.0) <= 1e-2);
        // Voice B: T_voice = 0.5 * 1.0 * 2.0 = 1.0s -> 48000 samples
        expect (std::abs (results[1].targetDelaySamples - 48000.0) <= 1e-2);

        // 2. Valid BPM updates
        timing.bpm = 100.0; // T_quarter = 0.6s
        timing.bpmValid = true;
        results = controller.update (input, timing);
        // Voice A: 0.6s * 48000 = 28800 samples
        expect (std::abs (results[0].targetDelaySamples - 28800.0) <= 1e-2);

        // 3. Invalid BPM -> retains last valid BPM (100.0)
        timing.bpm = 150.0;
        timing.bpmValid = false;
        results = controller.update (input, timing);
        expect (std::abs (results[0].targetDelaySamples - 28800.0) <= 1e-2);
    }

    void runTapTempoTest()
    {
        beginTest ("Tap Tempo and Median");

        shredspace::DelayTimeController controller;
        controller.prepare (44100.0);

        shredspace::ControllerInput input {};
        input.timeMode = 2; // Tap
        input.voiceA.timeRatio = 1.0;
        input.voiceA.fineOffsetMs = 0.0;

        shredspace::HostTiming timing { 120.0, true, false, 0 };

        // 1. Empty tap history -> fallback to 120 BPM (500 ms interval)
        auto results = controller.update (input, timing);
        expect (std::abs (results[0].targetDelaySamples - 0.5 * 44100.0) <= 1e-2);

        // 2. Record taps
        // Tap 1 at 0.0s
        controller.tap (0.0);
        // Tap 2 at 0.4s (interval 400ms)
        controller.tap (0.4);
        results = controller.update (input, timing);
        // Median of [0.4] is 0.4s
        expect (std::abs (results[0].targetDelaySamples - 0.4 * 44100.0) <= 1e-2);

        // Tap 3 at 0.9s (interval 500ms) -> history: [0.4, 0.5]
        controller.tap (0.9);
        // Median of [0.4, 0.5] is 0.45s
        results = controller.update (input, timing);
        expect (std::abs (results[0].targetDelaySamples - 0.45 * 44100.0) <= 1e-2);

        // Tap 4 at 2.9s (interval 2.0s) -> history: [0.4, 0.5, 2.0]
        controller.tap (2.9);
        // Median of [0.4, 0.5, 2.0] is 0.5s
        results = controller.update (input, timing);
        expect (std::abs (results[0].targetDelaySamples - 0.5 * 44100.0) <= 1e-2);

        // Tap 5 at 3.2s (interval 300ms) -> history: [0.4, 0.5, 2.0, 0.3]
        controller.tap (3.2);
        // Sorted: [0.3, 0.4, 0.5, 2.0]. Median is (0.4 + 0.5) / 2 = 0.45s
        results = controller.update (input, timing);
        expect (std::abs (results[0].targetDelaySamples - 0.45 * 44100.0) <= 1e-2);

        // Tap 6 at 4.0s (interval 800ms) -> history: [0.4, 0.5, 2.0, 0.3, 0.8]
        controller.tap (4.0);
        // Sorted: [0.3, 0.4, 0.5, 0.8, 2.0]. Median is 0.5s
        results = controller.update (input, timing);
        expect (std::abs (results[0].targetDelaySamples - 0.5 * 44100.0) <= 1e-2);

        // Tap 7 at 4.6s (interval 600ms) -> history: oldest (0.4) shifted out.
        // History: [0.5, 2.0, 0.3, 0.8, 0.6]
        // Sorted: [0.3, 0.5, 0.6, 0.8, 2.0]. Median is 0.6s
        controller.tap (4.6);
        results = controller.update (input, timing);
        expect (std::abs (results[0].targetDelaySamples - 0.6 * 44100.0) <= 1e-2);

        // 3. Rejected tap
        // Tap 8 at 10.0s (interval 5.4s) -> rejected! history remains same.
        controller.tap (10.0);
        results = controller.update (input, timing);
        expect (std::abs (results[0].targetDelaySamples - 0.6 * 44100.0) <= 1e-2);
    }

    void runTransitionRequestsTest()
    {
        beginTest ("Transition Requests and Large Changes");

        shredspace::DelayTimeController controller;
        controller.prepare (44100.0);

        shredspace::ControllerInput input {};
        input.timeMode = 0;
        input.macroTimeMs = 500.0;
        input.voiceA.timeRatio = 1.0;
        input.voiceA.fineOffsetMs = 0.0;
        input.voiceB.timeRatio = 1.0;
        input.voiceB.fineOffsetMs = 0.0;

        shredspace::HostTiming timing { 120.0, true, false, 0 };

        // First update must NOT require transitions
        auto results = controller.update (input, timing);
        expect (! results[0].transitionRequired);
        expect (! results[1].transitionRequired);

        // Small change: macroTimeMs 500ms -> 502ms (2ms change -> 0.002s)
        // This is below the 10 ms threshold for large changes, so transitionRequired = true, mode = bend
        input.macroTimeMs = 502.0;
        results = controller.update (input, timing);
        expect (results[0].transitionRequired);
        expect (results[0].transitionMode == shredspace::TransitionMode::bend);

        // Let's run update again with no changes: transitionRequired should be false
        results = controller.update (input, timing);
        expect (! results[0].transitionRequired);

        // Large change: macroTimeMs 502ms -> 600ms (98ms change) -> mode = smooth
        input.macroTimeMs = 600.0;
        results = controller.update (input, timing);
        expect (results[0].transitionRequired);
        expect (results[0].transitionMode == shredspace::TransitionMode::smooth);

        // Host BPM jump (when timing.bpmValid is true) -> triggers large transition request
        // First establish current target in Host Sync mode
        input.timeMode = 1; // triggers mode change -> large change
        results = controller.update (input, timing);
        expect (results[0].transitionRequired);
        expect (results[0].transitionMode == shredspace::TransitionMode::smooth);

        // Let target settle
        results = controller.update (input, timing);
        expect (! results[0].transitionRequired);

        // BPM jump
        timing.bpm = 125.0;
        results = controller.update (input, timing);
        expect (results[0].transitionRequired);
        expect (results[0].transitionMode == shredspace::TransitionMode::smooth);

        // Settle
        results = controller.update (input, timing);

        // Scene change
        input.sceneIndex = 1;
        results = controller.update (input, timing);
        expect (results[0].transitionRequired);
        expect (results[0].transitionMode == shredspace::TransitionMode::smooth);

        // Settle
        results = controller.update (input, timing);

        // Profile change
        input.profileIndex = 1;
        results = controller.update (input, timing);
        expect (results[0].transitionRequired);
        expect (results[0].transitionMode == shredspace::TransitionMode::smooth);
    }
};

static ShredSpaceDelayTimeControllerTests sShredSpaceDelayTimeControllerTestsInstance;

} // namespace

#include "../Source/Profiles/ProfileSchema.h"
#include "../Source/Profiles/ProfileSchema.cpp"

namespace
{

class ProfileSchemaTests final : public juce::UnitTest
{
public:
    ProfileSchemaTests()
        : juce::UnitTest ("ProfileSchema", "ShredSpace Profiles")
    {}

    juce::StringArray getAllowedIDs()
    {
        juce::StringArray ids;
        for (const auto* id : kAllParameterIds)
            ids.add (id);
        return ids;
    }

    void runTest() override
    {
        auto allowedIDs = getAllowedIDs();

        // 1. Test Valid profile JSON
        {
            beginTest ("Valid Profile Parsing");
            juce::var json (new juce::DynamicObject());
            json.getDynamicObject()->setProperty ("schema_version", 1);
            json.getDynamicObject()->setProperty ("product_version", "1.0.0");
            json.getDynamicObject()->setProperty ("profile_name", "Clean Room");
            json.getDynamicObject()->setProperty ("author", "Engineer");
            json.getDynamicObject()->setProperty ("description", "A clean test profile");

            juce::var baseVals (new juce::DynamicObject());
            baseVals.getDynamicObject()->setProperty ("GLOBAL_INPUT_GAIN", 1.5f);
            baseVals.getDynamicObject()->setProperty ("PERF_MACRO_TIME", 120.0f);
            json.getDynamicObject()->setProperty ("base_parameter_values", baseVals);

            juce::var macroMapping (new juce::DynamicObject());
            for (const auto* macroName : shredspace::kMacroNames)
            {
                juce::Array<juce::var> dests;
                if (juce::String (macroName) == "TIME")
                {
                    juce::var dest (new juce::DynamicObject());
                    dest.getDynamicObject()->setProperty ("destination_parameter_id", "VOICE_A_TIME_RATIO");
                    dest.getDynamicObject()->setProperty ("delta_min", 0.1f);
                    dest.getDynamicObject()->setProperty ("delta_max", 0.5f);
                    dest.getDynamicObject()->setProperty ("curve", 1.2f);
                    dests.add (dest);
                }
                macroMapping.getDynamicObject()->setProperty (macroName, dests);
            }
            json.getDynamicObject()->setProperty ("macro_mapping", macroMapping);

            juce::Array<juce::var> scenes;
            for (int i = 0; i < 4; ++i)
            {
                juce::var scene (new juce::DynamicObject());
                for (const auto* macroName : shredspace::kMacroNames)
                {
                    scene.getDynamicObject()->setProperty (macroName, 0.5f);
                }

                juce::var mixTarget (new juce::DynamicObject());
                mixTarget.getDynamicObject()->setProperty ("parameter_id", "GLOBAL_MIX");
                mixTarget.getDynamicObject()->setProperty ("value", 50.0f);
                scene.getDynamicObject()->setProperty ("mix_target", mixTarget);

                juce::var routingTarget (new juce::DynamicObject());
                routingTarget.getDynamicObject()->setProperty ("parameter_id", "PERF_ROUTING");
                routingTarget.getDynamicObject()->setProperty ("value", 1.0f);
                scene.getDynamicObject()->setProperty ("routing_target", routingTarget);

                scene.getDynamicObject()->setProperty ("hold_behavior", "Momentary");
                scenes.add (scene);
            }
            json.getDynamicObject()->setProperty ("scene_values", scenes);

            juce::Array<juce::var> tags;
            tags.add ("clean");
            tags.add ("reverb");
            json.getDynamicObject()->setProperty ("tags", tags);

            auto [doc, result] = shredspace::ProfileSchema::parse (json, allowedIDs);
            expect (result.ok);
            expectEquals (doc.schema_version, 1);
            expectEquals (doc.profile_name, juce::String ("Clean Room"));
            expectEquals (doc.base_parameter_values["GLOBAL_INPUT_GAIN"], 1.5f);
            expectEquals (doc.base_parameter_values["PERF_MACRO_TIME"], 120.0f);
            expectEquals (doc.tags[0], juce::String ("clean"));
            expectEquals ((int) doc.scene_values.size(), 4);
            expect (doc.scene_values[0].hold_behavior == shredspace::HoldBehavior::Momentary);
        }

        // 2. Test Rule 1: Schema Version validation
        {
            beginTest ("Rule 1: Schema Version checks");
            juce::var json (new juce::DynamicObject());
            auto [doc1, result1] = shredspace::ProfileSchema::parse (json, allowedIDs);
            expect (! result1.ok);

            json.getDynamicObject()->setProperty ("schema_version", "one");
            auto [doc2, result2] = shredspace::ProfileSchema::parse (json, allowedIDs);
            expect (! result2.ok);

            json.getDynamicObject()->setProperty ("schema_version", 0);
            auto [doc3, result3] = shredspace::ProfileSchema::parse (json, allowedIDs);
            expect (! result3.ok);

            json.getDynamicObject()->setProperty ("schema_version", 2);
            auto [doc4, result4] = shredspace::ProfileSchema::parse (json, allowedIDs);
            expect (! result4.ok);
        }

        // 3. Test Rule 3 & 5: Migration and clamping
        {
            beginTest ("Rule 3 & 5: Migration of unknown IDs and clamping");
            juce::var json (new juce::DynamicObject());
            json.getDynamicObject()->setProperty ("schema_version", 1);

            juce::var baseVals (new juce::DynamicObject());
            baseVals.getDynamicObject()->setProperty ("UNKNOWN_ID", 10.0f);
            baseVals.getDynamicObject()->setProperty ("GLOBAL_INPUT_GAIN", 30.0f);
            json.getDynamicObject()->setProperty ("base_parameter_values", baseVals);

            juce::var macroMapping (new juce::DynamicObject());
            for (const auto* macroName : shredspace::kMacroNames)
            {
                juce::Array<juce::var> dests;
                if (juce::String (macroName) == "TIME")
                {
                    juce::var dest (new juce::DynamicObject());
                    dest.getDynamicObject()->setProperty ("destination_parameter_id", "UNKNOWN_DEST_ID");
                    dest.getDynamicObject()->setProperty ("delta_min", 0.0f);
                    dest.getDynamicObject()->setProperty ("delta_max", 1.0f);
                    dests.add (dest);
                }
                macroMapping.getDynamicObject()->setProperty (macroName, dests);
            }
            json.getDynamicObject()->setProperty ("macro_mapping", macroMapping);

            juce::Array<juce::var> scenes;
            for (int i = 0; i < 4; ++i)
            {
                juce::var scene (new juce::DynamicObject());
                for (const auto* macroName : shredspace::kMacroNames)
                    scene.getDynamicObject()->setProperty (macroName, 0.5f);

                juce::var mixTarget (new juce::DynamicObject());
                mixTarget.getDynamicObject()->setProperty ("parameter_id", "UNKNOWN_MIX_ID");
                mixTarget.getDynamicObject()->setProperty ("value", 50.0f);
                scene.getDynamicObject()->setProperty ("mix_target", mixTarget);

                juce::var routingTarget (new juce::DynamicObject());
                routingTarget.getDynamicObject()->setProperty ("parameter_id", "PERF_ROUTING");
                routingTarget.getDynamicObject()->setProperty ("value", 1.0f);
                scene.getDynamicObject()->setProperty ("routing_target", routingTarget);

                scene.getDynamicObject()->setProperty ("hold_behavior", "Latch");
                scenes.add (scene);
            }
            json.getDynamicObject()->setProperty ("scene_values", scenes);

            auto [doc, result] = shredspace::ProfileSchema::parse (json, allowedIDs);
            expect (result.ok);
            expect (doc.base_parameter_values.find ("UNKNOWN_ID") == doc.base_parameter_values.end());
            expectEquals (doc.base_parameter_values["GLOBAL_INPUT_GAIN"], 24.0f);
            expect (doc.macro_mapping["TIME"].empty());
            expect (doc.scene_values[0].mix_target.parameter_id.isEmpty());
            expectEquals (doc.scene_values[0].mix_target.value, 0.0f);
            expect (result.migration_notes.size() > 0);
            expect (result.migration_notes.contains ("dropped unknown parameter id: UNKNOWN_ID"));
            expect (result.migration_notes.contains ("dropped unknown parameter id: UNKNOWN_DEST_ID"));
            expect (result.migration_notes.contains ("dropped unknown parameter id: UNKNOWN_MIX_ID"));
        }

        // 4. Test Rule 4: Malformed input fails safely
        {
            beginTest ("Rule 4: Malformed input failures");
            juce::var json (new juce::DynamicObject());
            json.getDynamicObject()->setProperty ("schema_version", 1);

            juce::var macroMapping (new juce::DynamicObject());
            for (const auto* macroName : shredspace::kMacroNames)
            {
                juce::Array<juce::var> dests;
                if (juce::String (macroName) == "TIME")
                {
                    juce::var dest (new juce::DynamicObject());
                    dest.getDynamicObject()->setProperty ("destination_parameter_id", "VOICE_A_TIME_RATIO");
                    dest.getDynamicObject()->setProperty ("delta_min", 0.0f);
                    dest.getDynamicObject()->setProperty ("delta_max", 1.0f);
                    dest.getDynamicObject()->setProperty ("curve", -0.1f);
                    dests.add (dest);
                }
                macroMapping.getDynamicObject()->setProperty (macroName, dests);
            }
            json.getDynamicObject()->setProperty ("macro_mapping", macroMapping);

            auto [doc1, result1] = shredspace::ProfileSchema::parse (json, allowedIDs);
            expect (! result1.ok);

            macroMapping.getDynamicObject()->removeProperty ("TIME");
            {
                juce::Array<juce::var> dests;
                juce::var dest (new juce::DynamicObject());
                dest.getDynamicObject()->setProperty ("destination_parameter_id", "VOICE_A_TIME_RATIO");
                dest.getDynamicObject()->setProperty ("delta_min", std::numeric_limits<double>::infinity());
                dest.getDynamicObject()->setProperty ("delta_max", 1.0f);
                dests.add (dest);
                macroMapping.getDynamicObject()->setProperty ("TIME", dests);
            }
            auto [doc2, result2] = shredspace::ProfileSchema::parse (json, allowedIDs);
            expect (! result2.ok);
        }
    }
};

static ProfileSchemaTests sProfileSchemaTestsInstance;

} // namespace

