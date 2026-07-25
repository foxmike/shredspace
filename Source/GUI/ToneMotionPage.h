#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../Parameters/ParameterIDs.h"
#include "../Telemetry/TelemetryState.h"

namespace shredspace
{

class ToneMotionPage : public juce::Component
{
public:
    ToneMotionPage (juce::AudioProcessorValueTreeState& apvtsToUse,
                    shredspace::TelemetryState& telemetryStateToUse);
    ~ToneMotionPage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    shredspace::TelemetryState& telemetryState;

    struct VoiceSection
    {
        // Real sliders & labels
        juce::Slider hpfSlider;
        juce::Label hpfLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hpfAttachment;

        juce::Slider lpfSlider;
        juce::Label lpfLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lpfAttachment;

        // Slope controls (dummy)
        juce::ComboBox hpfSlopeSelector;
        juce::Label hpfSlopeLabel;

        juce::ComboBox lpfSlopeSelector;
        juce::Label lpfSlopeLabel;

        // Presence Guard
        juce::Slider presenceFreqSlider;
        juce::Label presenceFreqLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> presenceFreqAttachment;

        juce::Slider presenceThresholdSlider;
        juce::Label presenceThresholdLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> presenceThresholdAttachment;

        juce::Slider presenceAmountSlider; // dummy
        juce::Label presenceAmountLabel;

        // Transient Softener
        juce::Slider transientAmountSlider;
        juce::Label transientAmountLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> transientAmountAttachment;

        juce::Slider transientTimeSlider; // dummy
        juce::Label transientTimeLabel;

        // Saturation
        juce::Slider saturationDriveSlider;
        juce::Label saturationDriveLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> saturationDriveAttachment;

        juce::Slider saturationCharacterSlider; // dummy
        juce::Label saturationCharacterLabel;

        // Modulation
        juce::Slider modRateSlider;
        juce::Label modRateLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modRateAttachment;

        juce::Slider modDepthSlider;
        juce::Label modDepthLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modDepthAttachment;

        // Drift
        juce::Slider driftSlider;
        juce::Label driftLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driftAttachment;

        // Diffusion
        juce::Slider diffusionAmountSlider;
        juce::Label diffusionAmountLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> diffusionAmountAttachment;

        juce::Slider diffusionSizeSlider; // dummy
        juce::Label diffusionSizeLabel;

        // Stereo behaviour
        juce::Slider stereoWidthSlider;
        juce::Label stereoWidthLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stereoWidthAttachment;

        juce::Slider stereoSpreadSlider;
        juce::Label stereoSpreadLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stereoSpreadAttachment;

        juce::Slider stereoCorrelationSlider;
        juce::Label stereoCorrelationLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stereoCorrelationAttachment;
    };

    void initVoiceSection (VoiceSection& section, bool isVoiceB);
    void setupSlider (juce::Slider& slider, const juce::String& paramId, juce::AudioProcessorValueTreeState& state, const juce::String& suffix);
    void setupLabel (juce::Label& label, const juce::String& text);

    VoiceSection voiceA;
    VoiceSection voiceB;

    // Voice Column Labels
    juce::Label voiceAToneHeaderLabel;
    juce::Label voiceBToneHeaderLabel;
    juce::Label voiceAMotionHeaderLabel;
    juce::Label voiceBMotionHeaderLabel;

    // Global Low-Frequency Crossover Freq
    juce::Slider lfCrossoverSlider;
    juce::Label lfCrossoverLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfCrossoverAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneMotionPage)
};

} // namespace shredspace
