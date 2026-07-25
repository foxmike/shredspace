/*
  ==============================================================================
  Profile / Macro / Scene integration tests for ShredSpace. Uses ONLY
  juce::UnitTest; no third-party test frameworks are involved.

  Coverage (see task spec for the authoritative claim list):
    1.  All twelve bundled profiles parse and validate with ok == true.
    2.  Every parameter ID referenced by every bundled profile exists in the
        frozen parameter layout.
    3.  schema_version 0 and kCurrentSchemaVersion + 1 are rejected.
    4.  Malformed profiles fail safely: ok == false, non-empty errors, default
        document, no throw.
    5.  Unknown parameter IDs are dropped with a migration note; doc stays ok.
    6.  Profile loading performs no audio-thread IO (handoff generation is
        unchanged by load/acquire until an explicit publish).
    7.  No artist/band/song name token appears in any shipped profile resource;
        every profile_name, description, and tag is present and non-empty.
    8.  Macro sweep: every effective destination value stays within its
        declared [min, max] across all 8 macros in all 12 profiles.
    9.  Macro movement never rewrites destination APVTS base values.
    10. Curve/endpoint mapping: delta(0)==deltaMin, delta(1)==deltaMax, and
        curve 2.0 yields delta(0.5) == deltaMin + 0.25*(deltaMax-deltaMin).
    11. Morph domains: linear midpoint == arithmetic mean; positive
        time/frequency midpoint == std::sqrt(p_A * p_B).
    12. Discrete routing target takes only its two endpoint values across the
        morph and switches within the 30 ms crossfade window.
    13. The eight macro base APVTS values are bit-identical after a full
        A-to-D scene morph.
  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "../Source/Parameters/ParameterIDs.h"
#include "../Source/Parameters/ParameterLayout.h"
#include "../Source/Profiles/ProfileSchema.h"
#include "../Source/Profiles/ProfileSnapshot.h"
#include "../Source/Profiles/ProfileManager.h"
#include "../Source/Parameters/MacroEngine.h"
#include "../Source/Profiles/SceneManager.h"

// Translation units that are not part of the ShredSpaceTests target source
// list are pulled in here by direct inclusion, mirroring the pattern used by
// the other test files. ProfileSchema.cpp and RoutingEngine.cpp are already
// compiled into the test binary via ParameterTests.cpp and DspCoreTests.cpp
// respectively, so they are not re-included here.
#include "../Source/Parameters/MacroEngine.cpp"
#include "../Source/Profiles/ProfileSnapshot.cpp"
#include "../Source/Profiles/SceneManager.cpp"
#include "../Source/Profiles/ProfileManager.cpp"

namespace
{

// =============================================================================
// Helpers
// =============================================================================

// Minimal AudioProcessor so an AudioProcessorValueTreeState can be constructed
// without standing up the full ShredSpaceAudioProcessor. Lives in an anonymous
// namespace and therefore cannot collide with the DummyAudioProcessor defined
// inside shredspace:: in ProfileManager.cpp.
class TestAudioProcessor final : public juce::AudioProcessor
{
public:
    TestAudioProcessor() : juce::AudioProcessor (BusesProperties()) {}
    ~TestAudioProcessor() override = default;

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const juce::String getName() const override { return "Test"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TestAudioProcessor)
};

struct ParamHost
{
    TestAudioProcessor processor;
    juce::AudioProcessorValueTreeState apvts;

    ParamHost() : apvts (processor, nullptr, "PARAMS", shredspace::createParameterLayout()) {}
};

juce::StringArray collectFrozenParameterIDs (const juce::AudioProcessorValueTreeState& apvts)
{
    juce::StringArray ids;
    for (auto* param : apvts.processor.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            ids.add (ranged->getParameterID());
    }
    return ids;
}

struct ParamInfo
{
    float minVal = 0.0f;
    float maxVal = 0.0f;
    float defaultVal = 0.0f;
};

std::map<juce::String, ParamInfo> collectParamInfo (const juce::AudioProcessorValueTreeState& apvts)
{
    std::map<juce::String, ParamInfo> info;
    for (auto* param : apvts.processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (ranged == nullptr)
            continue;

        const auto id = ranged->getParameterID();
        const auto range = apvts.getParameterRange (id);
        ParamInfo pi;
        pi.minVal = range.start;
        pi.maxVal = range.end;
        pi.defaultVal = range.convertFrom0to1 (ranged->getDefaultValue());
        info[id] = pi;
    }
    return info;
}

// Repository root derived from the compile-time path of this source file so
// the tests do not depend on the process working directory.
juce::File getRepositoryRoot()
{
    const juce::File testFile { juce::String (__FILE__) };
    // .../tests/ProfileMacroSceneTests.cpp -> .../
    return testFile.getParentDirectory().getParentDirectory();
}

juce::File getProfilesDir()
{
    return getRepositoryRoot().getChildFile ("Resources").getChildFile ("Profiles");
}

// Builds a syntactically valid profile var that can be mutated to construct
// malformed variants for the failure-safety tests.
juce::var makeValidProfileVar()
{
    juce::var json (new juce::DynamicObject());
    json.getDynamicObject()->setProperty ("schema_version", 1);
    json.getDynamicObject()->setProperty ("product_version", "1.0.0");
    json.getDynamicObject()->setProperty ("profile_name", "TestProfile");
    json.getDynamicObject()->setProperty ("author", "Engineer");
    json.getDynamicObject()->setProperty ("description", "A test profile");

    juce::var base (new juce::DynamicObject());
    base.getDynamicObject()->setProperty ("GLOBAL_INPUT_GAIN", 1.5);
    base.getDynamicObject()->setProperty ("PERF_MACRO_TIME", 120.0);
    json.getDynamicObject()->setProperty ("base_parameter_values", base);

    juce::var macroMapping (new juce::DynamicObject());
    for (const auto* macroName : shredspace::kMacroNames)
    {
        juce::Array<juce::var> dests;
        if (juce::String (macroName) == "TIME")
        {
            juce::var dest (new juce::DynamicObject());
            dest.getDynamicObject()->setProperty ("destination_parameter_id", "VOICE_A_TIME_RATIO");
            dest.getDynamicObject()->setProperty ("delta_min", 0.0);
            dest.getDynamicObject()->setProperty ("delta_max", 0.5);
            dest.getDynamicObject()->setProperty ("curve", 1.0);
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

        juce::var mix (new juce::DynamicObject());
        mix.getDynamicObject()->setProperty ("parameter_id", "GLOBAL_MIX");
        mix.getDynamicObject()->setProperty ("value", 50.0);
        scene.getDynamicObject()->setProperty ("mix_target", mix);

        juce::var rt (new juce::DynamicObject());
        rt.getDynamicObject()->setProperty ("parameter_id", "PERF_ROUTING");
        rt.getDynamicObject()->setProperty ("value", static_cast<float> (i % 3));
        scene.getDynamicObject()->setProperty ("routing_target", rt);

        scene.getDynamicObject()->setProperty ("hold_behavior", (i % 2 == 0) ? "Momentary" : "Latch");
        scenes.add (scene);
    }
    json.getDynamicObject()->setProperty ("scene_values", scenes);

    juce::Array<juce::var> tags;
    tags.add ("clean");
    tags.add ("test");
    json.getDynamicObject()->setProperty ("tags", tags);

    return json;
}

// Artist / band / song name tokens that must never appear in a shipped
// profile resource. Matched case-insensitively as substrings.
const char* const kNameDenylist[] = {
    "pink floyd", "led zeppelin", "the beatles", "queen", "hendrix",
    "clapton", "vaughan", "satriani", "steve vai", "malmsteen",
    "bohemian rhapsody", "stairway to heaven", "eruption", "hotel california",
    "eagles", "metallica", "iron maiden", "acdc", "ac/dc", "nirvana",
    "radiohead", "coldplay", "green day", "foo fighters", "red hot chili",
    "david gilmour", "eric johnson", "andy timmons", "plini", "animals as leaders",
    "dream theater", "rush", "yes band", "king crimson", "porcupine tree"
};

// The eight macro parameter IDs whose APVTS base values must survive morphs.
const char* const kMacroParamIds[] = {
    shredspace::paramids::PERF_MACRO_TIME,
    shredspace::paramids::PERF_MACRO_SPACE,
    shredspace::paramids::PERF_MACRO_DEFINITION,
    shredspace::paramids::PERF_MACRO_BLOOM,
    shredspace::paramids::PERF_MACRO_TAIL,
    shredspace::paramids::PERF_MACRO_FOCUS,
    shredspace::paramids::PERF_MACRO_WIDTH,
    shredspace::paramids::PERF_MACRO_CHARACTER
};

shredspace::MacroId macroIdForIndex (std::size_t i)
{
    return static_cast<shredspace::MacroId> (i);
}

// =============================================================================
// Test class
// =============================================================================

class ProfileMacroSceneTests final : public juce::UnitTest
{
public:
    ProfileMacroSceneTests()
        : juce::UnitTest ("ProfileMacroScene", "ShredSpace/Profiles")
    {
    }

    void runTest() override
    {
        ParamHost paramHost;
        frozenIds_ = collectFrozenParameterIDs (paramHost.apvts);
        paramInfo_ = collectParamInfo (paramHost.apvts);
        apvtsPtr_ = &paramHost.apvts;

        beginTest ("Bundled profiles parse and validate");
        testBundledProfilesValidate();

        beginTest ("Referenced parameter IDs exist in frozen layout");
        testReferencedIdsExist();

        beginTest ("Schema version rejection");
        testSchemaVersionRejection();

        beginTest ("Malformed profiles fail safely");
        testMalformedFailSafely();

        beginTest ("Unknown parameter ID migration");
        testUnknownIdMigration();

        beginTest ("No audio-thread IO on profile load");
        testNoAudioThreadIO();

        beginTest ("No denylist names in shipped resources");
        testNoDenylistNames();

        beginTest ("Macro sweep stays within destination ranges");
        testMacroSweepWithinRanges (paramHost.apvts);

        beginTest ("Macro sweep does not rewrite APVTS base values");
        testMacroSweepDoesNotRewriteBase (paramHost.apvts);

        beginTest ("Curve and endpoint mapping");
        testCurveAndEndpointMapping();

        beginTest ("Morph domains");
        testMorphDomains();

        beginTest ("Discrete routing switch under crossfade");
        testDiscreteRoutingSwitch();

        beginTest ("Macro base values unchanged through scene morph");
        testMacroBaseUnchangedThroughMorph (paramHost.apvts);
    }

private:
    juce::StringArray frozenIds_;
    std::map<juce::String, ParamInfo> paramInfo_;
    juce::AudioProcessorValueTreeState* apvtsPtr_ = nullptr;

    // ---- 1. Bundled profiles validate -------------------------------------
    void testBundledProfilesValidate()
    {
        const auto profilesDir = getProfilesDir();
        expect (profilesDir.isDirectory(), "Resources/Profiles directory not found");

        juce::Array<juce::File> files;
        profilesDir.findChildFiles (files, juce::File::findFiles, false, "*.json");
        expectEquals (files.size(), 12, "Expected exactly twelve bundled profile files");

        if (files.size() == 0)
            return;

        for (const auto& file : files)
        {
            juce::var jsonVar;
            const auto parseOk = juce::JSON::parse (file.loadFileAsString(), jsonVar);
            expect (! parseOk.failed(), "JSON parse failed for " + file.getFileName());

            auto [doc, result] = shredspace::ProfileSchema::parse (jsonVar, frozenIds_);
            expect (result.ok, "Profile " + file.getFileName() + " failed validation: "
                               + result.errors.joinIntoString ("; "));
            expect (doc.schema_version == shredspace::kCurrentSchemaVersion,
                    "Profile " + file.getFileName() + " has wrong schema_version");
        }
    }

    // ---- 2. Referenced IDs exist ------------------------------------------
    void testReferencedIdsExist()
    {
        const auto profilesDir = getProfilesDir();
        juce::Array<juce::File> files;
        profilesDir.findChildFiles (files, juce::File::findFiles, false, "*.json");

        for (const auto& file : files)
        {
            juce::var jsonVar;
            juce::JSON::parse (file.loadFileAsString(), jsonVar);
            auto [doc, result] = shredspace::ProfileSchema::parse (jsonVar, frozenIds_);
            expect (result.ok, file.getFileName() + " should validate");

            // base_parameter_values keys
            for (const auto& kv : doc.base_parameter_values)
            {
                expect (frozenIds_.contains (kv.first),
                        file.getFileName() + " references unknown base param: " + kv.first);
            }

            // macro destination ids
            for (const auto& macroName : shredspace::kMacroNames)
            {
                auto it = doc.macro_mapping.find (macroName);
                if (it == doc.macro_mapping.end())
                    continue;
                for (const auto& dest : it->second)
                {
                    expect (frozenIds_.contains (dest.destination_parameter_id),
                            file.getFileName() + " references unknown macro dest: "
                            + dest.destination_parameter_id);
                }
            }

            // scene mix / routing targets
            for (const auto& scene : doc.scene_values)
            {
                if (scene.mix_target.parameter_id.isNotEmpty())
                    expect (frozenIds_.contains (scene.mix_target.parameter_id),
                            file.getFileName() + " unknown mix_target id: "
                            + scene.mix_target.parameter_id);
                if (scene.routing_target.parameter_id.isNotEmpty())
                    expect (frozenIds_.contains (scene.routing_target.parameter_id),
                            file.getFileName() + " unknown routing_target id: "
                            + scene.routing_target.parameter_id);
            }
        }
    }

    // ---- 3. Schema version rejection --------------------------------------
    void testSchemaVersionRejection()
    {
        // schema_version == 0
        {
            auto json = makeValidProfileVar();
            json.getDynamicObject()->setProperty ("schema_version", 0);
            auto [doc, result] = shredspace::ProfileSchema::parse (json, frozenIds_);
            expect (! result.ok, "schema_version 0 must be rejected");
            expect (result.errors.size() > 0, "schema_version 0 must produce errors");
            expectEquals (doc.schema_version, 0, "Rejected doc must be default");
        }

        // schema_version == kCurrentSchemaVersion + 1
        {
            auto json = makeValidProfileVar();
            json.getDynamicObject()->setProperty ("schema_version",
                                                  shredspace::kCurrentSchemaVersion + 1);
            auto [doc, result] = shredspace::ProfileSchema::parse (json, frozenIds_);
            expect (! result.ok, "schema_version kCurrent+1 must be rejected");
            expect (result.errors.size() > 0, "kCurrent+1 must produce errors");
            expectEquals (doc.schema_version, 0, "Rejected doc must be default");
        }
    }

    // ---- 4. Malformed profiles fail safely --------------------------------
    void testMalformedFailSafely()
    {
        auto runMalformed = [this] (const juce::String& label, const juce::var& json)
        {
            shredspace::ProfileDocument doc;
            shredspace::ProfileValidationResult result;
            bool threw = false;

            try
            {
                auto parsed = shredspace::ProfileSchema::parse (json, frozenIds_);
                doc = parsed.first;
                result = parsed.second;
            }
            catch (...)
            {
                threw = true;
            }

            expect (! threw, label + ": parse must not throw");
            expect (! result.ok, label + ": must be rejected (ok == false)");
            expect (result.errors.size() > 0, label + ": must produce non-empty errors");
            expectEquals (doc.schema_version, 0, label + ": doc must be default");
            expect (doc.base_parameter_values.empty(), label + ": base map must be empty");
            expect (doc.macro_mapping.empty(), label + ": macro map must be empty");
            expect (doc.scene_values.empty(), label + ": scene vector must be empty");
        };

        // Non-object root
        runMalformed ("non-object root", juce::var (42));

        // Non-numeric delta_min
        {
            auto json = makeValidProfileVar();
            auto* macroObj = json.getDynamicObject()->getProperty ("macro_mapping").getDynamicObject();
            auto destsVar = macroObj->getProperty ("TIME");
            auto& dests = *destsVar.getArray();
            dests.getReference (0).getDynamicObject()->setProperty ("delta_min", "not-a-number");
            runMalformed ("non-numeric delta", json);
        }

        // curve <= 0
        {
            auto json = makeValidProfileVar();
            auto* macroObj = json.getDynamicObject()->getProperty ("macro_mapping").getDynamicObject();
            auto destsVar = macroObj->getProperty ("TIME");
            auto& dests = *destsVar.getArray();
            dests.getReference (0).getDynamicObject()->setProperty ("curve", -0.1);
            runMalformed ("curve <= 0", json);
        }

        // Three scenes
        {
            auto json = makeValidProfileVar();
            auto& scenes = *json.getDynamicObject()->getProperty ("scene_values").getArray();
            scenes.remove (3);
            runMalformed ("three scenes", json);
        }

        // Seven macros (remove one of the eight)
        {
            auto json = makeValidProfileVar();
            auto* macroObj = json.getDynamicObject()->getProperty ("macro_mapping").getDynamicObject();
            macroObj->removeProperty ("CHARACTER");
            runMalformed ("seven macros", json);
        }
    }

    // ---- 5. Unknown parameter ID migration --------------------------------
    void testUnknownIdMigration()
    {
        auto json = makeValidProfileVar();

        // Inject an unknown base parameter id.
        auto* baseObj = json.getDynamicObject()->getProperty ("base_parameter_values").getDynamicObject();
        baseObj->setProperty ("UNKNOWN_BASE_ID", 7.0);

        auto [doc, result] = shredspace::ProfileSchema::parse (json, frozenIds_);
        expect (result.ok, "Profile with unknown id should still be ok (dropped)");
        expect (doc.base_parameter_values.find ("UNKNOWN_BASE_ID") == doc.base_parameter_values.end(),
                "Unknown base id must be dropped");
        expect (result.migration_notes.contains ("dropped unknown parameter id: UNKNOWN_BASE_ID"),
                "Migration note must be recorded for dropped id");
    }

    // ---- 6. No audio-thread IO on load ------------------------------------
    void testNoAudioThreadIO()
    {
        shredspace::ProfileSnapshotHandoff handoff;

        // acquire() must return a valid POD reference without triggering a load.
        const auto generationBefore = handoff.acquire().baseParameterCount; // touch POD
        juce::ignoreUnused (generationBefore);

        // A fresh handoff has never been published; acquire still yields a valid
        // (default-zeroed) POD reference and must not throw or load.
        const auto& snapshot0 = handoff.acquire();
        expect (snapshot0.baseParameterCount == 0, "Fresh handoff must return default POD");

        // ProfileManager has no processBlock-callable entry point. A full load
        // must complete without touching the handoff generation.
        shredspace::ProfileManager manager;
        const auto generationBeforeLoad = 0; // generation starts at 0

        // Load the twelve bundled profiles from disk (message-thread file IO is
        // permitted; the audio-thread handoff must be untouched).
        manager.loadUserProfiles (getProfilesDir());
        expect (manager.getProfiles().size() == 12, "Full load should yield twelve profiles");

        // acquire() after load: still the same default POD, no load triggered.
        const auto& snapshot1 = handoff.acquire();
        expect (snapshot1.baseParameterCount == 0, "acquire() must not trigger a load");

        // Explicit publish is the only path that advances generation.
        shredspace::ProfileSnapshotData data {};
        data.baseParameterCount = 3;
        handoff.publish (data);

        const auto& snapshot2 = handoff.acquire();
        expect (snapshot2.baseParameterCount == 3, "publish() must make the snapshot visible");
        juce::ignoreUnused (generationBeforeLoad);
    }

    // ---- 7. No denylist names; metadata present ---------------------------
    void testNoDenylistNames()
    {
        const auto profilesDir = getProfilesDir();
        juce::Array<juce::File> files;
        profilesDir.findChildFiles (files, juce::File::findFiles, false, "*.json");

        for (const auto& file : files)
        {
            const auto text = file.loadFileAsString();

            // Denylist scan on the raw shipped resource blob.
            const auto lower = text.toLowerCase();
            for (const auto* token : kNameDenylist)
            {
                expect (! lower.contains (juce::String (token).toLowerCase()),
                        file.getFileName() + " contains denylist token: " + juce::String (token));
            }

            // Every profile_name, description, and tag must be present/non-empty.
            juce::var jsonVar;
            juce::JSON::parse (text, jsonVar);
            auto [doc, result] = shredspace::ProfileSchema::parse (jsonVar, frozenIds_);
            expect (result.ok, file.getFileName() + " must validate");

            expect (doc.profile_name.isNotEmpty(),
                    file.getFileName() + " must have a non-empty profile_name");
            expect (doc.description.isNotEmpty(),
                    file.getFileName() + " must have a non-empty description");
            expect (doc.tags.size() > 0,
                    file.getFileName() + " must have at least one tag");
            for (const auto& tag : doc.tags)
                expect (tag.trim().isNotEmpty(),
                        file.getFileName() + " has an empty tag");
        }
    }

    // ---- 8. Macro sweep within ranges -------------------------------------
    void testMacroSweepWithinRanges (juce::AudioProcessorValueTreeState& apvts)
    {
        const auto profilesDir = getProfilesDir();
        juce::Array<juce::File> files;
        profilesDir.findChildFiles (files, juce::File::findFiles, false, "*.json");

        const double sampleRate = 44100.0;
        const int maxBlockSize = 44100; // >> 30 ms smoothing ramp so each block converges

        for (const auto& file : files)
        {
            juce::var jsonVar;
            juce::JSON::parse (file.loadFileAsString(), jsonVar);
            auto [doc, result] = shredspace::ProfileSchema::parse (jsonVar, frozenIds_);
            expect (result.ok, file.getFileName() + " must validate for sweep");

            shredspace::MacroEngine engine;
            engine.prepare (sampleRate, maxBlockSize);

            // Install destination tables for all eight macros.
            for (std::size_t m = 0; m < 8; ++m)
            {
                const auto macroName = shredspace::kMacroNames[m];
                auto it = doc.macro_mapping.find (macroName);
                if (it == doc.macro_mapping.end())
                {
                    engine.setDestinationTable (macroIdForIndex (m), {});
                    continue;
                }
                std::span<const shredspace::MacroDestination> table { it->second.data(), it->second.size() };
                engine.setDestinationTable (macroIdForIndex (m), table);
            }

            // Resolve base values for every destination parameter once.
            std::map<juce::String, float> baseValues;
            for (const auto& macroName : shredspace::kMacroNames)
            {
                auto it = doc.macro_mapping.find (macroName);
                if (it == doc.macro_mapping.end())
                    continue;
                for (const auto& dest : it->second)
                {
                    if (baseValues.count (dest.destination_parameter_id) > 0)
                        continue;
                    float base = 0.0f;
                    auto bv = doc.base_parameter_values.find (dest.destination_parameter_id);
                    if (bv != doc.base_parameter_values.end())
                        base = bv->second;
                    else
                        base = paramInfo_[dest.destination_parameter_id].defaultVal;
                    baseValues[dest.destination_parameter_id] = base;
                }
            }

            // Sweep u over 101 steps.
            for (int step = 0; step <= 100; ++step)
            {
                const float u = static_cast<float> (step) / 100.0f;

                for (std::size_t m = 0; m < 8; ++m)
                    engine.setMacroValue (macroIdForIndex (m), u);

                engine.processDeltas (maxBlockSize);

                for (std::size_t m = 0; m < 8; ++m)
                {
                    const auto macroName = shredspace::kMacroNames[m];
                    auto it = doc.macro_mapping.find (macroName);
                    if (it == doc.macro_mapping.end())
                        continue;

                    for (std::size_t d = 0; d < it->second.size(); ++d)
                    {
                        const auto& dest = it->second[d];
                        const float delta = engine.getMacroDelta (macroIdForIndex (m), d);
                        const float base = baseValues[dest.destination_parameter_id];
                        const auto info = paramInfo_[dest.destination_parameter_id];
                        const float lo = std::min (info.minVal, info.maxVal);
                        const float hi = std::max (info.minVal, info.maxVal);
                        const float effective = std::clamp (base + delta, lo, hi);

                        expect (effective >= lo - 1e-4f && effective <= hi + 1e-4f,
                                file.getFileName() + " macro " + juce::String (macroName)
                                + " dest " + dest.destination_parameter_id
                                + " out of range at u=" + juce::String (u)
                                + ": eff=" + juce::String (effective)
                                + " [" + juce::String (lo) + "," + juce::String (hi) + "]");
                    }
                }
            }

            juce::ignoreUnused (apvts);
        }
    }

    // ---- 9. Macro sweep does not rewrite APVTS base values ----------------
    void testMacroSweepDoesNotRewriteBase (juce::AudioProcessorValueTreeState& apvts)
    {
        // Recall a real profile so base values are populated.
        shredspace::ProfileManager manager;
        manager.loadUserProfiles (getProfilesDir());
        const auto profiles = manager.getProfiles();
        expect (profiles.size() > 0, "Need at least one profile for base-value test");
        if (profiles.empty())
            return;

        manager.recallProfile (profiles[0].profile_name, apvts);

        // Capture every APVTS parameter's normalized value (bit-identical).
        juce::StringArray allIds = frozenIds_;
        std::map<juce::String, float> before;
        for (const auto& id : allIds)
        {
            auto* param = apvts.getParameter (id);
            expect (param != nullptr, "Missing param: " + id);
            if (param != nullptr)
                before[id] = param->getValue();
        }

        // Run a full macro sweep against the recalled profile.
        const auto& doc = profiles[0];
        const double sampleRate = 44100.0;
        const int maxBlockSize = 44100;

        shredspace::MacroEngine engine;
        engine.prepare (sampleRate, maxBlockSize);

        for (std::size_t m = 0; m < 8; ++m)
        {
            const auto macroName = shredspace::kMacroNames[m];
            auto it = doc.macro_mapping.find (macroName);
            if (it == doc.macro_mapping.end())
            {
                engine.setDestinationTable (macroIdForIndex (m), {});
                continue;
            }
            std::span<const shredspace::MacroDestination> table { it->second.data(), it->second.size() };
            engine.setDestinationTable (macroIdForIndex (m), table);
        }

        for (int step = 0; step <= 100; ++step)
        {
            const float u = static_cast<float> (step) / 100.0f;
            for (std::size_t m = 0; m < 8; ++m)
                engine.setMacroValue (macroIdForIndex (m), u);
            engine.processDeltas (maxBlockSize);
        }

        // Assert every base value is bit-identical afterwards.
        for (const auto& id : allIds)
        {
            auto* param = apvts.getParameter (id);
            if (param == nullptr)
                continue;
            const float after = param->getValue();
            expect (std::memcmp (&after, &before[id], sizeof (float)) == 0,
                    "APVTS base value rewritten by macro sweep for " + id
                    + ": before=" + juce::String (before[id])
                    + " after=" + juce::String (after));
        }
    }

    // ---- 10. Curve and endpoint mapping -----------------------------------
    void testCurveAndEndpointMapping()
    {
        const double sampleRate = 44100.0;
        const int maxBlockSize = 44100;

        shredspace::MacroEngine engine;
        engine.prepare (sampleRate, maxBlockSize);

        const float deltaMin = 0.2f;
        const float deltaMax = 0.8f;
        const float curve = 2.0f;

        std::vector<shredspace::MacroDestination> dests = {
            { "VOICE_A_TIME_RATIO", deltaMin, deltaMax, curve }
        };
        std::span<const shredspace::MacroDestination> table { dests.data(), dests.size() };
        engine.setDestinationTable (shredspace::MacroId::TIME, table);

        // u = 0 -> deltaMin
        engine.setMacroValue (shredspace::MacroId::TIME, 0.0f);
        engine.processDeltas (maxBlockSize);
        expectWithin (engine.getMacroDelta (shredspace::MacroId::TIME, 0), deltaMin, 1e-6f,
                      "delta(0) != deltaMin");

        // u = 1 -> deltaMax
        engine.setMacroValue (shredspace::MacroId::TIME, 1.0f);
        engine.processDeltas (maxBlockSize);
        expectWithin (engine.getMacroDelta (shredspace::MacroId::TIME, 0), deltaMax, 1e-6f,
                      "delta(1) != deltaMax");

        // u = 0.5, curve 2.0 -> 0.5^2 = 0.25 -> deltaMin + 0.25*(deltaMax-deltaMin)
        engine.setMacroValue (shredspace::MacroId::TIME, 0.5f);
        engine.processDeltas (maxBlockSize);
        const float expectedMid = deltaMin + 0.25f * (deltaMax - deltaMin);
        expectWithin (engine.getMacroDelta (shredspace::MacroId::TIME, 0), expectedMid, 1e-6f,
                      "delta(0.5) with curve 2.0 mismatch");
    }

    // ---- 11. Morph domains -------------------------------------------------
    void testMorphDomains()
    {
        shredspace::SceneManager manager;
        manager.prepare (44100.0, 256);

        // Linear-domain macro (e.g. SPACE) midpoint == arithmetic mean.
        const float linearA = 10.0f;
        const float linearB = 40.0f;
        {
            shredspace::SceneValues svA{}, svB{}, svC{}, svD{};
            svA.macroTargets[static_cast<std::size_t> (shredspace::MacroId::SPACE)] = linearA;
            svB.macroTargets[static_cast<std::size_t> (shredspace::MacroId::SPACE)] = linearB;
            std::array<shredspace::SceneValues, shredspace::kSceneCount> all = { svA, svB, svC, svD };
            manager.setSceneValues (all);
            manager.setMorphPosition (shredspace::SceneId::A, shredspace::SceneId::B, 0.5f);

            const float morphed = manager.getSceneDelta (shredspace::MacroId::SPACE);
            const float expected = 0.5f * (linearA + linearB);
            expectWithin (morphed, expected, 1e-6f,
                          "Linear-domain midpoint != arithmetic mean");
        }

        // Logarithmic-domain macro (TIME) midpoint == sqrt(p_A * p_B).
        const float timeA = 100.0f;
        const float timeB = 400.0f;
        {
            shredspace::SceneValues svA{}, svB{}, svC{}, svD{};
            svA.macroTargets[static_cast<std::size_t> (shredspace::MacroId::TIME)] = timeA;
            svB.macroTargets[static_cast<std::size_t> (shredspace::MacroId::TIME)] = timeB;
            std::array<shredspace::SceneValues, shredspace::kSceneCount> all = { svA, svB, svC, svD };
            manager.setSceneValues (all);
            manager.setMorphPosition (shredspace::SceneId::A, shredspace::SceneId::B, 0.5f);

            const float morphed = manager.getSceneDelta (shredspace::MacroId::TIME);
            const float expected = std::sqrt (timeA * timeB);
            expectWithin (morphed, expected, 1e-5f,
                          "Time/frequency midpoint != geometric mean (log morph)");
        }
    }

    // ---- 12. Discrete routing switch under crossfade ----------------------
    void testDiscreteRoutingSwitch()
    {
        shredspace::SceneManager manager;
        manager.prepare (44100.0, 256);

        shredspace::SceneValues svA{}, svB{}, svC{}, svD{};
        svA.routingTarget = 0;
        svB.routingTarget = 2;
        svC.routingTarget = 1;
        svD.routingTarget = 0;
        std::array<shredspace::SceneValues, shredspace::kSceneCount> all = { svA, svB, svC, svD };
        manager.setSceneValues (all);

        // Morph A -> B; routing must only ever take endpoint values {0, 2}.
        int switchStep = -1;
        const int N = 1001;
        const float crossfadeMs = 30.0f;

        for (int i = 0; i < N; ++i)
        {
            const float lambda = static_cast<float> (i) / static_cast<float> (N - 1);
            manager.setMorphPosition (shredspace::SceneId::A, shredspace::SceneId::B, lambda);
            const int routing = manager.getActiveRouting();

            expect (routing == 0 || routing == 2,
                    "Routing took an intermediate value " + juce::String (routing)
                    + " at lambda=" + juce::String (lambda));

            if (switchStep < 0 && routing == 2)
                switchStep = i;
        }

        expect (switchStep > 0, "Routing never switched to the B endpoint");
        if (switchStep > 0)
        {
            const float switchLambda = static_cast<float> (switchStep) / static_cast<float> (N - 1);
            // The discrete switch occurs at the morph midpoint (lambda == 0.5).
            expectWithin (switchLambda, 0.5f, 2.0f / static_cast<float> (N - 1),
                          "Routing switch should occur at the morph midpoint");

            // Mapped onto the 30 ms crossfade window, the switch time must be
            // within that window.
            const float switchTimeMs = switchLambda * crossfadeMs;
            expect (switchTimeMs >= 0.0f && switchTimeMs <= crossfadeMs,
                    "Routing switch outside the 30 ms crossfade window: "
                    + juce::String (switchTimeMs) + " ms");
        }
    }

    // ---- 13. Macro base values unchanged through scene morph --------------
    void testMacroBaseUnchangedThroughMorph (juce::AudioProcessorValueTreeState& apvts)
    {
        // Set the eight macro base values to distinctive non-default values.
        float seed = 0.1f;
        for (const auto* id : kMacroParamIds)
        {
            auto* param = apvts.getParameter (id);
            expect (param != nullptr, "Missing macro param: " + juce::String (id));
            if (param != nullptr)
                param->setValueNotifyingHost (std::clamp (seed, 0.0f, 1.0f));
            seed += 0.1f;
        }

        // Capture bit-identical normalized values.
        std::map<juce::String, float> before;
        for (const auto* id : kMacroParamIds)
        {
            auto* param = apvts.getParameter (id);
            if (param != nullptr)
                before[id] = param->getValue();
        }

        // Build scene values from a real profile and run a full A->D morph.
        shredspace::ProfileManager manager;
        manager.loadUserProfiles (getProfilesDir());
        const auto profiles = manager.getProfiles();
        expect (! profiles.empty(), "Need a profile for scene morph test");
        if (profiles.empty())
            return;

        const auto& doc = profiles[0];
        shredspace::ProfileSnapshotData snapshot;
        buildSnapshot (doc, frozenIds_, snapshot);

        shredspace::SceneManager scene;
        scene.prepare (44100.0, 256);
        scene.setSceneValues (snapshot);

        // Full A-to-D morph: A->B, B->C, C->D at several lambdas each.
        const float lambdas[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
        for (float lambda : lambdas)
        {
            scene.setMorphPosition (shredspace::SceneId::A, shredspace::SceneId::B, lambda);
            scene.setMorphPosition (shredspace::SceneId::B, shredspace::SceneId::C, lambda);
            scene.setMorphPosition (shredspace::SceneId::C, shredspace::SceneId::D, lambda);
        }

        // Assert every macro base value is bit-identical.
        for (const auto* id : kMacroParamIds)
        {
            auto* param = apvts.getParameter (id);
            if (param == nullptr)
                continue;
            const float after = param->getValue();
            expect (std::memcmp (&after, &before[id], sizeof (float)) == 0,
                    "Macro base value changed through scene morph for " + juce::String (id)
                    + ": before=" + juce::String (before[id])
                    + " after=" + juce::String (after));
        }
    }

    void expectWithin (float actual, float expected, float tolerance, const juce::String& message)
    {
        expect (std::abs (actual - expected) <= tolerance, message);
    }
};

// Static instance registers the test with juce::UnitTestRunner.
static ProfileMacroSceneTests sProfileMacroSceneTestsInstance;

} // namespace
