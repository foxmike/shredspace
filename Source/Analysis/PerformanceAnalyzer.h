#pragma once

#include "Analysis/AnalysisResampler.h"

#include <cmath>
#include <vector>

namespace shredspace
{

/**
 * Fixed-rate analysis front end for the performance signal path.
 *
 * Computes the analysis-band mono energy signal from the incoming host-rate
 * audio, resamples it to the fixed 48 kHz analysis rate through an owned
 * AnalysisResampler, and updates fast/slow one-pole envelopes plus a
 * time-domain novelty feature at that fixed rate.
 *
 * All storage is allocated in prepare(); process() performs no heap
 * allocation, locking, file IO, or buffer resizing. Outputs are continuous
 * scalar features only — this module never classifies technique or emits
 * boolean decisions.
 */
class PerformanceAnalyzer
{
public:
    static constexpr double analysisSampleRate = AnalysisResampler::analysisSampleRate;

    PerformanceAnalyzer() = default;
    PerformanceAnalyzer (const PerformanceAnalyzer&) = delete;
    PerformanceAnalyzer& operator= (const PerformanceAnalyzer&) = delete;
    PerformanceAnalyzer (PerformanceAnalyzer&&) = delete;
    PerformanceAnalyzer& operator= (PerformanceAnalyzer&&) = delete;

    /** Allocates all buffers, prepares the owned resampler, precomputes
        envelope coefficients, and resets state. */
    void prepare (double hostSampleRate, int maxBlockSize);

    /** Zeroes envelopes and history. Allocates nothing. */
    void reset() noexcept;

    /** Computes the analysis-band mono energy signal, resamples it to the
        fixed analysis rate, and updates envelopes and novelty for every
        produced analysis sample. */
    void process (const float* left,
                  const float* right,
                  int numChannels,
                  int numSamples) noexcept;

    // ---- Continuous outputs (const, cheap, no allocation) ----

    [[nodiscard]] float getFastEnvelope() const noexcept { return fastEnvelope_; }
    [[nodiscard]] float getSlowEnvelope() const noexcept { return slowEnvelope_; }
    [[nodiscard]] float getTimeNovelty() const noexcept { return timeNovelty_; }

    [[nodiscard]] float getSlowEnvelopeDb() const noexcept
    {
        return 20.0f * std::log10 (slowEnvelope_ + slowEnvelopeEps);
    }

    /** Fixed-rate analysis samples produced by the most recent process() call. */
    [[nodiscard]] const float* getAnalysisBuffer() const noexcept
    {
        return analysisBuffer_.data();
    }

    [[nodiscard]] int getNumAnalysisSamples() const noexcept
    {
        return numAnalysisSamples_;
    }

private:
    static constexpr double tauFast = 0.002;   // 2 ms
    static constexpr double tauSlow = 0.060;   // 60 ms
    static constexpr double stereoEnergyEpsilon = 1.0e-20;
    static constexpr float noveltyEps = 1.0e-12f;
    static constexpr float slowEnvelopeEps = 1.0e-12f;

    AnalysisResampler resampler_;

    std::vector<float> energyStagingBuffer_; // host-rate mono energy, per block
    std::vector<float> analysisBuffer_;      // fixed-rate analysis samples

    double alphaFast_ { 0.0 };
    double alphaSlow_ { 0.0 };

    float fastEnvelope_ { 0.0f };
    float slowEnvelope_ { 0.0f };
    float timeNovelty_ { 0.0f };

    int numAnalysisSamples_ { 0 };
};

} // namespace shredspace
