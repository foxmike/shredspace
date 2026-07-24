#include "DSP/StereoProcessor.h"
#include <algorithm>
#include <cmath>

namespace shredspace
{

StereoProcessor::StereoProcessor()
{
    panSmoother_.setCurrentAndTargetValue (0.0f);
    widthSmoother_.setCurrentAndTargetValue (1.0f);
    crossoverFreqSmoother_.setCurrentAndTargetValue (180.0f);
    reset();
}

void StereoProcessor::prepare (double sampleRate, int maxBlockSize)
{
    sampleRate_ = std::isfinite (sampleRate) ? std::max (1.0, sampleRate) : 44100.0;

    panSmoother_.reset (sampleRate_, 0.03);    // 30 ms
    widthSmoother_.reset (sampleRate_, 0.05);  // 50 ms
    crossoverFreqSmoother_.reset (sampleRate_, 0.05); // 50 ms

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate_;
    spec.maximumBlockSize = static_cast<juce::uint32> (maxBlockSize);
    spec.numChannels = 2;
    crossoverFilter_.prepare (spec);

    const double timeConstant = 0.1; // 100 ms
    correlationFilterCoeff_ = static_cast<float> (1.0 - std::exp (-1.0 / (sampleRate_ * timeConstant)));

    reset();
}

void StereoProcessor::reset() noexcept
{
    crossoverFilter_.reset();

    panSmoother_.setCurrentAndTargetValue (panSmoother_.getTargetValue());
    widthSmoother_.setCurrentAndTargetValue (widthSmoother_.getTargetValue());
    crossoverFreqSmoother_.setCurrentAndTargetValue (crossoverFreqSmoother_.getTargetValue());

    sumLR_ = 0.0f;
    sumL2_ = 0.0f;
    sumR2_ = 0.0f;
    correlation_.store (0.0f, std::memory_order_relaxed);
}

void StereoProcessor::setPan (float p) noexcept
{
    panSmoother_.setTargetValue (std::clamp (p, -1.0f, 1.0f));
}

void StereoProcessor::setWidth (float w) noexcept
{
    widthSmoother_.setTargetValue (std::clamp (w, 0.0f, 2.0f));
}

void StereoProcessor::setLowFrequencyMonoCrossover (float hz) noexcept
{
    crossoverFreqSmoother_.setTargetValue (std::clamp (hz, 60.0f, 500.0f));
}

void StereoProcessor::setLowFrequencyMonoEnabled (bool enabled) noexcept
{
    lowFrequencyMonoEnabled_ = enabled;
}

void StereoProcessor::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    const float pi = static_cast<float> (juce::MathConstants<double>::pi);
    const float invSqrt2 = 1.0f / std::sqrt (2.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        float l = left[i];
        float r = right[i];

        // 1. Mid/Side Width
        const float w = widthSmoother_.getNextValue();
        const float m = (l + r) * invSqrt2;
        const float s = (l - r) * invSqrt2;
        l = (m + w * s) * invSqrt2;
        r = (m - w * s) * invSqrt2;

        // 2. Low-Frequency Mono
        if (lowFrequencyMonoEnabled_)
        {
            const float crossoverFreq = crossoverFreqSmoother_.getNextValue();
            const float clampedCrossover = std::clamp (crossoverFreq, 60.0f, std::min (500.0f, static_cast<float> (sampleRate_ * 0.49)));

            if (std::abs (clampedCrossover - crossoverFilter_.getCutoffFrequency()) > 1.0e-4f)
            {
                crossoverFilter_.setCutoffFrequency (clampedCrossover);
            }

            float lowL = 0.0f, highL = 0.0f;
            float lowR = 0.0f, highR = 0.0f;

            crossoverFilter_.processSample (0, l, lowL, highL);
            crossoverFilter_.processSample (1, r, lowR, highR);

            const float m_low = (lowL + lowR) * 0.5f;
            l = m_low + highL;
            r = m_low + highR;
        }
        else
        {
            crossoverFreqSmoother_.getNextValue(); // advance smoother to prevent lag
        }

        // 3. Constant-Power Pan
        const float p = panSmoother_.getNextValue();
        const float gainL = std::cos (pi * (p + 1.0f) / 4.0f);
        const float gainR = std::sin (pi * (p + 1.0f) / 4.0f);

        l *= gainL;
        r *= gainR;

        left[i] = l;
        right[i] = r;

        // 4. Correlation telemetry accumulation
        const float instLR = l * r;
        const float instL2 = l * l;
        const float instR2 = r * r;

        sumLR_ += correlationFilterCoeff_ * (instLR - sumLR_);
        sumL2_ += correlationFilterCoeff_ * (instL2 - sumL2_);
        sumR2_ += correlationFilterCoeff_ * (instR2 - sumR2_);
    }

    // Publish correlation telemetry as a scalar via std::atomic<float>
    // correlation = sumLR / sqrt(sumL2*sumR2 + epsilon), clamped to [-1, 1]
    const float denominator = std::sqrt (sumL2_ * sumR2_ + 1.0e-9f);
    const float corr = sumLR_ / denominator;
    correlation_.store (std::clamp (corr, -1.0f, 1.0f), std::memory_order_relaxed);
}

float StereoProcessor::getCorrelation() const noexcept
{
    return correlation_.load (std::memory_order_relaxed);
}

} // namespace shredspace
