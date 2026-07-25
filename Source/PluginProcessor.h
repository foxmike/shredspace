#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterSnapshot.h"
#include "DSP/DelayTimeController.h"
#include "DSP/DelayVoice.h"
#include "DSP/RoutingEngine.h"
#include "DSP/SafetyProcessor.h"
#include "DSP/StereoProcessor.h"
#include "Telemetry/TelemetryState.h"
#include "Analysis/PerformanceAnalyzer.h"
#include "Analysis/OnsetDetector.h"
#include "Analysis/DensityEstimator.h"
#include "Analysis/SustainEstimator.h"
#include "Analysis/AdaptiveController.h"

class ShredSpaceAudioProcessorEditor;

class ShredSpaceAudioProcessor : public juce::AudioProcessor {
public:
    ShredSpaceAudioProcessor();
    ~ShredSpaceAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }
    shredspace::TelemetryState& getTelemetryState() noexcept { return telemetry; }

private:
    static constexpr int kProcessingChannels = 2;

    juce::AudioProcessorValueTreeState apvts;
    shredspace::ParameterSnapshot parameterSnapshot;
    shredspace::DelayTimeController delayTimeController;
    shredspace::DelayVoice delayVoiceA;
    shredspace::DelayVoice delayVoiceB;
    shredspace::RoutingEngine routingEngine;
    shredspace::StereoProcessor stereoProcessor;
    shredspace::SafetyProcessor safetyProcessor;

    shredspace::PerformanceAnalyzer performanceAnalyzer;
    shredspace::OnsetDetector onsetDetector;
    shredspace::DensityEstimator densityEstimator;
    shredspace::SustainEstimator sustainEstimator;
    shredspace::AdaptiveController adaptiveController;
    shredspace::TelemetryState telemetry;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> voiceABuffer;
    juce::AudioBuffer<float> voiceBBuffer;
    juce::AudioBuffer<float> wetBuffer;
    juce::AudioBuffer<float> mixBuffer;

    int maximumBlockSize { 1 };
    bool clearDelayBuffersAtNextBlock {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShredSpaceAudioProcessor)
};
