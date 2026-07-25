#pragma once

#include <array>
#include <cstdint>

namespace shredspace
{

/**
 * Fixed-rate note density estimator.
 *
 * Tracks the rate of onset events inside a sliding temporal window using a
 * fixed-size, real-time-safe ring buffer. All calculations are performed
 * at the fixed 48 kHz analysis rate.
 */
class DensityEstimator
{
public:
    static constexpr double analysisSampleRate = 48000.0;
    static constexpr std::size_t ringBufferCapacity = 64;

    DensityEstimator() noexcept;
    ~DensityEstimator() = default;

    DensityEstimator (const DensityEstimator&) = delete;
    DensityEstimator& operator= (const DensityEstimator&) = delete;
    DensityEstimator (DensityEstimator&&) = delete;
    DensityEstimator& operator= (DensityEstimator&&) = delete;

    /** Prepares the estimator and resets the internal state. */
    void prepare() noexcept;

    /** Clears all timestamps and resets the internal clock. */
    void reset() noexcept;

    /** Pushes an onset timestamp (expressed in analysis samples). */
    void pushOnset (std::int64_t analysisSampleTimestamp) noexcept;

    /** Advances the internal analysis-sample clock and evicts expired timestamps. */
    void advance (int numAnalysisSamples) noexcept;

    /** Returns the current event rate (events per second). */
    [[nodiscard]] float getEventRate() const noexcept;

    /** Returns the density confidence value in [0, 1]. */
    [[nodiscard]] float getDensity() const noexcept;

    /** Sets the sliding density window duration in seconds. */
    void setWindowDuration (double seconds) noexcept;

    /** Sets the lower threshold for event rate mapping (events per second). */
    void setDensityStart (float start) noexcept { densityStart_ = start; }

    /** Sets the upper threshold for event rate mapping (events per second). */
    void setDensityFull (float full) noexcept { densityFull_ = full; }

    /** Returns the window duration in seconds. */
    [[nodiscard]] double getWindowDuration() const noexcept { return windowDuration_; }

    /** Returns the density start parameter. */
    [[nodiscard]] float getDensityStart() const noexcept { return densityStart_; }

    /** Returns the density full parameter. */
    [[nodiscard]] float getDensityFull() const noexcept { return densityFull_; }

private:
    void evictExpired() noexcept;

    std::array<std::int64_t, ringBufferCapacity> buffer_ {};
    std::size_t head_ { 0 };
    std::size_t tail_ { 0 };
    std::size_t size_ { 0 };

    std::int64_t currentClock_ { 0 };

    double windowDuration_ { 2.0 };
    std::int64_t windowSamples_ { 96000 }; // 2.0 * 48000

    float densityStart_ { 2.0f };
    float densityFull_ { 14.0f };
};

} // namespace shredspace
