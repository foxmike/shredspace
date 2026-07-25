cmake_minimum_required(VERSION 3.22)

project(ShredSpace VERSION 1.0.0 LANGUAGES CXX)

enable_language(C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_plugin(ShredSpace
    PRODUCT_NAME "ShredSpace"
    COMPANY_NAME "nSpace Systems"
    VERSION 1.0.0
    PLUGIN_MANUFACTURER_CODE NSPC
    PLUGIN_CODE ShSp
    BUNDLE_ID com.nspacesystems.shredspace
    LV2URI urn:nspacesystems:plugin:shredspace
    FORMATS VST3 LV2 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(ShredSpace
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Telemetry/TelemetryState.cpp
        Source/GUI/CustomLookAndFeel.cpp
        Source/GUI/MeterComponent.cpp
        Source/GUI/PerformancePage.cpp
        Source/GUI/VoicesPage.cpp
        Source/GUI/ToneMotionPage.cpp
        Source/GUI/IntelligencePage.cpp
        Source/GUI/DetectorDisplay.cpp
        Source/DSP/FractionalDelay.cpp
        Source/DSP/FeedbackController.cpp
        Source/DSP/DelayTimeController.cpp
        Source/DSP/DelayVoice.cpp
        Source/DSP/ModulationEngine.cpp
        Source/DSP/RepeatToneProcessor.cpp
        Source/DSP/PresenceGuard.cpp
        Source/DSP/RepeatTransientSoftener.cpp
        Source/DSP/SoftSaturator.cpp
        Source/DSP/ShortDiffuser.cpp
        Source/DSP/StereoProcessor.cpp
        Source/DSP/RoutingEngine.cpp
        Source/DSP/SafetyProcessor.cpp
        Source/Analysis/AnalysisResampler.cpp
        Source/Analysis/PerformanceAnalyzer.cpp
        Source/Analysis/OnsetDetector.cpp
        Source/Analysis/DensityEstimator.cpp
        Source/Analysis/SustainEstimator.cpp
        Source/Analysis/AdaptiveController.cpp
        Source/Profiles/ProfileSchema.cpp
        Source/Profiles/ProfileManager.cpp
        Source/Profiles/ProfileSnapshot.cpp
        Source/Profiles/SceneManager.cpp
        Source/Parameters/MacroEngine.cpp
)

# Bundled profile resources: the twelve engineering starting-value JSON
# profiles under Resources/Profiles are embedded into the plugin binary.
juce_add_binary_data(ShredSpaceProfiles
    SOURCES
        Resources/Profiles/PrecisionRack.json
        Resources/Profiles/RapidDefinition.json
        Resources/Profiles/NeoclassicalFlight.json
        Resources/Profiles/WideArpeggio.json
        Resources/Profiles/SingingLead.json
        Resources/Profiles/DryCenterArena.json
        Resources/Profiles/LegatoSpace.json
        Resources/Profiles/DarkHighGain.json
        Resources/Profiles/BalladBloom.json
        Resources/Profiles/ControlledCascade.json
        Resources/Profiles/MonoRigPolish.json
        Resources/Profiles/BufferBend.json
)

# The DSP translation units include their headers as "DSP/<name>.h", which
# requires the Source/ directory on the include path.
target_include_directories(ShredSpace PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

add_library(ShredSpaceParameterSources INTERFACE)
target_sources(ShredSpaceParameterSources
    INTERFACE
        Source/Parameters/ParameterLayout.cpp
        Source/Parameters/ParameterSnapshot.cpp
)

juce_generate_juce_header(ShredSpace)

target_compile_definitions(ShredSpace
    PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(ShredSpace
    PRIVATE
        ShredSpaceParameterSources
        ShredSpaceProfiles
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

enable_testing()
include(CTest)

# ---------------------------------------------------------------------------
# Console test runner. Runs every registered juce::UnitTest under CTest.
# Uses ONLY juce::UnitTest; no third-party test frameworks are involved.
# ---------------------------------------------------------------------------
add_executable(ShredSpaceTests
    tests/TestMain.cpp
    tests/ParameterTests.cpp
    tests/DspCoreTests.cpp
    tests/RepeatChainTests.cpp
    tests/MotionStereoTests.cpp
    tests/SafetyHoldTests.cpp
    tests/StaticSignalPathTests.cpp
    tests/AnalysisTests.cpp
    tests/ProfileMacroSceneTests.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Telemetry/TelemetryState.cpp
    Source/GUI/CustomLookAndFeel.cpp
    Source/GUI/MeterComponent.cpp
    Source/GUI/PerformancePage.cpp
    Source/GUI/VoicesPage.cpp
    Source/GUI/ToneMotionPage.cpp
    Source/GUI/IntelligencePage.cpp
    Source/GUI/DetectorDisplay.cpp
    Source/Parameters/ParameterLayout.cpp
    Source/Parameters/ParameterSnapshot.cpp
    Source/DSP/ShortDiffuser.cpp
    Source/Analysis/AnalysisResampler.cpp
    Source/Analysis/PerformanceAnalyzer.cpp
    Source/Analysis/OnsetDetector.cpp
    Source/Analysis/DensityEstimator.cpp
    Source/Analysis/SustainEstimator.cpp
    Source/Analysis/AdaptiveController.cpp
)

target_include_directories(ShredSpaceTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

target_compile_definitions(ShredSpaceTests
    PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(ShredSpaceTests
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
)

add_test(NAME ShredSpaceTests COMMAND ShredSpaceTests)
