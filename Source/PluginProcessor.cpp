#include "PluginProcessor.h"
#include "PluginEditor.h"

ShredSpaceAudioProcessor::ShredSpaceAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                            .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                            .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this,
             nullptr,
             juce::Identifier (shredspace::paramids::kApvtsType),
             shredspace::createParameterLayout())
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
    parameterSnapshot.prepare (sampleRate, samplesPerBlock, apvts);
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
    parameterSnapshot.updateFromParameters();

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
    return new ShredSpaceAudioProcessorEditor (*this);
}

void ShredSpaceAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (shredspace::paramids::kSchemaVersionProperty,
                       shredspace::paramids::kStateSchemaVersion,
                       nullptr);

    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void ShredSpaceAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto* messageManager = juce::MessageManager::getInstanceWithoutCreating();
    jassert (messageManager != nullptr && messageManager->isThisTheMessageThread());

    if (messageManager == nullptr
        || ! messageManager->isThisTheMessageThread()
        || data == nullptr
        || sizeInBytes <= 0)
        return;

    juce::MemoryInputStream stream (data, static_cast<size_t> (sizeInBytes), false);
    auto state = juce::ValueTree::readFromStream (stream);
    const juce::Identifier expectedType (shredspace::paramids::kApvtsType);
    const juce::Identifier schemaProperty (shredspace::paramids::kSchemaVersionProperty);

    if (! state.isValid()
        || ! state.hasType (expectedType)
        || ! state.hasProperty (schemaProperty))
        return;

    const auto schemaVersion = state.getProperty (schemaProperty);

    if (! schemaVersion.isInt()
        || static_cast<int> (schemaVersion) != shredspace::paramids::kStateSchemaVersion)
        return;

    apvts.replaceState (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ShredSpaceAudioProcessor();
}
