#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterSnapshot.h"

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

private:
    juce::AudioProcessorValueTreeState apvts;
    shredspace::ParameterSnapshot parameterSnapshot;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShredSpaceAudioProcessor)
};
