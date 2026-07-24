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
)

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
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

enable_testing()
include(CTest)
