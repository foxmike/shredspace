#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <array>
#include <algorithm>

namespace shredspace
{

struct HistorySample
{
    float onset { 0.0f };
    float density { 0.0f };
    float sustain { 0.0f };
    float gap { 0.0f };
};

class TelemetryState
{
public:
    static constexpr int historyCapacity = 1024;

    TelemetryState() noexcept;
    ~TelemetryState() = default;

    TelemetryState (const TelemetryState&) = delete;
    TelemetryState& operator= (const TelemetryState&) = delete;
    TelemetryState (TelemetryState&&) = delete;
    TelemetryState& operator= (TelemetryState&&) = delete;

    void prepare() noexcept;
    void reset() noexcept;

    // --- Writers (Relaxed memory order) ---
    void setLeftPeak (float value) noexcept { leftPeak.store (value, std::memory_order_relaxed); }
    void setRightPeak (float value) noexcept { rightPeak.store (value, std::memory_order_relaxed); }
    void setLeftRms (float value) noexcept { leftRms.store (value, std::memory_order_relaxed); }
    void setRightRms (float value) noexcept { rightRms.store (value, std::memory_order_relaxed); }
    void setActivity (float value) noexcept { activity.store (value, std::memory_order_relaxed); }
    void setDensity (float value) noexcept { density.store (value, std::memory_order_relaxed); }
    void setSustain (float value) noexcept { sustain.store (value, std::memory_order_relaxed); }
    void setContraction (float value) noexcept { contraction.store (value, std::memory_order_relaxed); }
    void setBloom (float value) noexcept { bloom.store (value, std::memory_order_relaxed); }
    void setCorrelation (float value) noexcept { correlation.store (value, std::memory_order_relaxed); }
    void setSafetyEngaged (bool value) noexcept { safetyEngaged.store (value, std::memory_order_relaxed); }

    // --- Readers (Relaxed memory order) ---
    float getLeftPeak() const noexcept { return leftPeak.load (std::memory_order_relaxed); }
    float getRightPeak() const noexcept { return rightPeak.load (std::memory_order_relaxed); }
    float getLeftRms() const noexcept { return leftRms.load (std::memory_order_relaxed); }
    float getRightRms() const noexcept { return rightRms.load (std::memory_order_relaxed); }
    float getActivity() const noexcept { return activity.load (std::memory_order_relaxed); }
    float getDensity() const noexcept { return density.load (std::memory_order_relaxed); }
    float getSustain() const noexcept { return sustain.load (std::memory_order_relaxed); }
    float getContraction() const noexcept { return contraction.load (std::memory_order_relaxed); }
    float getBloom() const noexcept { return bloom.load (std::memory_order_relaxed); }
    float getCorrelation() const noexcept { return correlation.load (std::memory_order_relaxed); }
    bool getSafetyEngaged() const noexcept { return safetyEngaged.load (std::memory_order_relaxed); }

    // --- History FIFO ---
    void pushHistory (const HistorySample& sample) noexcept;
    int readHistory (HistorySample* dest, int maxSamples) noexcept;

private:
    std::atomic<float> leftPeak { 0.0f };
    std::atomic<float> rightPeak { 0.0f };
    std::atomic<float> leftRms { 0.0f };
    std::atomic<float> rightRms { 0.0f };
    std::atomic<float> activity { 0.0f };
    std::atomic<float> density { 0.0f };
    std::atomic<float> sustain { 0.0f };
    std::atomic<float> contraction { 0.0f };
    std::atomic<float> bloom { 0.0f };
    std::atomic<float> correlation { 0.0f };
    std::atomic<bool> safetyEngaged { false };

    juce::AbstractFifo fifo;
    std::array<HistorySample, historyCapacity> historyBacking {};
};

} // namespace shredspace
