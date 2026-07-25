#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../Parameters/ParameterIDs.h"
#include "../Parameters/MacroEngine.h"
#include "../Profiles/SceneManager.h"
#include "../Profiles/ProfileManager.h"
#include "../Telemetry/TelemetryState.h"
#include "MeterComponent.h"

namespace shredspace
{

class PerformancePage : public juce::Component,
                        public juce::AudioProcessorValueTreeState::Listener
{
public:
    PerformancePage (juce::AudioProcessorValueTreeState& apvtsToUse,
                     shredspace::MacroEngine& macroEngineToUse,
                     shredspace::SceneManager& sceneManagerToUse,
                     shredspace::ProfileManager& profileManagerToUse,
                     shredspace::TelemetryState& telemetryStateToUse);

    ~PerformancePage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void parameterChanged (const juce::String& parameterID, float newValue) override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    shredspace::MacroEngine& macroEngine;
    shredspace::SceneManager& sceneManager;
    shredspace::ProfileManager& profileManager;

    // Macro controls (8 rotary sliders)
    std::array<juce::Slider, 8> macroSliders;
    std::array<juce::Label, 8> macroLabels;

    // Profile browser combo box
    juce::Label profileLabel;
    juce::ComboBox profileComboBox;

    // Four scene select buttons
    juce::Label sceneLabel;
    std::array<juce::TextButton, 4> sceneButtons;

    // Global Mix, input, output sliders
    juce::Slider mixSlider;
    juce::Label mixLabel;

    juce::Slider inputGainSlider;
    juce::Label inputGainLabel;

    juce::Slider outputGainSlider;
    juce::Label outputGainLabel;

    // Bypass, Tap Tempo, Hold buttons
    juce::TextButton bypassButton;
    juce::TextButton tapTempoButton;
    juce::TextButton holdButton;

    // Adaptive mode combo box
    juce::Label adaptiveModeLabel;
    juce::ComboBox adaptiveModeComboBox;

    // Owned MeterComponent instance
    MeterComponent meterComponent;

    // Attachments
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> macroAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tapTempoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> holdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> adaptiveModeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerformancePage)
};

} // namespace shredspace
