#include "PluginProcessor.h"

#if __has_include("PluginEditor.h")
 #include "PluginEditor.h"
#endif

ShredSpaceAudioProcessor::ShredSpaceAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                            .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                            .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

ShredSpaceAudioProcessor::~ShredSpaceAudioProcessor() = default;

const juce::String ShredSpaceAudioProcessor::getName() const
{
    return "ShredSpace";
}

bool ShredSpaceAudioProcessor::acceptsMidi() const
{
    return false;
}

bool ShredSpaceAudioProcessor::producesMidi() const
{
    return false;
}

bool ShredSpaceAudioProcessor::isMidiEffect() const
{
    return false;
}

double ShredSpaceAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ShredSpaceAudioProcessor::getNumPrograms()
{
    return 1;
}

int ShredSpaceAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ShredSpaceAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String ShredSpaceAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void ShredSpaceAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void ShredSpaceAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void ShredSpaceAudioProcessor::releaseResources()
{
}

bool ShredSpaceAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    const bool inputIsMono = (input == juce::AudioChannelSet::mono());
    const bool inputIsStereo = (input == juce::AudioChannelSet::stereo());
    const bool outputIsMono = (output == juce::AudioChannelSet::mono());
    const bool outputIsStereo = (output == juce::AudioChannelSet::stereo());

    if (! (inputIsMono || inputIsStereo))
        return false;

    if (! (outputIsMono || outputIsStereo))
        return false;

    if (inputIsStereo && outputIsMono)
        return false;

    return true;
}

void ShredSpaceAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples             = buffer.getNumSamples();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);
}

bool ShredSpaceAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* ShredSpaceAudioProcessor::createEditor()
{
#if __has_include("PluginEditor.h")
    return new ShredSpaceAudioProcessorEditor (*this);
#else
    // TODO(a.2): return new ShredSpaceAudioProcessorEditor (*this);
    return nullptr;
#endif
}

void ShredSpaceAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ignoreUnused (destData);
}

void ShredSpaceAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ShredSpaceAudioProcessor();
}
