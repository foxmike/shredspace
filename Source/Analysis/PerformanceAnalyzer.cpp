#include "Analysis/PerformanceAnalyzer.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>

namespace shredspace
{

void PerformanceAnalyzer::prepare (double hostSampleRate, int maxBlockSize)
{
    const int preparedBlockSize = std::max (1, maxBlockSize);

    // Host-rate staging for the per-block mono energy signal.
    energyStagingBuffer_.assign (static_cast<std::size_t> (preparedBlockSize), 0.0f);

    // Size the fixed-rate analysis buffer to safely hold the worst-case
    // number of samples the resampler can emit for one host block. The
    // ratio is bounded by analysisSampleRate / 44100.0 (the lowest common
    // host rate); a guard and margin mirror the resampler's own sizing.
    constexpr double worstCaseRatio = analysisSampleRate / 44100.0;
    constexpr int interpolationGuardSamples = 4;
    constexpr int safetyMarginSamples = 8;
    const int maximumAnalysisSamples =
        static_cast<int> (std::ceil (static_cast<double> (preparedBlockSize)
                                    * worstCaseRatio))
        + interpolationGuardSamples + safetyMarginSamples;
    analysisBuffer_.assign (static_cast<std::size_t> (maximumAnalysisSamples), 0.0f);

    // Envelope coefficients are computed against the fixed analysis rate,
    // never the host rate, so they do not vary with host sample rate.
    alphaFast_ = std::exp (-1.0 / (tauFast * analysisSampleRate));
    alphaSlow_ = std::exp (-1.0 / (tauSlow * analysisSampleRate));

    resampler_.prepare (hostSampleRate, preparedBlockSize);

    reset();
}

void PerformanceAnalyzer::reset() noexcept
{
    std::fill (energyStagingBuffer_.begin(), energyStagingBuffer_.end(), 0.0f);
    std::fill (analysisBuffer_.begin(), analysisBuffer_.end(), 0.0f);

    fastEnvelope_ = 0.0f;
    slowEnvelope_ = 0.0f;
    timeNovelty_ = 0.0f;
    numAnalysisSamples_ = 0;

    resampler_.reset();
}

void PerformanceAnalyzer::process (const float* left,
                                   const float* right,
                                   int numChannels,
                                   int numSamples) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    numAnalysisSamples_ = 0;

    if (numSamples <= 0 || energyStagingBuffer_.empty() || analysisBuffer_.empty())
        return;

    const int inputCount = std::min (
        numSamples, static_cast<int> (energyStagingBuffer_.size()));

    // Build the analysis-band mono energy signal at the host rate.
    if (numChannels >= 2 && left != nullptr && right != nullptr)
    {
        for (int sample = 0; sample < inputCount; ++sample)
        {
            const double l = static_cast<double> (left[sample]);
            const double r = static_cast<double> (right[sample]);
            const double energy = std::sqrt ((l * l + r * r) * 0.5
                                             + stereoEnergyEpsilon);
            energyStagingBuffer_[static_cast<std::size_t> (sample)] =
                static_cast<float> (energy);
        }
    }
    else if (numChannels == 1 && left != nullptr)
    {
        for (int sample = 0; sample < inputCount; ++sample)
            energyStagingBuffer_[static_cast<std::size_t> (sample)] =
                std::abs (left[sample]);
    }
    else if (left != nullptr)
    {
        for (int sample = 0; sample < inputCount; ++sample)
            energyStagingBuffer_[static_cast<std::size_t> (sample)] =
                std::abs (left[sample]);
    }
    else
    {
        return;
    }

    // Resample to the fixed 48 kHz analysis rate through the owned resampler.
    const int produced = resampler_.process (energyStagingBuffer_.data(),
                                             inputCount,
                                             analysisBuffer_.data(),
                                             static_cast<int> (analysisBuffer_.size()));
    numAnalysisSamples_ = std::min (produced, static_cast<int> (analysisBuffer_.size()));

    // Update envelopes and novelty per analysis sample at the fixed rate.
    const double alphaFast = alphaFast_;
    const double alphaSlow = alphaSlow_;
    double fast = static_cast<double> (fastEnvelope_);
    double slow = static_cast<double> (slowEnvelope_);

    for (int sample = 0; sample < numAnalysisSamples_; ++sample)
    {
        const double energy = static_cast<double> (analysisBuffer_[static_cast<std::size_t> (sample)]);

        fast = alphaFast * fast + (1.0 - alphaFast) * energy;
        slow = alphaSlow * slow + (1.0 - alphaSlow) * energy;

        const float ratio = static_cast<float> ((fast + noveltyEps) / (slow + noveltyEps));
        const float novelty = std::max (0.0f, 20.0f * std::log10 (ratio));

        fastEnvelope_ = static_cast<float> (fast);
        slowEnvelope_ = static_cast<float> (slow);
        timeNovelty_ = novelty;
    }
}

} // namespace shredspace
