#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "DetectorDisplay.h"
#include "Telemetry/TelemetryState.h"
#include "Parameters/ParameterIDs.h"

namespace shredspace
{

class IntelligencePage : public juce::Component
{
public:
    IntelligencePage (juce::AudioProcessorValueTreeState& apvtsToUse,
                      shredspace::TelemetryState& telemetryStateToUse);
    ~IntelligencePage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct ParameterControl
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupControl (ParameterControl& control,
                       const juce::String& paramId,
                       const juce::String& labelText,
                       const juce::String& suffix);

    juce::AudioProcessorValueTreeState& apvts;
    shredspace::TelemetryState& telemetryState;

    // Owned DetectorDisplay
    DetectorDisplay detectorDisplay;

    // --- Detector Settings ---
    ParameterControl sensitivityControl;
    ParameterControl inputGateControl;
    ParameterControl densityWindowControl;
    ParameterControl densityStartControl;
    ParameterControl densityFullControl;
    ParameterControl sustainStartControl;
    ParameterControl sustainFullControl;
    ParameterControl phraseGapControl;
    ParameterControl transientCrossoverControl;

    // --- Contraction Settings ---
    ParameterControl contractionAttackControl;
    ParameterControl contractionReleaseControl;
    ParameterControl wetReductionControl;
    ParameterControl tailReductionControl;
    ParameterControl toneDarkeningControl;
    ParameterControl widthReductionControl;
    ParameterControl attackSoftenIncreaseControl;

    // --- Bloom Settings ---
    ParameterControl bloomRiseControl;
    ParameterControl bloomHoldControl;
    ParameterControl bloomReleaseControl;
    ParameterControl wetLiftControl;
    ParameterControl tailExpansionControl;
    ParameterControl toneOpeningControl;
    ParameterControl widthExpansionControl;
    ParameterControl diffusionIncreaseControl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IntelligencePage)
};

} // namespace shredspace
