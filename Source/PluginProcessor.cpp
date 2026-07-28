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
    voiceStereoA.prepare (sampleRate, maximumBlockSize);
    voiceStereoB.prepare (sampleRate, maximumBlockSize);
    stereoProcessor.prepare (sampleRate, maximumBlockSize);
    safetyProcessor.prepare (sampleRate, maximumBlockSize, kProcessingChannels);

    performanceAnalyzer.prepare (sampleRate, maximumBlockSize);
    const int maxAnalysis = static_cast<int> (std::ceil (maximumBlockSize * (48000.0 / 44100.0))) + 64;
    onsetDetector.prepare (maxAnalysis);
    densityEstimator.prepare();
    sustainEstimator.prepare();
    adaptiveController.prepare();
    macroEngine.prepare (sampleRate, maximumBlockSize);

    // The engine evaluates fixed-size destination tables on the audio thread;
    // profile/scene controls only supply normalized macro positions.
    const std::array<shredspace::MacroDestination, 5> definitionTable {{
        { {}, 0.0f, 2.0f, 1.0f },   // voice wet dB
        { {}, 0.0f, 0.50f, 1.0f },  // HPF octaves
        { {}, 0.0f, 0.35f, 1.0f },  // LPF octaves
        { {}, 0.0f, 4.0f, 1.0f },   // presence cut dB
        { {}, 0.0f, -0.15f, 1.0f }  // attack softening
    }};
    const std::array<shredspace::MacroDestination, 3> bloomTable {{
        { {}, 0.0f, 3.0f, 1.0f },   // voice wet dB
        { {}, 0.0f, 0.75f, 1.0f },  // T60 octaves
        { {}, 0.0f, 0.10f, 1.0f }   // diffusion
    }};
    const std::array<shredspace::MacroDestination, 5> focusTable {{
        { {}, 0.0f, -0.50f, 1.0f }, // width
        { {}, 0.0f, 0.40f, 1.0f },  // HPF octaves
        { {}, 0.0f, -0.30f, 1.0f }, // LPF octaves
        { {}, 0.0f, 3.0f, 1.0f },   // presence cut dB
        { {}, 0.0f, 0.20f, 1.0f }   // attack softening
    }};
    const std::array<shredspace::MacroDestination, 5> widthTable {{
        { {}, 0.0f, 0.35f, 1.0f },  // outward pan
        { {}, -0.35f, 0.60f, 1.0f },// width
        { {}, 0.0f, 6.0f, 1.0f },   // stereo offset ms
        { {}, 0.0f, 0.15f, 1.0f },  // cross feedback
        { {}, 0.0f, 45.0f, 1.0f }   // modulation phase
    }};
    const std::array<shredspace::MacroDestination, 6> characterTable {{
        { {}, 0.0f, 0.45f, 1.0f },  // saturation
        { {}, 0.0f, 1.50f, 1.0f },  // modulation depth ms
        { {}, 0.0f, 0.25f, 1.0f },  // drift
        { {}, 0.0f, 0.12f, 1.0f },  // diffusion
        { {}, 0.0f, 0.15f, 1.0f },  // attack softening
        { {}, 0.0f, -0.35f, 1.0f }  // LPF octaves
    }};
    macroEngine.setDestinationTable (shredspace::MacroId::DEFINITION, definitionTable);
    macroEngine.setDestinationTable (shredspace::MacroId::BLOOM, bloomTable);
    macroEngine.setDestinationTable (shredspace::MacroId::FOCUS, focusTable);
    macroEngine.setDestinationTable (shredspace::MacroId::WIDTH, widthTable);
    macroEngine.setDestinationTable (shredspace::MacroId::CHARACTER, characterTable);

    // Built-in scene snapshots remain internal deltas: APVTS base values are
    // never rewritten by morphing.
    std::array<shredspace::SceneValues, shredspace::kSceneCount> scenes {};
    scenes[0].macroTargets = { 0.35f, 0.25f, 0.80f, 0.25f, 0.50f, 0.80f, 0.35f, 0.20f };
    scenes[1].macroTargets = { 0.50f, 0.55f, 0.65f, 0.50f, 0.60f, 0.55f, 0.65f, 0.40f };
    scenes[2].macroTargets = { 0.60f, 0.80f, 0.45f, 0.90f, 0.80f, 0.35f, 0.90f, 0.55f };
    scenes[3].macroTargets = { 0.45f, 0.65f, 0.55f, 0.70f, 0.70f, 0.45f, 0.75f, 0.85f };
    scenes[0].mixTarget = 0.18f; scenes[0].routingTarget = 0;
    scenes[1].mixTarget = 0.25f; scenes[1].routingTarget = 0;
    scenes[2].mixTarget = 0.32f; scenes[2].routingTarget = 1;
    scenes[3].mixTarget = 0.28f; scenes[3].routingTarget = 2;
    sceneManager.registerEngines (&routingEngine, &delayTimeController);
    sceneManager.setSceneValues (scenes);
    sceneManager.prepare (sampleRate, maximumBlockSize);
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

    const auto normalizedPercent = [] (float percent) noexcept {
        return std::clamp (percent * 0.01f, 0.0f, 1.0f);
    };
    const float sceneWeight = normalizedPercent (parameters.sceneMorphPercent);
    sceneManager.setMorphPosition (parameters.sceneSelect,
                                   parameters.sceneMorphPercent);

    const std::array<float, 8> baseMacros {
        std::clamp ((parameters.macroTimeMs - 20.0f) / 1980.0f, 0.0f, 1.0f),
        normalizedPercent (parameters.macroSpacePercent),
        normalizedPercent (parameters.macroDefinitionPercent),
        normalizedPercent (parameters.macroBloomPercent),
        std::clamp ((parameters.macroTailSeconds - 0.15f) / 11.85f, 0.0f, 1.0f),
        normalizedPercent (parameters.macroFocusPercent),
        normalizedPercent (parameters.macroWidthPercent),
        normalizedPercent (parameters.macroCharacterPercent)
    };
    std::array<float, 8> effectiveMacros {};
    for (std::size_t macroIndex = 0; macroIndex < effectiveMacros.size(); ++macroIndex)
    {
        const auto macro = static_cast<shredspace::MacroId> (macroIndex);
        effectiveMacros[macroIndex] = std::clamp (
            baseMacros[macroIndex]
                + sceneWeight * sceneManager.getSceneDelta (
                    macro, baseMacros[macroIndex]),
            0.0f, 1.0f);
        macroEngine.setMacroValue (macro, effectiveMacros[macroIndex]);
    }
    macroEngine.processDeltas (numSamples);

    const float effectiveMacroTimeMs = std::clamp (
        parameters.macroTimeMs
            * std::exp2 (2.0f * (effectiveMacros[0] - baseMacros[0])),
        20.0f, 2000.0f);
    const float effectiveMacroTailSeconds = std::clamp (
        parameters.macroTailSeconds
            * std::exp2 (4.0f * (effectiveMacros[4] - baseMacros[4])),
        0.15f, 12.0f);

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

        // Higher user sensitivity lowers the detector's internal score bias.
        onsetDetector.setThreshold (
            1.0f - parameters.intelligence.sensitivityPercent * 0.01f);
        onsetDetector.setNoiseGateDb (parameters.intelligence.inputGateDb);

        adaptiveController.setBloomAmount (effectiveMacros[3]);
        adaptiveController.setDefinition (effectiveMacros[2]);
        adaptiveController.setTimeConstants (
            parameters.intelligence.contractionAttackMs,
            parameters.intelligence.contractionReleaseMs,
            parameters.intelligence.bloomRiseMs,
            parameters.intelligence.bloomHoldMs,
            parameters.intelligence.bloomReleaseMs);

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
    timingInput.macroTimeMs = effectiveMacroTimeMs;
    timingInput.timeGlideMs = parameters.timeGlideMs;
    timingInput.voiceA.timeRatio = parameters.voiceA.timeRatio;
    timingInput.voiceA.fineOffsetMs = parameters.voiceA.fineOffsetMs;
    timingInput.voiceB.timeRatio = parameters.voiceB.timeRatio;
    timingInput.voiceB.fineOffsetMs = parameters.voiceB.fineOffsetMs;
    timingInput.sceneIndex = parameters.sceneSelect;
    const auto delayTargets = delayTimeController.update (timingInput, hostTiming);

    const int effectiveRouting =
        sceneWeight >= 0.5f
            ? sceneManager.getActiveRouting()
            : parameters.routing;
    const auto topology = static_cast<shredspace::RoutingEngine::Topology> (
        std::clamp (effectiveRouting, 0, 2));
    routingEngine.setTopology (topology);
    routingEngine.setCascade (parameters.routingCascadePercent * 0.01f);
    const float baseMix = parameters.mixPercent * 0.01f;
    routingEngine.setMix (std::clamp (
        baseMix + sceneWeight * sceneManager.getMixDelta (baseMix),
        0.0f, 1.0f));
    routingEngine.setMixMode (
        parameters.mixMode == 0 ? shredspace::RoutingEngine::MixMode::Insert
                                : shredspace::RoutingEngine::MixMode::Send);
    routingEngine.setBypassMode (
        ! parameters.bypass
            ? shredspace::RoutingEngine::BypassMode::Off
            : (parameters.bypassMode == 0
                   ? shredspace::RoutingEngine::BypassMode::Hard
                   : shredspace::RoutingEngine::BypassMode::Trails));

    const auto adaptiveA = adaptiveController.getDeltasForVoice (
        parameters.voiceA.adaptiveRolePercent * 0.01f);
    const auto adaptiveB = adaptiveController.getDeltasForVoice (
        parameters.voiceB.adaptiveRolePercent * 0.01f);

    const float definitionWetDb =
        macroEngine.getMacroDelta (shredspace::MacroId::DEFINITION, 0);
    const float definitionHpfOct =
        macroEngine.getMacroDelta (shredspace::MacroId::DEFINITION, 1);
    const float definitionLpfOct =
        macroEngine.getMacroDelta (shredspace::MacroId::DEFINITION, 2);
    const float definitionGuardDb =
        macroEngine.getMacroDelta (shredspace::MacroId::DEFINITION, 3);
    const float definitionSoften =
        macroEngine.getMacroDelta (shredspace::MacroId::DEFINITION, 4);
    const float bloomWetDb =
        macroEngine.getMacroDelta (shredspace::MacroId::BLOOM, 0);
    const float bloomTailOct =
        macroEngine.getMacroDelta (shredspace::MacroId::BLOOM, 1);
    const float bloomDiffusion =
        macroEngine.getMacroDelta (shredspace::MacroId::BLOOM, 2);
    const float focusWidth =
        macroEngine.getMacroDelta (shredspace::MacroId::FOCUS, 0);
    const float focusHpfOct =
        macroEngine.getMacroDelta (shredspace::MacroId::FOCUS, 1);
    const float focusLpfOct =
        macroEngine.getMacroDelta (shredspace::MacroId::FOCUS, 2);
    const float focusGuardDb =
        macroEngine.getMacroDelta (shredspace::MacroId::FOCUS, 3);
    const float focusSoften =
        macroEngine.getMacroDelta (shredspace::MacroId::FOCUS, 4);
    const float widthPan =
        macroEngine.getMacroDelta (shredspace::MacroId::WIDTH, 0);
    const float widthAmount =
        macroEngine.getMacroDelta (shredspace::MacroId::WIDTH, 1);
    const float widthOffsetMs =
        macroEngine.getMacroDelta (shredspace::MacroId::WIDTH, 2);
    const float widthCrossFeedback =
        macroEngine.getMacroDelta (shredspace::MacroId::WIDTH, 3);
    const float widthPhase =
        macroEngine.getMacroDelta (shredspace::MacroId::WIDTH, 4);
    const float characterSaturation =
        macroEngine.getMacroDelta (shredspace::MacroId::CHARACTER, 0);
    const float characterModDepth =
        macroEngine.getMacroDelta (shredspace::MacroId::CHARACTER, 1);
    const float characterDrift =
        macroEngine.getMacroDelta (shredspace::MacroId::CHARACTER, 2);
    const float characterDiffusion =
        macroEngine.getMacroDelta (shredspace::MacroId::CHARACTER, 3);
    const float characterSoften =
        macroEngine.getMacroDelta (shredspace::MacroId::CHARACTER, 4);
    const float characterLpfOct =
        macroEngine.getMacroDelta (shredspace::MacroId::CHARACTER, 5);

    const auto configureVoice =
        [&] (shredspace::DelayVoice& voice,
             shredspace::StereoProcessor& stereo,
             const shredspace::ParameterSnapshot::VoiceValues& voiceValues,
             const shredspace::AdaptiveController::AdaptiveDeltas& adaptive,
             float panDirection) noexcept
    {
        const float wetDb = voiceValues.levelDb + definitionWetDb
                          + bloomWetDb + adaptive.wetDbDelta;
        const float wetGain = std::clamp (
            std::pow (10.0f, wetDb / 20.0f)
                * (0.75f + 0.5f * effectiveMacros[1]),
            0.0f, 1.0f);
        const float effectiveT60 = std::clamp (
            effectiveMacroTailSeconds
                * std::exp2 (bloomTailOct + adaptive.t60OctaveDelta),
            0.15f, 12.0f);
        const float hpf = std::clamp (
            voiceValues.hpfHz * std::exp2 (definitionHpfOct + focusHpfOct),
            40.0f, 1500.0f);
        const float lpf = std::clamp (
            voiceValues.lpfHz
                * std::exp2 (definitionLpfOct + focusLpfOct
                             + characterLpfOct + adaptive.lpfOctaveDelta),
            2000.0f, 20000.0f);
        const float soften = std::clamp (
            voiceValues.attackSoftenPercent * 0.01f
                + definitionSoften + focusSoften + characterSoften
                + adaptive.softeningDelta,
            0.0f, 1.0f);
        const float diffusion = std::clamp (
            voiceValues.diffusionPercent * 0.01f
                + bloomDiffusion + characterDiffusion
                + 0.08f * effectiveMacros[1] + adaptive.diffusionDelta,
            0.0f, 0.40f);

        voice.setT60Target (effectiveT60);
        voice.setHighPassFrequency (hpf);
        voice.setLowPassFrequency (lpf);
        voice.setPresenceGuard (
            voiceValues.presenceFrequencyHz,
            std::clamp (voiceValues.presenceGuardDb
                        + definitionGuardDb + focusGuardDb,
                        0.0f, 12.0f));
        voice.setAttackSoften (
            soften * 100.0f,
            parameters.intelligence.transientCrossoverHz);
        voice.setCrossFeedback (std::clamp (
            voiceValues.crossFeedbackPercent * 0.01f + widthCrossFeedback,
            0.0f, 1.0f));
        voice.setSaturationAmount (std::clamp (
            voiceValues.saturationPercent * 0.01f + characterSaturation,
            0.0f, 1.0f));
        voice.setDiffusionAmount (diffusion);
        voice.setModulation (
            voiceValues.modulationRateHz,
            std::clamp (voiceValues.modulationDepthMs + characterModDepth,
                        0.0f, 8.0f),
            std::clamp (voiceValues.modulationPhaseDegrees + widthPhase,
                        0.0f, 180.0f),
            std::clamp (voiceValues.driftPercent * 0.01f + characterDrift,
                        0.0f, 1.0f),
            std::clamp (voiceValues.stereoOffsetMs + widthOffsetMs,
                        0.0f, 30.0f));

        stereo.setPan (std::clamp (
            voiceValues.pan * 0.01f + panDirection * widthPan,
            -1.0f, 1.0f));
        stereo.setWidth (std::clamp (
            voiceValues.widthPercent * 0.01f + widthAmount + focusWidth
                + adaptive.widthDelta,
            0.0f, 2.0f));
        stereo.setLowFrequencyMonoEnabled (false);
        return wetGain;
    };

    const float voiceALevel = configureVoice (
        delayVoiceA, voiceStereoA, parameters.voiceA, adaptiveA, -1.0f);
    const float voiceBLevel = configureVoice (
        delayVoiceB, voiceStereoB, parameters.voiceB, adaptiveB, 1.0f);
    routingEngine.setVoiceLevels (
        parameters.voiceA.enabled ? voiceALevel : 0.0f,
        parameters.voiceB.enabled ? voiceBLevel : 0.0f);
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
        voiceStereoA.process (voiceABlock.getWritePointer (0),
                              voiceABlock.getWritePointer (1),
                              numSamples);
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
        voiceStereoB.process (voiceBBlock.getWritePointer (0),
                              voiceBBlock.getWritePointer (1),
                              numSamples);
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

    stereoProcessor.setWidth (effectiveMacros[6] * 2.0f);
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
