#pragma once

#include <array>
#include <vector>

namespace shredspace
{

/**
 * Band-limited, fixed-rate front end for the analysis signal path.
 *
 * Input is filtered at the host sample rate and converted to 48 kHz. All
 * storage is allocated by prepare(); process() is allocation-free and carries
 * its filter and interpolation state across block boundaries.
 */
class AnalysisResampler
{
public:
    static constexpr double analysisSampleRate = 48000.0;

    AnalysisResampler() = default;
    AnalysisResampler (const AnalysisResampler&) = delete;
    AnalysisResampler& operator= (const AnalysisResampler&) = delete;
    AnalysisResampler (AnalysisResampler&&) = delete;
    AnalysisResampler& operator= (AnalysisResampler&&) = delete;

    /** Allocates scratch storage and configures the anti-alias filters. */
    void prepare (double hostSampleRate, int maxBlockSize);

    /** Clears filter, interpolation, and scratch history without allocating. */
    void reset() noexcept;

    /**
     * Filters and converts one mono host-rate block to the fixed analysis rate.
     * Returns the number of samples written, never exceeding outBufferCapacity.
     */
    [[nodiscard]] int process (const float* monoAnalysisInput,
                               int numSamples,
                               float* outBuffer,
                               int outBufferCapacity) noexcept;

private:
    struct BiquadCoefficients
    {
        double b0 {};
        double b1 {};
        double b2 {};
        double a1 {};
        double a2 {};
    };

    struct BiquadState
    {
        double z1 {};
        double z2 {};
    };

    [[nodiscard]] float processLowPass (float input) noexcept;

    std::array<BiquadCoefficients, 2> lowPassCoefficients_ {};
    std::array<BiquadState, 2> lowPassState_ {};
    std::vector<float> inputStagingBuffer_;
    std::vector<float> outputBuffer_;

    double hostSampleRate_ { analysisSampleRate };
    double phaseIncrement_ { 1.0 };
    double readPhase {};
    float previousInputSample_ {};
    bool hasPreviousInputSample_ {};
};

} // namespace shredspace
