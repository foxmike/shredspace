#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Telemetry/TelemetryState.h"

namespace shredspace
{

class DetectorDisplay : public juce::Component, private juce::Timer
{
public:
    explicit DetectorDisplay (shredspace::TelemetryState& telemetryStateToUse);
    ~DetectorDisplay() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    shredspace::TelemetryState& telemetryState;

    // Fixed-size ring buffer of HistorySample
    static constexpr int ringBufferSize = 512;
    std::array<shredspace::HistorySample, ringBufferSize> ringBuffer {};
    int writeIndex { 0 };

    // Cached current scalar values from TelemetryState atomics (relaxed memory order)
    float currentActivity { 0.0f };
    float currentDensity { 0.0f };
    float currentSustain { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DetectorDisplay)
};

} // namespace shredspace
