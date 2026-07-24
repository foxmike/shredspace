#include "DSP/RoutingEngine.h"

namespace shredspace
{

void RoutingEngine::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    sampleRate_ = std::isfinite (sampleRate) ? std::max (1.0, sampleRate) : 1.0;
    maxBlockSize_ = std::max (1, maxBlockSize);
    numChannels_ = std::max (1, numChannels);

    mixSmoother_.reset (sampleRate_, 0.020);     // 20 ms mix
    cascadeSmoother_.reset (sampleRate_, 0.030); // 30 ms cascade
    levelASmoother_.reset (sampleRate_, 0.030);  // 30 ms level A
    levelBSmoother_.reset (sampleRate_, 0.030);  // 30 ms level B

    totalRampSamples_ = static_cast<int> (std::round (sampleRate_ * 0.030)); // 30 ms topology crossfade
    totalRampSamples_ = std::max (1, totalRampSamples_);

    // All heap allocation must be done here in prepare()
    smoothedMix_.resize (maxBlockSize_);
    smoothedCascade_.resize (maxBlockSize_);
    smoothedLevelA_.resize (maxBlockSize_);
    smoothedLevelB_.resize (maxBlockSize_);
    fadeOldRamp_.resize (maxBlockSize_);
    fadeNewRamp_.resize (maxBlockSize_);

    crossfadeScratch_.setSize (numChannels_, maxBlockSize_, false, true, true);
    voiceAStaging_.setSize (numChannels_, maxBlockSize_, false, true, true);
    voiceBStaging_.setSize (numChannels_, maxBlockSize_, false, true, true);

    reset();
}

void RoutingEngine::reset() noexcept
{
    mixSmoother_.setCurrentAndTargetValue (mixSmoother_.getTargetValue());
    cascadeSmoother_.setCurrentAndTargetValue (cascadeSmoother_.getTargetValue());
    levelASmoother_.setCurrentAndTargetValue (levelASmoother_.getTargetValue());
    levelBSmoother_.setCurrentAndTargetValue (levelBSmoother_.getTargetValue());

    transitionState_ = TransitionState::idle;
    activeTopology_ = targetTopology_;
    sourceTopology_ = targetTopology_;
    rampCounter_ = 0;

    smoothersUpdatedForBlock_ = false;
}

void RoutingEngine::setTopology (Topology topology) noexcept
{
    if (topology == targetTopology_)
        return;

    sourceTopology_ = activeTopology_;
    targetTopology_ = topology;
    rampCounter_ = 0;
    transitionState_ = TransitionState::fading;
}

void RoutingEngine::setCascade (float cascade) noexcept
{
    cascadeSmoother_.setTargetValue (std::clamp (cascade, 0.0f, 1.0f));
}

void RoutingEngine::setVoiceLevels (float levelA, float levelB) noexcept
{
    levelASmoother_.setTargetValue (std::clamp (levelA, 0.0f, 1.0f));
    levelBSmoother_.setTargetValue (std::clamp (levelB, 0.0f, 1.0f));
}

void RoutingEngine::setMixMode (MixMode mode) noexcept
{
    mixMode_ = mode;
}

void RoutingEngine::setMix (float mix) noexcept
{
    mixSmoother_.setTargetValue (std::clamp (mix, 0.0f, 1.0f));
}

bool RoutingEngine::shouldProcessVoiceAFirst() const noexcept
{
    if (targetTopology_ == Topology::BtoA)
        return false;
    return true;
}

void RoutingEngine::updateBlockState (int numSamples) noexcept
{
    if (smoothersUpdatedForBlock_)
        return;

    const int limit = std::min (numSamples, maxBlockSize_);

    for (int i = 0; i < limit; ++i)
    {
        smoothedMix_[i] = mixSmoother_.getNextValue();
        smoothedCascade_[i] = cascadeSmoother_.getNextValue();
        smoothedLevelA_[i] = levelASmoother_.getNextValue();
        smoothedLevelB_[i] = levelBSmoother_.getNextValue();

        if (transitionState_ == TransitionState::fading)
        {
            float progress = static_cast<float> (rampCounter_) / static_cast<float> (totalRampSamples_);
            float angle = progress * (std::numbers::pi_v<float> / 2.0f);
            fadeOldRamp_[i] = std::cos (angle);
            fadeNewRamp_[i] = std::sin (angle);

            rampCounter_++;
            if (rampCounter_ >= totalRampSamples_)
            {
                transitionState_ = TransitionState::idle;
                activeTopology_ = targetTopology_;
                sourceTopology_ = targetTopology_;

                // Fill the rest of the block at target values
                for (int j = i + 1; j < limit; ++j)
                {
                    fadeOldRamp_[j] = 1.0f;
                    fadeNewRamp_[j] = 0.0f;
                }
                break;
            }
        }
        else
        {
            fadeOldRamp_[i] = 1.0f;
            fadeNewRamp_[i] = 0.0f;
        }
    }

    // In case numSamples > maxBlockSize (which shouldn't happen if properly configured, but let's be safe)
    if (numSamples > limit)
    {
        for (int i = limit; i < numSamples; ++i)
        {
            smoothedMix_[i] = mixSmoother_.getNextValue();
            smoothedCascade_[i] = cascadeSmoother_.getNextValue();
            smoothedLevelA_[i] = levelASmoother_.getNextValue();
            smoothedLevelB_[i] = levelBSmoother_.getNextValue();
            fadeOldRamp_[i] = 1.0f;
            fadeNewRamp_[i] = 0.0f;
        }
    }

    smoothersUpdatedForBlock_ = true;
}

