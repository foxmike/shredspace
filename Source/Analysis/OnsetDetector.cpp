#include "Analysis/OnsetDetector.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace shredspace
{

namespace
{
constexpr double pi = 3.1415926535897932384626433832795;
constexpr double minimumFluxFrequency = 150.0;
constexpr double maximumFluxFrequency = 8000.0;
constexpr double binSpacing = OnsetDetector::analysisSampleRate / OnsetDetector::fftSize;
constexpr double normalizationTimeSeconds = 1.0;
constexpr double activitySmoothingTimeSeconds = 0.050;
} // namespace

void OnsetDetector::prepare (const int maxAnalysisBlockSamples)
{
    fft_ = std::make_unique<juce::dsp::FFT> (fftOrder);

    inputRing_.assign (fftSize, 0.0f);
    window_.resize (fftSize);
    fftData_.assign (2 * fftSize, 0.0f);
    magnitudes_.assign (fftSize / 2 + 1, 0.0f);
    previousMagnitudes_.assign (fftSize / 2 + 1, 0.0f);
    binWeights_.assign (fftSize / 2 + 1, 0.0f);

    for (int n = 0; n < fftSize; ++n)
    {
        window_[static_cast<std::size_t> (n)] = static_cast<float> (
            0.5 * (1.0 - std::cos (2.0 * pi * static_cast<double> (n)
                                   / static_cast<double> (fftSize - 1))));
    }

    firstFluxBin_ = static_cast<int> (std::ceil (minimumFluxFrequency / binSpacing));
    lastFluxBin_ = static_cast<int> (std::floor (maximumFluxFrequency / binSpacing));
    numFluxBins_ = lastFluxBin_ - firstFluxBin_ + 1;

    // A neutral per-bin weighting preserves the specified log-flux scale while
    // keeping the weighting table ready for later calibration.
    for (int k = firstFluxBin_; k <= lastFluxBin_; ++k)
        binWeights_[static_cast<std::size_t> (k)] = 1.0f;

    const auto maximumFrames = std::max (1, maxAnalysisBlockSamples / hopSize + 2);
    acceptedOnsets_.resize (static_cast<std::size_t> (maximumFrames));

    const auto frameInterval = static_cast<double> (hopSize) / analysisSampleRate;
    normalizationCoefficient_ = static_cast<float> (
        1.0 - std::exp (-frameInterval / normalizationTimeSeconds));
    activityPole_ = static_cast<float> (
        std::exp (-1.0 / (analysisSampleRate * activitySmoothingTimeSeconds)));

    reset();
}

void OnsetDetector::reset() noexcept
{
    std::fill (inputRing_.begin(), inputRing_.end(), 0.0f);
    std::fill (fftData_.begin(), fftData_.end(), 0.0f);
    std::fill (magnitudes_.begin(), magnitudes_.end(), 0.0f);
    std::fill (previousMagnitudes_.begin(), previousMagnitudes_.end(), 0.0f);

    writePosition_ = 0;
    samplesUntilNextFrame_ = fftSize;
    numAcceptedOnsets_ = 0;
    samplesSinceLastOnset_ = refractorySamples;
    totalAnalysisSamples_ = 0;
    lastAcceptedSampleIndex_ = 0;
    hasPreviousSpectrum_ = false;
    hasAcceptedOnset_ = false;

    timeFeature_ = {};
    fluxFeature_ = {};
    energyFeature_ = {};
    olderScoreFrame_ = {};
    previousScoreFrame_ = {};
    scoreHistorySize_ = 0;

    spectralFlux_ = 0.0f;
    onsetScore_ = 0.0f;
    smoothedOnsetActivity_ = 0.0f;
}

void OnsetDetector::process (const float* const analysisSamples,
                             const int numAnalysisSamples,
                             const float timeNovelty,
                             const float slowEnvelopeDb) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    numAcceptedOnsets_ = 0;

    if (analysisSamples == nullptr || numAnalysisSamples <= 0 || fft_ == nullptr)
        return;

    for (int sample = 0; sample < numAnalysisSamples; ++sample)
    {
        inputRing_[static_cast<std::size_t> (writePosition_)] = analysisSamples[sample];
        writePosition_ = (writePosition_ + 1) % fftSize;
        ++totalAnalysisSamples_;

        if (samplesSinceLastOnset_ < std::numeric_limits<int>::max())
            ++samplesSinceLastOnset_;

        bool acceptedNow = false;
        if (--samplesUntilNextFrame_ == 0)
        {
            acceptedNow = processFrame (timeNovelty, slowEnvelopeDb);
            samplesUntilNextFrame_ = hopSize;
        }

        smoothedOnsetActivity_ *= activityPole_;
        if (acceptedNow)
            smoothedOnsetActivity_ += 1.0f - activityPole_;
    }
}

