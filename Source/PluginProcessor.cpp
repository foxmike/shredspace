#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

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
    maximumBlockSize = std::max (1, samplesPerBlock);

    parameterSnapshot.prepare (sampleRate, maximumBlockSize, apvts);
    delayTimeController.prepare (sampleRate);
    delayVoiceA.prepare (sampleRate, maximumBlockSize);
    delayVoiceB.prepare (sampleRate, maximumBlockSize);
    routingEngine.prepare (sampleRate, maximumBlockSize, kProcessingChannels);
    stereoProcessor.prepare (sampleRate, maximumBlockSize);
    safetyProcessor.prepare (sampleRate, maximumBlockSize, kProcessingChannels);

    performanceAnalyzer.prepare (sampleRate, maximumBlockSize);
    const int maxAnalysis = static_cast<int> (std::ceil (maximumBlockSize * (48000.0 / 44100.0))) + 64;
    onsetDetector.prepare (maxAnalysis);
    densityEstimator.prepare();
    sustainEstimator.prepare();
    adaptiveController.prepare();
    telemetry.prepare();

    dryBuffer.setSize (kProcessingChannels, maximumBlockSize, false, true, true);
    voiceABuffer.setSize (kProcessingChannels, maximumBlockSize, false, true, true);
    voiceBBuffer.setSize (kProcessingChannels, maximumBlockSize, false, true, true);
    wetBuffer.setSize (kProcessingChannels, maximumBlockSize, false, true, true);
    mixBuffer.setSize (kProcessingChannels, maximumBlockSize, false, true, true);
    clearDelayBuffersAtNextBlock = false;
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

    if (clearDelayBuffersAtNextBlock)
    {
        delayVoiceA.reset();
        delayVoiceB.reset();
        clearDelayBuffersAtNextBlock = false;
    }

    parameterSnapshot.updateFromParameters();
    const auto numSamples = std::min (buffer.getNumSamples(), maximumBlockSize);
    parameterSnapshot.advance (numSamples);
    const auto& parameters = parameterSnapshot.values();

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (numSamples <= 0 || totalNumInputChannels <= 0 || totalNumOutputChannels <= 0)
        return;

    // Input gain is smoothed in dB for 20 ms by ParameterSnapshot and is
    // converted to linear only after the dB smoother has advanced.
    const float inputGain = std::pow (10.0f, parameters.inputGainDb / 20.0f);
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
        buffer.applyGain (channel, 0, numSamples, inputGain);

    // Preserve a stereo, center-equivalent dry path. For a mono input both
    // working channels receive the same sample; stereo width is wet-only.
    dryBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
    dryBuffer.copyFrom (1, 0, buffer,
                        totalNumInputChannels > 1 ? 1 : 0,
                        0, numSamples);

    // --- Sidechain Analysis ---
    {
        // 1. Configure parameters
        densityEstimator.setWindowDuration (parameters.intelligence.densityWindowMs * 0.001);
        densityEstimator.setDensityStart (parameters.intelligence.densityStartEventsPerSecond);
        densityEstimator.setDensityFull (parameters.intelligence.densityFullEventsPerSecond);

        sustainEstimator.setGateDb (parameters.intelligence.inputGateDb);
        sustainEstimator.setSustainStart (parameters.intelligence.sustainStartMs * 0.001f);
        sustainEstimator.setSustainFull (parameters.intelligence.sustainFullMs * 0.001f);
        sustainEstimator.setGapDuration (parameters.intelligence.phraseGapMs * 0.001f);

        onsetDetector.setThreshold (parameters.intelligence.sensitivityPercent * 0.01f);
        onsetDetector.setNoiseGateDb (parameters.intelligence.inputGateDb);

        adaptiveController.setBloomAmount (parameters.macroBloomPercent * 0.01f);
        adaptiveController.setDefinition (parameters.macroDefinitionPercent * 0.01f);

        shredspace::AdaptiveController::Mode mode = shredspace::AdaptiveController::Mode::Off;
        if (parameters.adaptiveMode == 1)
            mode = shredspace::AdaptiveController::Mode::Level;
        else if (parameters.adaptiveMode == 2)
            mode = shredspace::AdaptiveController::Mode::Articulation;
        adaptiveController.setMode (mode);

        adaptiveController.setDestinationDepths (
            parameters.intelligence.wetReductionDb,
            parameters.intelligence.wetLiftDb,
            parameters.intelligence.tailReductionOctaves,
            parameters.intelligence.tailExpansionOctaves,
            parameters.intelligence.toneDarkeningOctaves,
            parameters.intelligence.toneOpeningOctaves,
            parameters.intelligence.widthReductionPercent * 0.01f,
            parameters.intelligence.widthExpansionPercent * 0.01f,
            parameters.intelligence.attackSoftenIncreasePercent * 0.01f,
            parameters.intelligence.diffusionIncreasePercent * 0.01f
        );

        // 2. Process
        const float* leftDry = dryBuffer.getReadPointer (0);
        const float* rightDry = dryBuffer.getReadPointer (1);
        performanceAnalyzer.process (leftDry, rightDry, kProcessingChannels, numSamples);

        const float* analysisSamples = performanceAnalyzer.getAnalysisBuffer();
        const int numAnalysis = performanceAnalyzer.getNumAnalysisSamples();

        onsetDetector.process (analysisSamples,
                               numAnalysis,
                               performanceAnalyzer.getTimeNovelty(),
                               performanceAnalyzer.getSlowEnvelopeDb());

        int acceptedCount = 0;
        const auto* acceptedOnsets = onsetDetector.consumeAcceptedOnsets (acceptedCount);
        for (int i = 0; i < acceptedCount; ++i)
        {
            densityEstimator.pushOnset (static_cast<std::int64_t> (acceptedOnsets[i].analysisSampleIndex));
        }
        densityEstimator.advance (numAnalysis);

        const float timeSinceLastOnset =
            static_cast<float> (onsetDetector.getSamplesSinceLastOnset()) / 48000.0f;

        sustainEstimator.process (performanceAnalyzer.getSlowEnvelopeDb(),
                                  timeSinceLastOnset,
                                  densityEstimator.getDensity(),
                                  onsetDetector.getSmoothedOnsetActivity(),
                                  numAnalysis);

        adaptiveController.process (densityEstimator.getDensity(),
                                    onsetDetector.getSmoothedOnsetActivity(),
                                    sustainEstimator.getSustain(),
                                    sustainEstimator.getPhraseGap(),
                                    sustainEstimator.getActivity(),
                                    numAnalysis);
    }

    float* dryChannels[] {
        dryBuffer.getWritePointer (0),
        dryBuffer.getWritePointer (1)
    };
    float* voiceAChannels[] {
        voiceABuffer.getWritePointer (0),
        voiceABuffer.getWritePointer (1)
    };
    float* voiceBChannels[] {
        voiceBBuffer.getWritePointer (0),
        voiceBBuffer.getWritePointer (1)
    };
    float* wetChannels[] {
        wetBuffer.getWritePointer (0),
        wetBuffer.getWritePointer (1)
    };
    float* mixChannels[] {
        mixBuffer.getWritePointer (0),
        mixBuffer.getWritePointer (1)
    };

    juce::AudioBuffer<float> dryBlock (dryChannels, kProcessingChannels, numSamples);
    juce::AudioBuffer<float> voiceABlock (voiceAChannels, kProcessingChannels, numSamples);
    juce::AudioBuffer<float> voiceBBlock (voiceBChannels, kProcessingChannels, numSamples);
    juce::AudioBuffer<float> wetBlock (wetChannels, kProcessingChannels, numSamples);
    juce::AudioBuffer<float> mixBlock (mixChannels, kProcessingChannels, numSamples);

    // Acquire the host playhead position once for this block.
    shredspace::HostTiming hostTiming { 120.0, false, false, 0 };
    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto position = hostPlayHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
            {
                hostTiming.bpm = *bpm;
                hostTiming.bpmValid = std::isfinite (*bpm) && *bpm > 0.0;
            }

            hostTiming.isPlaying = position->getIsPlaying();
            if (const auto samplePosition = position->getTimeInSamples())
                hostTiming.samplePosition = *samplePosition;
        }
    }

    shredspace::ControllerInput timingInput;
    timingInput.timeMode = parameters.timeMode;
    timingInput.syncDivision = parameters.syncDivision;
    timingInput.macroTimeMs = parameters.macroTimeMs;
    timingInput.timeGlideMs = parameters.timeGlideMs;
    timingInput.voiceA.timeRatio = parameters.voiceA.timeRatio;
    timingInput.voiceA.fineOffsetMs = parameters.voiceA.fineOffsetMs;
    timingInput.voiceB.timeRatio = parameters.voiceB.timeRatio;
    timingInput.voiceB.fineOffsetMs = parameters.voiceB.fineOffsetMs;
    timingInput.sceneIndex = parameters.sceneSelect;
    const auto delayTargets = delayTimeController.update (timingInput, hostTiming);

    const auto topology = static_cast<shredspace::RoutingEngine::Topology> (
        std::clamp (parameters.routing, 0, 2));
    routingEngine.setTopology (topology);
    routingEngine.setCascade (parameters.routingCascadePercent * 0.01f);
    routingEngine.setVoiceLevels (
        parameters.voiceA.enabled ? parameters.voiceA.levelGain : 0.0f,
        parameters.voiceB.enabled ? parameters.voiceB.levelGain : 0.0f);
    routingEngine.setMix (parameters.mixPercent * 0.01f);
    routingEngine.setMixMode (
        parameters.mixMode == 0 ? shredspace::RoutingEngine::MixMode::Insert
                                : shredspace::RoutingEngine::MixMode::Send);
    routingEngine.setBypassMode (
        ! parameters.bypass
            ? shredspace::RoutingEngine::BypassMode::Off
            : (parameters.bypassMode == 0
                   ? shredspace::RoutingEngine::BypassMode::Hard
                   : shredspace::RoutingEngine::BypassMode::Trails));

    delayVoiceA.setT60Target (parameters.macroTailSeconds);
    delayVoiceB.setT60Target (parameters.macroTailSeconds);
    delayVoiceA.setCrossFeedback (parameters.voiceA.crossFeedbackPercent * 0.01f);
    delayVoiceB.setCrossFeedback (parameters.voiceB.crossFeedbackPercent * 0.01f);
    delayVoiceA.setSaturationAmount (parameters.voiceA.saturationPercent * 0.01f);
    delayVoiceB.setSaturationAmount (parameters.voiceB.saturationPercent * 0.01f);
    delayVoiceA.setDiffusionAmount (parameters.voiceA.diffusionPercent * 0.01f);
    delayVoiceB.setDiffusionAmount (parameters.voiceB.diffusionPercent * 0.01f);
    delayVoiceA.setHold (parameters.hold);
    delayVoiceB.setHold (parameters.hold);

    voiceABlock.clear();
    voiceBBlock.clear();

    const auto processVoiceA = [&] {
        routingEngine.prepareVoiceAInput (dryBlock, voiceBBlock, voiceABlock);
        // RoutingEngine advances its click-free trails ramp while preparing
        // this input; delayInputGain() is the authoritative bypass surface.
        if (routingEngine.delayInputGain() <= 0.0f)
            voiceABlock.clear();
        delayVoiceA.process (voiceABlock.getWritePointer (0),
                             voiceABlock.getWritePointer (1),
                             numSamples, delayTargets[0],
                             parameters.timeGlideMs,
                             parameters.voiceA.tailScale);
    };

    const auto processVoiceB = [&] {
        routingEngine.prepareVoiceBInput (dryBlock, voiceABlock, voiceBBlock);
        if (routingEngine.delayInputGain() <= 0.0f)
            voiceBBlock.clear();
        delayVoiceB.process (voiceBBlock.getWritePointer (0),
                             voiceBBlock.getWritePointer (1),
                             numSamples, delayTargets[1],
                             parameters.timeGlideMs,
                             parameters.voiceB.tailScale);
    };

    if (routingEngine.shouldProcessVoiceAFirst())
    {
        processVoiceA();
        processVoiceB();
    }
    else
    {
        processVoiceB();
        processVoiceA();
    }

    // Wet routing/topology, then stereo placement, then mandatory safety.
    routingEngine.processWet (voiceABlock, voiceBBlock, wetBlock);

    stereoProcessor.setWidth (parameters.macroWidthPercent * 0.02f);
    stereoProcessor.setLowFrequencyMonoEnabled (parameters.lfMono);
    stereoProcessor.setLowFrequencyMonoCrossover (parameters.lfMonoFrequencyHz);
    stereoProcessor.process (wetBlock.getWritePointer (0),
                             wetBlock.getWritePointer (1),
                             numSamples);

    safetyProcessor.setLimiterEnabled (parameters.safetyLimiter);
    safetyProcessor.processBlock (wetBlock);
    const auto safeFeedbackCeiling = safetyProcessor.applyFeedbackCeiling (
        shredspace::SafetyProcessor::kMaximumFeedbackGain);
    delayVoiceA.setFeedbackGainCeiling (safeFeedbackCeiling);
    delayVoiceB.setFeedbackGainCeiling (safeFeedbackCeiling);

    if (safetyProcessor.consumeBufferClearRequest()
        || routingEngine.consumeTailClearRequest())
        clearDelayBuffersAtNextBlock = true;

    // Global Insert/Send mix follows all wet processing.
    routingEngine.mix (dryBlock, wetBlock, mixBlock);

    // Output gain follows the mix and uses the same 20 ms dB-domain rule.
    const float outputGain = std::pow (10.0f, parameters.outputGainDb / 20.0f);
    mixBlock.applyGain (outputGain);

    if (totalNumOutputChannels == 1)
    {
        for (int sample = 0; sample < numSamples; ++sample)
            buffer.setSample (0, sample,
                              0.5f * (mixBlock.getSample (0, sample)
                                      + mixBlock.getSample (1, sample)));
    }
    else
    {
        buffer.copyFrom (0, 0, mixBlock, 0, 0, numSamples);
        buffer.copyFrom (1, 0, mixBlock, 1, 0, numSamples);
    }

    // A host should not exceed the maximum block size supplied to prepare,
    // but any excess is deterministic silence rather than stale output.
    for (int channel = 0; channel < totalNumOutputChannels; ++channel)
        if (buffer.getNumSamples() > numSamples)
            buffer.clear (channel, numSamples, buffer.getNumSamples() - numSamples);

    // --- Telemetry Publication ---
    {
        const float lPeak = buffer.getMagnitude (0, 0, numSamples);
        const float rPeak = buffer.getNumChannels() > 1 ? buffer.getMagnitude (1, 0, numSamples) : lPeak;
        const float lRms  = buffer.getRMSLevel (0, 0, numSamples);
        const float rRms  = buffer.getNumChannels() > 1 ? buffer.getRMSLevel (1, 0, numSamples) : lRms;

        telemetry.setLeftPeak (lPeak);
        telemetry.setRightPeak (rPeak);
        telemetry.setLeftRms (lRms);
        telemetry.setRightRms (rRms);

        telemetry.setActivity (sustainEstimator.getActivity());
        telemetry.setDensity (densityEstimator.getDensity());
        telemetry.setSustain (sustainEstimator.getSustain());
        telemetry.setContraction (adaptiveController.getContraction());
        telemetry.setBloom (adaptiveController.getBloom());
        telemetry.setCorrelation (stereoProcessor.getCorrelation());
        telemetry.setSafetyEngaged (safetyProcessor.safetyInterventionOccurred());

        shredspace::HistorySample historySample;
        historySample.onset = onsetDetector.getSmoothedOnsetActivity();
        historySample.density = densityEstimator.getDensity();
        historySample.sustain = sustainEstimator.getSustain();
        historySample.gap = sustainEstimator.getPhraseGap();
        telemetry.pushHistory (historySample);
    }
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
