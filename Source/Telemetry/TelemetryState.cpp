#include "Telemetry/TelemetryState.h"

namespace shredspace
{

TelemetryState::TelemetryState() noexcept
    : fifo (historyCapacity)
{
    reset();
}

void TelemetryState::prepare() noexcept
{
    reset();
}

void TelemetryState::reset() noexcept
{
    fifo.reset();
    
    leftPeak.store (0.0f, std::memory_order_relaxed);
    rightPeak.store (0.0f, std::memory_order_relaxed);
    leftRms.store (0.0f, std::memory_order_relaxed);
    rightRms.store (0.0f, std::memory_order_relaxed);
    activity.store (0.0f, std::memory_order_relaxed);
    density.store (0.0f, std::memory_order_relaxed);
    sustain.store (0.0f, std::memory_order_relaxed);
    contraction.store (0.0f, std::memory_order_relaxed);
    bloom.store (0.0f, std::memory_order_relaxed);
    correlation.store (0.0f, std::memory_order_relaxed);
    safetyEngaged.store (false, std::memory_order_relaxed);
}

void TelemetryState::pushHistory (const HistorySample& sample) noexcept
{
    int start1, size1, start2, size2;
    fifo.prepareToWrite (1, start1, size1, start2, size2);
    
    if (size1 + size2 < 1)
        return; // drop the sample and return
    
    if (size1 > 0)
    {
        historyBacking[static_cast<size_t> (start1)] = sample;
    }
    else if (size2 > 0)
    {
        historyBacking[static_cast<size_t> (start2)] = sample;
    }
    
    fifo.finishedWrite (size1 + size2);
}

int TelemetryState::readHistory (HistorySample* dest, int maxSamples) noexcept
{
    if (maxSamples <= 0 || dest == nullptr)
        return 0;
    
    int start1, size1, start2, size2;
    fifo.prepareToRead (maxSamples, start1, size1, start2, size2);
    
    const int totalRead = size1 + size2;
    
    if (totalRead > 0)
    {
        if (size1 > 0)
        {
            std::copy (historyBacking.begin() + start1,
                       historyBacking.begin() + start1 + size1,
                       dest);
        }
        if (size2 > 0)
        {
            std::copy (historyBacking.begin() + start2,
                       historyBacking.begin() + start2 + size2,
                       dest + size1);
        }
        
        fifo.finishedRead (totalRead);
    }
    
    return totalRead;
}

} // namespace shredspace