float OnsetDetector::normalizeFeature (const float value,
                                       AdaptiveFeature& feature) noexcept
{
    if (! feature.initialized)
    {
        feature.mean = value;
        feature.deviation = 0.0f;
        feature.initialized = true;
        return 0.0f;
    }

    const auto difference = value - feature.mean;
    const auto z = std::max (0.0f, difference / (feature.deviation + epsilon));

    feature.mean += normalizationCoefficient_ * difference;
    feature.deviation += normalizationCoefficient_
                          * (std::abs (difference) - feature.deviation);
    return z;
}

bool OnsetDetector::processFrame (const float timeNovelty,
                                  const float slowEnvelopeDb) noexcept
{
    for (int n = 0; n < fftSize; ++n)
    {
        const auto ringIndex = (writePosition_ + n) % fftSize;
        fftData_[static_cast<std::size_t> (n)]
            = inputRing_[static_cast<std::size_t> (ringIndex)]
              * window_[static_cast<std::size_t> (n)];
    }
    std::fill (fftData_.begin() + fftSize, fftData_.end(), 0.0f);

    fft_->performFrequencyOnlyForwardTransform (fftData_.data(), true);
    std::copy_n (fftData_.begin(), fftSize / 2 + 1, magnitudes_.begin());

    double fluxSum = 0.0;
    if (hasPreviousSpectrum_)
    {
        for (int k = firstFluxBin_; k <= lastFluxBin_; ++k)
        {
            const auto index = static_cast<std::size_t> (k);
            const auto logDifference = std::log (magnitudes_[index] + epsilon)
                                     - std::log (previousMagnitudes_[index] + epsilon);
            fluxSum += static_cast<double> (binWeights_[index])
                       * std::max (0.0, static_cast<double> (logDifference));
        }
    }

    spectralFlux_ = hasPreviousSpectrum_
                        ? static_cast<float> (fluxSum / static_cast<double> (numFluxBins_))
                        : 0.0f;
    std::copy (magnitudes_.begin(), magnitudes_.end(), previousMagnitudes_.begin());
    hasPreviousSpectrum_ = true;

    const auto zTime = normalizeFeature (timeNovelty, timeFeature_);
    const auto zFlux = normalizeFeature (spectralFlux_, fluxFeature_);
    const auto zEnergy = normalizeFeature (slowEnvelopeDb, energyFeature_);
    const auto fused = timeWeight_ * zTime + fluxWeight_ * zFlux
                       + energyWeight_ * zEnergy - threshold_;
    onsetScore_ = 1.0f / (1.0f + std::exp (-fused));

    const ScoreFrame currentFrame {
        onsetScore_, slowEnvelopeDb, totalAnalysisSamples_ - 1
    };

    bool accepted = false;
    if (scoreHistorySize_ >= 2)
    {
        const auto crossedThreshold
            = previousScoreFrame_.score > acceptanceThreshold_
              && olderScoreFrame_.score <= acceptanceThreshold_;
        const auto isLocalMaximum
            = previousScoreFrame_.score > olderScoreFrame_.score
              && previousScoreFrame_.score > currentFrame.score;
        const auto passesGate = previousScoreFrame_.slowEnvelopeDb > noiseGateDb_;
        const auto passesRefractory
            = ! hasAcceptedOnset_
              || previousScoreFrame_.sampleIndex - lastAcceptedSampleIndex_
                     >= static_cast<std::uint64_t> (refractorySamples);

        if (crossedThreshold && isLocalMaximum && passesGate && passesRefractory)
        {
            recordAcceptedOnset (previousScoreFrame_);
            accepted = true;
        }
    }

    olderScoreFrame_ = previousScoreFrame_;
    previousScoreFrame_ = currentFrame;
    scoreHistorySize_ = std::min (2, scoreHistorySize_ + 1);
    return accepted;
}

void OnsetDetector::recordAcceptedOnset (const ScoreFrame& frame) noexcept
{
    if (numAcceptedOnsets_ < static_cast<int> (acceptedOnsets_.size()))
    {
        acceptedOnsets_[static_cast<std::size_t> (numAcceptedOnsets_)]
            = { frame.sampleIndex, frame.score };
        ++numAcceptedOnsets_;
    }

    hasAcceptedOnset_ = true;
    lastAcceptedSampleIndex_ = frame.sampleIndex;
    const auto samplesSinceFrame = (totalAnalysisSamples_ - 1) - frame.sampleIndex;
    samplesSinceLastOnset_ = static_cast<int> (std::min<std::uint64_t> (
        samplesSinceFrame,
        static_cast<std::uint64_t> (std::numeric_limits<int>::max())));
}

const OnsetDetector::AcceptedOnset*
OnsetDetector::consumeAcceptedOnsets (int& count) noexcept
{
    count = numAcceptedOnsets_;
    numAcceptedOnsets_ = 0;
    return acceptedOnsets_.data();
}

void OnsetDetector::setWeights (const float timeWeight,
                                const float fluxWeight,
                                const float energyWeight) noexcept
{
    timeWeight_ = timeWeight;
    fluxWeight_ = fluxWeight;
    energyWeight_ = energyWeight;
}

} // namespace shredspace
