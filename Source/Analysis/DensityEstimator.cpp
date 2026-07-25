#include "Analysis/DensityEstimator.h"

#include <algorithm>
#include <cmath>

namespace shredspace
{

DensityEstimator::DensityEstimator() noexcept
{
    reset();
}

void DensityEstimator::prepare() noexcept
{
    reset();
}

void DensityEstimator::reset() noexcept
{
    head_ = 0;
    tail_ = 0;
    size_ = 0;
    currentClock_ = 0;
    buffer_.fill (0);
}

void DensityEstimator::pushOnset (std::int64_t analysisSampleTimestamp) noexcept
{
    buffer_[head_] = analysisSampleTimestamp;
    head_ = (head_ + 1) % ringBufferCapacity;

    if (size_ < ringBufferCapacity)
    {
        size_++;
    }
    else
    {
        tail_ = (tail_ + 1) % ringBufferCapacity;
    }

    evictExpired();
}

void DensityEstimator::advance (int numAnalysisSamples) noexcept
{
    if (numAnalysisSamples <= 0)
        return;

    currentClock_ += numAnalysisSamples;
    evictExpired();
}

float DensityEstimator::getEventRate() const noexcept
{
    if (windowDuration_ <= 0.0)
        return 0.0f;
    return static_cast<float> (size_) / static_cast<float> (windowDuration_);
}

float DensityEstimator::getDensity() const noexcept
{
    const float eventRate = getEventRate();
    if (densityFull_ <= densityStart_)
        return 0.0f;

    float u = (eventRate - densityStart_) / (densityFull_ - densityStart_);
    u = std::max (0.0f, std::min (u, 1.0f));

    return 3.0f * u * u - 2.0f * u * u * u;
}

void DensityEstimator::setWindowDuration (double seconds) noexcept
{
    if (seconds > 0.0)
    {
        windowDuration_ = seconds;
        windowSamples_ = static_cast<std::int64_t> (std::round (seconds * analysisSampleRate));
        evictExpired();
    }
}

void DensityEstimator::evictExpired() noexcept
{
    std::size_t numEvicted = 0;
    while (size_ > 0 && numEvicted < ringBufferCapacity)
    {
        const std::int64_t oldestTimestamp = buffer_[tail_];
        if (currentClock_ - oldestTimestamp > windowSamples_)
        {
            tail_ = (tail_ + 1) % ringBufferCapacity;
            size_--;
            numEvicted++;
        }
        else
        {
            break;
        }
    }
}

} // namespace shredspace