void RoutingEngine::prepareVoiceAInput (const juce::AudioBuffer<float>& dryBuffer,
                                       const juce::AudioBuffer<float>& voiceBOutputBuffer,
                                       juce::AudioBuffer<float>& voiceAInputBuffer) noexcept
{
    const int numSamples = dryBuffer.getNumSamples();
    const int numChannels = dryBuffer.getNumChannels();
    updateBlockState (numSamples);

    const int limit = std::min (numSamples, maxBlockSize_);
    constexpr float pi_2 = std::numbers::pi_v<float> / 2.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* dry = dryBuffer.getReadPointer (ch);
        const float* voiceBOut = voiceBOutputBuffer.getReadPointer (ch);
        float* voiceAIn = voiceAInputBuffer.getWritePointer (ch);

        for (int i = 0; i < limit; ++i)
        {
            const float cascadeVal = smoothedCascade_[i];
            const float cos_c = std::cos (cascadeVal * pi_2);
            const float sin_c = std::sin (cascadeVal * pi_2);

            // Compute input under source topology
            float in_source = dry[i];
            if (sourceTopology_ == Topology::BtoA)
                in_source = cos_c * dry[i] + sin_c * voiceBOut[i];

            // Compute input under target topology
            float in_target = dry[i];
            if (targetTopology_ == Topology::BtoA)
                in_target = cos_c * dry[i] + sin_c * voiceBOut[i];

            // Blend based on transition ramp
            voiceAIn[i] = fadeOldRamp_[i] * in_source + fadeNewRamp_[i] * in_target;
        }

        // Fill safety limit excess if any
        for (int i = limit; i < numSamples; ++i)
            voiceAIn[i] = dry[i];
    }
}

void RoutingEngine::prepareVoiceBInput (const juce::AudioBuffer<float>& dryBuffer,
                                       const juce::AudioBuffer<float>& voiceAOutputBuffer,
                                       juce::AudioBuffer<float>& voiceBInputBuffer) noexcept
{
    const int numSamples = dryBuffer.getNumSamples();
    const int numChannels = dryBuffer.getNumChannels();
    updateBlockState (numSamples);

    const int limit = std::min (numSamples, maxBlockSize_);
    constexpr float pi_2 = std::numbers::pi_v<float> / 2.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* dry = dryBuffer.getReadPointer (ch);
        const float* voiceAOut = voiceAOutputBuffer.getReadPointer (ch);
        float* voiceBIn = voiceBInputBuffer.getWritePointer (ch);

        for (int i = 0; i < limit; ++i)
        {
            const float cascadeVal = smoothedCascade_[i];
            const float cos_c = std::cos (cascadeVal * pi_2);
            const float sin_c = std::sin (cascadeVal * pi_2);

            // Compute input under source topology
            float in_source = dry[i];
            if (sourceTopology_ == Topology::AtoB)
                in_source = cos_c * dry[i] + sin_c * voiceAOut[i];

            // Compute input under target topology
            float in_target = dry[i];
            if (targetTopology_ == Topology::AtoB)
                in_target = cos_c * dry[i] + sin_c * voiceAOut[i];

            // Blend based on transition ramp
            voiceBIn[i] = fadeOldRamp_[i] * in_source + fadeNewRamp_[i] * in_target;
        }

        // Fill safety limit excess if any
        for (int i = limit; i < numSamples; ++i)
            voiceBIn[i] = dry[i];
    }
}

void RoutingEngine::process (const juce::AudioBuffer<float>& dryBuffer,
                             juce::AudioBuffer<float>& voiceAWetBuffer,
                             juce::AudioBuffer<float>& voiceBWetBuffer,
                             juce::AudioBuffer<float>& outputBuffer) noexcept
{
    const int numSamples = dryBuffer.getNumSamples();
    const int numChannels = dryBuffer.getNumChannels();
    updateBlockState (numSamples);

    const int limit = std::min (numSamples, maxBlockSize_);
    constexpr float pi_2 = std::numbers::pi_v<float> / 2.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* dry = dryBuffer.getReadPointer (ch);
        const float* voiceAWet = voiceAWetBuffer.getReadPointer (ch);
        const float* voiceBWet = voiceBWetBuffer.getReadPointer (ch);
        float* out = outputBuffer.getWritePointer (ch);

        for (int i = 0; i < limit; ++i)
        {
            const float levelA = smoothedLevelA_[i];
            const float levelB = smoothedLevelB_[i];
            const float mixVal = smoothedMix_[i];

            // 1. Combine voice wet signals under wet voice normalization
            const float N = std::sqrt (std::max (1.0f, levelA * levelA + levelB * levelB));
            const float wet_source = (levelA * voiceAWet[i] + levelB * voiceBWet[i]) / N;
            const float wet_target = (levelA * voiceAWet[i] + levelB * voiceBWet[i]) / N;

            // Apply dedicated wet-path crossfade ramp (even if they are identical in formula,
            // this formally maps to the crossfade behavior of the two topologies).
            const float wet = fadeOldRamp_[i] * wet_source + fadeNewRamp_[i] * wet_target;

            // 2. Global mixing
            float dryGain = 0.0f;
            float wetGain = 1.0f;

            if (mixMode_ == MixMode::Insert)
            {
                dryGain = std::cos (mixVal * pi_2);
                wetGain = std::sin (mixVal * pi_2);
            }

            out[i] = dryGain * dry[i] + wetGain * wet;
        }

        // Fill safety limit excess if any
        for (int i = limit; i < numSamples; ++i)
            out[i] = dry[i];
    }

    // Reset update state at end of block
    smoothersUpdatedForBlock_ = false;
}

} // namespace shredspace
