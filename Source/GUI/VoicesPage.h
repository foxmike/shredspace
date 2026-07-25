#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../Parameters/ParameterIDs.h"
#include "../Telemetry/TelemetryState.h"

namespace shredspace
{

class VoicesPage : public juce::Component
{
public:
    VoicesPage (juce::AudioProcessorValueTreeState& apvtsToUse,
                shredspace::TelemetryState& telemetryStateToUse);
    ~VoicesPage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct VoiceParamIDs
    {
        const char* enableId;
        std::array<const char*, 20> sliderIds;
    };

    struct VoicePanel : public juce::Component,
                        public juce::AudioProcessorValueTreeState::Listener
    {
        VoicePanel (juce::AudioProcessorValueTreeState& apvtsToUse,
                    const VoiceParamIDs& ids,
                    const juce::String& title);
        ~VoicePanel() override;

        void paint (juce::Graphics& g) override;
        void resized() override;
        void parameterChanged (const juce::String& parameterID, float newValue) override;

        juce::AudioProcessorValueTreeState& apvts;
        const juce::String voiceTitle;
        const juce::String enableParamId;

        juce::ToggleButton enableButton;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttachment;

        static constexpr int NumSliders = 20;

        struct SliderItem
        {
            juce::Slider slider;
            juce::Label label;
            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        };

        std::array<SliderItem, NumSliders> sliders;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoicePanel)
    };

    juce::AudioProcessorValueTreeState& apvts;
    shredspace::TelemetryState& telemetryState;

    VoicePanel panelA;
    VoicePanel panelB;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoicesPage)
};

} // namespace shredspace
