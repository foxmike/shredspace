#include "DSP/SafetyProcessor.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace shredspace
{

void SafetyProcessor::prepare (double sampleRate,
                               int maxBlockSize,
                               int numChannels)
{
    const auto validSampleRate =
        std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 44100.0;

    maxBlockSize_ = std::max (0, maxBlockSize);
    preparedChannels_ = std::max (0, numChannels);
    previousInput_.assign (static_cast<std::size_t> (preparedChannels_), 0.0);
    previousOutput_.assign (static_cast<std::size_t> (preparedChannels_), 0.0);

    attackAlpha_ =
        std::exp (-1.0 / (kAttackSeconds * validSampleRate));
    releaseAlpha_ =
        std::exp (-1.0 / (kReleaseSeconds * validSampleRate));

    reset();
}

void SafetyProcessor::reset() noexcept
{
    resetLightweightState();
    safetyIntervention_.store (false, std::memory_order_release);
    bufferClearRequested_.store (false, std::memory_order_release);
}

void SafetyProcessor::setLimiterEnabled (bool shouldBeEnabled) noexcept
{
    limiterEnabled_ = shouldBeEnabled;
}

void SafetyProcessor::processBlock (
    juce::AudioBuffer<float>& wetBuffer) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    const auto numChannels = wetBuffer.getNumChannels();
    const auto numSamples = wetBuffer.getNumSamples();

    if (numChannels > preparedChannels_ || numSamples > maxBlockSize_)
    {
        muteAndPublishInvalidState (wetBuffer);
        return;
    }

    for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
    {
        double instantaneousEnergy2 = 0.0;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* samples = wetBuffer.getWritePointer (channel);
            const auto input = static_cast<double> (samples[sampleIndex]);

            if (! std::isfinite (input))
            {
                muteAndPublishInvalidState (wetBuffer);
                return;
            }

            const auto dcOutput =
                input - previousInput_[static_cast<std::size_t> (channel)]
                + kDcBlockerR
                    * previousOutput_[static_cast<std::size_t> (channel)];

            if (! std::isfinite (dcOutput)
                || std::abs (dcOutput)
                    > static_cast<double> (
                        std::numeric_limits<float>::max()))
            {
                muteAndPublishInvalidState (wetBuffer);
                return;
            }

            const auto safeInput = flushDenormal (input);
            const auto safeOutput = flushDenormal (dcOutput);
            previousInput_[static_cast<std::size_t> (channel)] = safeInput;
            previousOutput_[static_cast<std::size_t> (channel)] = safeOutput;
            samples[sampleIndex] = static_cast<float> (safeOutput);

            instantaneousEnergy2 =
                std::max (instantaneousEnergy2, safeOutput * safeOutput);
        }

        const auto alpha = instantaneousEnergy2 > feedbackEnergy2_
                               ? attackAlpha_
                               : releaseAlpha_;
        feedbackEnergy2_ =
            alpha * feedbackEnergy2_
            + (1.0 - alpha) * instantaneousEnergy2;

        if (! std::isfinite (feedbackEnergy2_)
            || feedbackEnergy2_ < 0.0)
        {
            muteAndPublishInvalidState (wetBuffer);
            return;
        }

        feedbackEnergy2_ = flushDenormal (feedbackEnergy2_);
    }
}

float SafetyProcessor::applyFeedbackCeiling (float requestedGain) noexcept
{
    if (! std::isfinite (requestedGain)
        || ! std::isfinite (feedbackEnergy2_)
        || feedbackEnergy2_ < 0.0)
    {
        resetLightweightState();
        publishInvalidState();
        return 0.0f;
    }

    double reduction = 1.0;

    if (limiterEnabled_)
    {
        const auto energy =
            std::sqrt (std::max (feedbackEnergy2_, 0.0) + kEnergyEpsilon);
        reduction = std::min (
            1.0,
            static_cast<double> (kLimiterThreshold)
                / (energy + kEnergyEpsilon));
    }

    const auto safeGain = std::min (
        static_cast<double> (kMaximumFeedbackGain),
        static_cast<double> (requestedGain) * reduction);

    if (! std::isfinite (safeGain))
    {
        resetLightweightState();
        publishInvalidState();
        return 0.0f;
    }

    return static_cast<float> (safeGain);
}

bool SafetyProcessor::safetyInterventionOccurred() const noexcept
{
    return safetyIntervention_.load (std::memory_order_acquire);
}

bool SafetyProcessor::consumeBufferClearRequest() noexcept
{
    return bufferClearRequested_.exchange (false, std::memory_order_acq_rel);
}

double SafetyProcessor::flushDenormal (double value) noexcept
{
    return std::abs (value) < kDenormalThreshold ? 0.0 : value;
}

void SafetyProcessor::resetLightweightState() noexcept
{
    std::fill (previousInput_.begin(), previousInput_.end(), 0.0);
    std::fill (previousOutput_.begin(), previousOutput_.end(), 0.0);
    feedbackEnergy2_ = 0.0;
}

void SafetyProcessor::publishInvalidState() noexcept
{
    safetyIntervention_.store (true, std::memory_order_release);
    bufferClearRequested_.store (true, std::memory_order_release);
}

void SafetyProcessor::muteAndPublishInvalidState (
    juce::AudioBuffer<float>& wetBuffer) noexcept
{
    wetBuffer.clear();
    resetLightweightState();
    publishInvalidState();
}

} // namespace shredspace
