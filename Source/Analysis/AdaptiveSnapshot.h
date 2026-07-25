#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace shredspace
{

struct AdaptiveVoiceDeltas
{
    float wetLevelDeltaDb { 0.0f };
    float t60OctaveDelta { 0.0f };
    float lpfOctaveDelta { 0.0f };
    float widthDelta { 0.0f };
    float softeningDelta { 0.0f };
    float diffusionDelta { 0.0f };
};

struct AdaptiveSnapshot
{
    static constexpr int kAdaptiveVoiceCount = 2;

    float contraction { 0.0f };
    float bloom { 0.0f };
    int mode { 0 }; // 0 = Off, 1 = Articulation, 2 = Level
    AdaptiveVoiceDeltas voices[kAdaptiveVoiceCount] {};
};

static_assert (std::is_trivially_copyable<AdaptiveSnapshot>::value,
               "AdaptiveSnapshot must remain trivially copyable");
static_assert (std::is_standard_layout<AdaptiveSnapshot>::value,
               "AdaptiveSnapshot must remain standard layout");
static_assert (std::atomic<int>::is_always_lock_free,
               "AdaptiveSnapshotPublisher requires lock-free int atomics");
static_assert (std::atomic<std::uint32_t>::is_always_lock_free,
               "AdaptiveSnapshotPublisher requires lock-free generation atomics");

class AdaptiveSnapshotPublisher
{
public:
    void publish (const AdaptiveSnapshot& next) noexcept
    {
        const int currentIndex = activeIndex.load (std::memory_order_relaxed);
        const int nextIndex = 1 - currentIndex;
        buffers[nextIndex] = next;
        activeIndex.store (nextIndex, std::memory_order_release);
        generation.fetch_add (1, std::memory_order_release);
    }

    [[nodiscard]] bool read (AdaptiveSnapshot& destination) const noexcept
    {
        const std::uint32_t generationBefore =
            generation.load (std::memory_order_acquire);
        const int currentIndex = activeIndex.load (std::memory_order_acquire);
        const AdaptiveSnapshot candidate = buffers[currentIndex];
        const std::uint32_t generationAfter =
            generation.load (std::memory_order_acquire);

        if (generationBefore != generationAfter)
            return false;

        destination = candidate;
        return true;
    }

    [[nodiscard]] std::uint32_t currentGeneration() const noexcept
    {
        return generation.load (std::memory_order_acquire);
    }

private:
    AdaptiveSnapshot buffers[2] {};
    std::atomic<int> activeIndex { 0 };
    std::atomic<std::uint32_t> generation { 0 };
};

} // namespace shredspace
