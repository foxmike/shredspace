/*
  ==============================================================================
  State round-trip fidelity tests for ShredSpace. Uses ONLY juce::UnitTest;
  no third-party test frameworks (no Catch2, no GoogleTest) are involved, and
  no new dependency of any kind is introduced.

  Proves exact round-trip fidelity of the processor's persisted state:
    1. A non-default value is set on at least one parameter in every APVTS
       parameter group defined in Source/Parameters/ParameterLayout.cpp.
    2. A non-default profile is recalled and a non-default scene is selected
       through the processor's message-thread recall path only (ProfileManager
       ::recallProfile asserts it is on the message thread; scene selection is
       performed via setValueNotifyingHost). No audio-thread code is invoked and
       processBlock is never called.
    3. State is serialized with getStateInformation into a juce::MemoryBlock.
    4. A FRESH processor instance is constructed and restored from that block
       with setStateInformation.
    5. Exact equality is asserted between source and restored processor for:
       every set parameter value (exact for discrete/choice/bool, tolerance
       <= 1.0e-6 for continuous floats), the active profile identity, the
       active scene identity, and the persisted schema version.

  The recalled profile has no persisted name/id of its own: a profile is
  reflected in persisted state entirely through the APVTS base parameter values
  that ProfileManager::recallProfile writes via setValueNotifyingHost. Exact
  round-trip of the full APVTS state therefore IS the proof that the active
  profile's parameter state was preserved.
  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <map>
#include <vector>

#include "../Source/PluginProcessor.h"
#include "../Source/Parameters/ParameterIDs.h"
#include "../Source/Parameters/ParameterLayout.h"
#include "../Source/Profiles/ProfileManager.h"

// ProfileManager.cpp (and ProfileSchema.cpp, ProfileSnapshot.cpp,
// SceneManager.cpp, MacroEngine.cpp) are compiled into the ShredSpaceTests
// binary via tests/ProfileMacroSceneTests.cpp and tests/ParameterTests.cpp, so
// only the headers are included here to avoid duplicate definitions.

namespace
{

// =============================================================================
// Helpers
// =============================================================================

// Repository root derived from the compile-time path of this source file so
// the test does not depend on the process working directory.
juce::File getRepositoryRoot()
{
    const juce::File testFile { juce::String (__FILE__) };
    // .../tests/StateRoundTripTests.cpp -> .../
    return testFile.getParentDirectory().getParentDirectory();
}

juce::File getProfilesDir()
{
    return getRepositoryRoot().getChildFile ("Resources").getChildFile ("Profiles");
}

// Reads the raw APVTS value for a parameter id.
float rawValue (const juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    auto* atomic = apvts.getRawParameterValue (id);
    jassert (atomic != nullptr);
    return atomic->load();
}

// True for discrete parameters (choice / bool), which must compare exactly.
bool isDiscrete (const juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    auto* param = apvts.getParameter (id);
    if (param == nullptr)
        return false;

    return dynamic_cast<const juce::AudioParameterChoice*> (param) != nullptr
        || dynamic_cast<const juce::AudioParameterBool*> (param) != nullptr;
}

// Sets a parameter to a non-default value through the message-thread path
// (setValueNotifyingHost). `value` is expressed in the parameter's natural
// units (float value, choice index, or 0/1 for bool); the NormalisableRange
// converts it to normalized form.
void setParameterValue (juce::AudioProcessorValueTreeState& apvts,
                        const juce::String& id, float value)
{
    auto* param = apvts.getParameter (id);
    jassert (param != nullptr);
    const auto normalized = param->getNormalisableRange().convertTo0to1 (value);
    param->setValueNotifyingHost (normalized);
}

// Collects every parameter id in the frozen layout, in registration order.
juce::StringArray collectAllParameterIDs (const juce::AudioProcessorValueTreeState& apvts)
{
    juce::StringArray ids;
    for (auto* param : apvts.processor.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            ids.add (ranged->getParameterID());
    }
    return ids;
}

// =============================================================================
// Test class
// =============================================================================

class StateRoundTripTests final : public juce::UnitTest
{
public:
    StateRoundTripTests()
        : juce::UnitTest ("StateRoundTrip", "ShredSpace/State")
    {
    }

    void runTest() override
    {
        // ---- Shared setup ------------------------------------------------
        // Source processor: recall a non-default profile, then set a
        // non-default value on one parameter in every APVTS group, then select
        // a non-default scene. All through message-thread APIs only.

        ShredSpaceAudioProcessor sourceProcessor;
        auto& sourceApvts = sourceProcessor.getValueTreeState();

        // Recall a non-default profile through the message-thread recall path.
        shredspace::ProfileManager profileManager;
        profileManager.loadUserProfiles (getProfilesDir());
        const auto profiles = profileManager.getProfiles();
        expect (profiles.size() >= 2,
                "Need at least two bundled profiles to choose a non-default one");

        const bool hasNonDefaultProfile = profiles.size() >= 2;
        juce::String recalledProfileName;
        bool recallOk = false;

        if (hasNonDefaultProfile)
        {
            // Choose a profile that is not the first one (the first is the
            // engineering default); the last is safely non-default.
            recalledProfileName = profiles.back().profile_name;
            recallOk = profileManager.recallProfile (recalledProfileName, sourceApvts);
        }

        // One non-default value per APVTS group, strictly inside the declared
        // range and different from the layout default.
        struct GroupSetting { const char* group; const char* paramId; float value; };

        const GroupSetting settings[] = {
            { "Global",       shredspace::paramids::GLOBAL_INPUT_GAIN, 6.0f   }, // default 0.0,  [-24, 24]
            { "Performance", shredspace::paramids::PERF_MACRO_TIME,    700.0f }, // default 380,  [20, 2000]
            { "Timing",       shredspace::paramids::TIME_MODE,          1.0f   }, // choice index 1 (Host Sync), default 0
            { "Stereo",       shredspace::paramids::STEREO_LF_MONO,     0.0f   }, // bool false, default true
            { "Scene",        shredspace::paramids::SCENE_SELECT,       2.0f   }, // choice index 2, default 0 (also the active scene)
            { "Voice",        shredspace::paramids::VOICE_A_PAN,       35.0f   }, // default -20, [-100, 100]
            { "Intelligence", shredspace::paramids::INTEL_SENSITIVITY, 80.0f   }, // default 55,  [0, 100]
        };

        for (const auto& s : settings)
            setParameterValue (sourceApvts, s.paramId, s.value);

        // Capture the source raw values for every parameter (for full-state
        // equality and for the per-group assertions).
        const auto allIds = collectAllParameterIDs (sourceApvts);
        std::map<juce::String, float> sourceRaw;
        for (const auto& id : allIds)
            sourceRaw[id] = rawValue (sourceApvts, id);

        const int activeSceneIndex = 2;

        // ---- Serialize ---------------------------------------------------
        juce::MemoryBlock block;
        sourceProcessor.getStateInformation (block);
        expect (block.getSize() > 0, "getStateInformation produced an empty block");

        // Parse the serialized state once for the schema-version assertion.
        juce::MemoryInputStream schemaStream (block, false);
        auto serializedState = juce::ValueTree::readFromStream (schemaStream);

        // ---- Restore into a fresh processor ------------------------------
        ShredSpaceAudioProcessor restoredProcessor;
        restoredProcessor.setStateInformation (block.getData(), static_cast<int> (block.getSize()));

        auto& restoredApvts = restoredProcessor.getValueTreeState();

        // =================================================================
        // Section 1: parameter round-trip
        // =================================================================
        beginTest ("Parameter round-trip");

        for (const auto& s : settings)
        {
            const float a = rawValue (sourceApvts, s.paramId);
            const float b = rawValue (restoredApvts, s.paramId);

            if (isDiscrete (sourceApvts, s.paramId))
            {
                expectEquals (b, a,
                              juce::String (s.group) + " group: discrete parameter "
                              + s.paramId + " not restored exactly (source=" + juce::String (a)
                              + " restored=" + juce::String (b) + ")");
            }
            else
            {
                expect (std::abs (b - a) <= 1.0e-6f,
                        juce::String (s.group) + " group: continuous parameter "
                        + s.paramId + " not restored within 1e-6 (source=" + juce::String (a)
                        + " restored=" + juce::String (b)
                        + " delta=" + juce::String (std::abs (b - a)) + ")");
            }
        }

        // =================================================================
        // Section 2: profile / scene round-trip
        // =================================================================
        beginTest ("Profile/scene round-trip");

        // The recalled profile has no persisted id; its state lives entirely
        // in the APVTS base values. Exact full-state equality between source
        // and restored therefore proves the active profile state round-tripped.
        expect (recallOk, "Recall of non-default profile should succeed");
        expect (recalledProfileName.isNotEmpty(),
                "A non-default profile name should have been selected");

        int fullStateMismatches = 0;
        for (const auto& id : allIds)
        {
            const float a = sourceRaw.at (id);
            const float b = rawValue (restoredApvts, id);
            const bool exact = isDiscrete (sourceApvts, id);

            if ((exact && a != b) || (! exact && std::abs (b - a) > 1.0e-6f))
                ++fullStateMismatches;
        }
        expect (fullStateMismatches == 0,
                "Active profile identity not preserved: " + juce::String (fullStateMismatches)
                + " parameter(s) differ between source and restored full APVTS state");

        // Active scene identity: SCENE_SELECT must round-trip exactly.
        const int restoredScene = static_cast<int> (rawValue (restoredApvts, shredspace::paramids::SCENE_SELECT));
        expectEquals (restoredScene, activeSceneIndex,
                      "Active scene identity not preserved: source=" + juce::String (activeSceneIndex)
                      + " restored=" + juce::String (restoredScene));

        // =================================================================
        // Section 3: schema-version round-trip
        // =================================================================
        beginTest ("Schema-version round-trip");

        expect (serializedState.isValid(), "Serialized state ValueTree is invalid");
        expect (serializedState.hasType (juce::Identifier (shredspace::paramids::kApvtsType)),
                "Serialized state has wrong root type");

        expect (serializedState.hasProperty (juce::Identifier (shredspace::paramids::kSchemaVersionProperty)),
                "Serialized state is missing the schemaVersion property");

        const auto schemaVar = serializedState.getProperty (
            juce::Identifier (shredspace::paramids::kSchemaVersionProperty));

        expect (schemaVar.isInt(), "Persisted schema version is not an int");
        expectEquals (static_cast<int> (schemaVar), shredspace::paramids::kStateSchemaVersion,
                      "Persisted schema version does not match kStateSchemaVersion");
    }
};

// Static instance registers the test with the juce::UnitTestRunner, mirroring
// the registration idiom used by every other test file in this directory.
static StateRoundTripTests sStateRoundTripTestsInstance;

} // namespace
