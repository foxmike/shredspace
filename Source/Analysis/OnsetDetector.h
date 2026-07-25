#pragma once

#include "Analysis/AnalysisResampler.h"

#include <juce_dsp/juce_dsp.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace shredspace
{

/**
 * Fixed-rate spectral-flux onset detector.
 *
 * The input must be the 48 kHz analysis stream produced by
 * PerformanceAnalyzer. The detector owns no host-rate-dependent state.
 */
class OnsetDetector
{
public:
    struct AcceptedOnset
    {
        std::uint64_t analysisSampleIndex {};
        float score {};
    };

    static constexpr int fftOrder = 9;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = 256;
    static constexpr int refractorySamples = 1200;
    static constexpr double analysisSampleRate = AnalysisResampler::analysisSampleRate;

    OnsetDetector() = default;
    OnsetDetector (const OnsetDetector&) = delete;
    OnsetDetector& operator= (const OnsetDetector&) = delete;
    OnsetDetector (OnsetDetector&&) = delete;
    OnsetDetector& operator= (OnsetDetector&&) = delete;

    /** Allocates all FFT, frame, history, and event storage. */
    void prepare (int maxAnalysisBlockSamples);

    /** Clears all analysis and event state without allocating. */
    void reset() noexcept;

    /** Consumes samples from PerformanceAnalyzer's fixed-rate output. */
    void process (const float* analysisSamples,
                  int numAnalysisSamples,
                  float timeNovelty,
                  float slowEnvelopeDb) noexcept;

    [[nodiscard]] float getOnsetScore() const noexcept { return onsetScore_; }
    [[nodiscard]] float getSpectralFlux() const noexcept { return spectralFlux_; }
    [[nodiscard]] float getSmoothedOnsetActivity() const noexcept
    {
        return smoothedOnsetActivity_;
    }

    [[nodiscard]] int getSamplesSinceLastOnset() const noexcept
    {
        return samplesSinceLastOnset_;
    }

    /** Events remain valid until the next process() call. */
    [[nodiscard]] const AcceptedOnset* getAcceptedOnsets() const noexcept
    {
        return acceptedOnsets_.data();
    }

    [[nodiscard]] int getNumAcceptedOnsets() const noexcept
    {
        return numAcceptedOnsets_;
    }

    /**
     * Returns the events produced by the most recent process() call and marks
     * them consumed. The returned storage remains valid until process() is
     * called again.
     */
    [[nodiscard]] const AcceptedOnset* consumeAcceptedOnsets (int& count) noexcept;

    void setWeights (float timeWeight, float fluxWeight, float energyWeight) noexcept;
    void setThreshold (float newThreshold) noexcept { threshold_ = newThreshold; }
    void setAcceptanceThreshold (float newThreshold) noexcept
    {
        acceptanceThreshold_ = newThreshold;
    }
    void setNoiseGateDb (float newGateDb) noexcept { noiseGateDb_ = newGateDb; }

    [[nodiscard]] float getTimeWeight() const noexcept { return timeWeight_; }
    [[nodiscard]] float getFluxWeight() const noexcept { return fluxWeight_; }
    [[nodiscard]] float getEnergyWeight() const noexcept { return energyWeight_; }
    [[nodiscard]] float getThreshold() const noexcept { return threshold_; }
    [[nodiscard]] float getAcceptanceThreshold() const noexcept
    {
        return acceptanceThreshold_;
    }
    [[nodiscard]] float getNoiseGateDb() const noexcept { return noiseGateDb_; }

private:
    struct AdaptiveFeature
    {
        float mean {};
        float deviation {};
        bool initialized {};
    };

    struct ScoreFrame
    {
        float score {};
        float slowEnvelopeDb {};
        std::uint64_t sampleIndex {};
    };

    [[nodiscard]] float normalizeFeature (float value,
                                          AdaptiveFeature& feature) noexcept;
    [[nodiscard]] bool processFrame (float timeNovelty,
                                     float slowEnvelopeDb) noexcept;
    void recordAcceptedOnset (const ScoreFrame& frame) noexcept;

    static constexpr float epsilon = 1.0e-9f;

    std::unique_ptr<juce::dsp::FFT> fft_;
    std::vector<float> inputRing_;
    std::vector<float> window_;
    std::vector<float> fftData_;
    std::vector<float> magnitudes_;
    std::vector<float> previousMagnitudes_;
    std::vector<float> binWeights_;
    std::vector<AcceptedOnset> acceptedOnsets_;

    int writePosition_ {};
    int samplesUntilNextFrame_ { fftSize };
    int firstFluxBin_ {};
    int lastFluxBin_ {};
    int numFluxBins_ {};
    int numAcceptedOnsets_ {};
    int samplesSinceLastOnset_ { refractorySamples };

    std::uint64_t totalAnalysisSamples_ {};
    std::uint64_t lastAcceptedSampleIndex_ {};
    bool hasPreviousSpectrum_ {};
    bool hasAcceptedOnset_ {};

    AdaptiveFeature timeFeature_;
    AdaptiveFeature fluxFeature_;
    AdaptiveFeature energyFeature_;
    float normalizationCoefficient_ {};
    float activityPole_ {};

    ScoreFrame olderScoreFrame_;
    ScoreFrame previousScoreFrame_;
    int scoreHistorySize_ {};

    float spectralFlux_ {};
    float onsetScore_ {};
    float smoothedOnsetActivity_ {};

    float timeWeight_ { 0.35f };
    float fluxWeight_ { 0.50f };
    float energyWeight_ { 0.15f };
    float threshold_ { 1.0f };
    float acceptanceThreshold_ { 0.5f };
    float noiseGateDb_ { -55.0f };
};

} // namespace shredspace
