#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Telemetry/TelemetryState.h"

/**
    MeterComponent displays stereo peak/RMS output meters with GUI ballistics
    and an indicator cluster (activity, density, contraction, bloom, safety status).
*/
class MeterComponent : public juce::Component, private juce::Timer
{
public:
    explicit MeterComponent (shredspace::TelemetryState& telemetryStateToUse);
    ~MeterComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    // Helper functions for mapping values
    static float convertLinearToDb (float level) noexcept;
    static float mapDbToProportional (float db) noexcept;

    void drawLabelledBar (juce::Graphics& g, const juce::String& label, float value, juce::Rectangle<int> area);
    void drawSafetyLamp (juce::Graphics& g, bool isEngaged, juce::Rectangle<int> area);

    shredspace::TelemetryState& telemetryState;

    // Ballistics decay factor (per 33.3ms tick)
    static constexpr float decayFactor = 0.9f;

    // Meter levels (smoothed on GUI side)
    float leftPeakLevel { 0.0f };
    float rightPeakLevel { 0.0f };
    float leftRmsLevel { 0.0f };
    float rightRmsLevel { 0.0f };

    // Peak-hold marker state
    float leftPeakHold { 0.0f };
    float rightPeakHold { 0.0f };
    int leftHoldTicks { 0 };
    int rightHoldTicks { 0 };

    // Cached indicator levels
    float activityLevel { 0.0f };
    float densityLevel { 0.0f };
    float contractionLevel { 0.0f };
    float bloomLevel { 0.0f };
    bool safetyEngagedVal { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterComponent)
};
