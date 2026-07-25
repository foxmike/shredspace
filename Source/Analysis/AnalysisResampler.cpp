#include "Analysis/AnalysisResampler.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace shredspace
{

void AnalysisResampler::prepare (double hostSampleRate, int maxBlockSize)
{
    hostSampleRate_ = std::isfinite (hostSampleRate) && hostSampleRate > 0.0
                          ? hostSampleRate
                          : analysisSampleRate;
    phaseIncrement_ = hostSampleRate_ / analysisSampleRate;

    const int preparedBlockSize = std::max (1, maxBlockSize);
    constexpr double worstCaseRatio = analysisSampleRate / 44100.0;
    constexpr int interpolationGuardSamples = 4;
    constexpr int safetyMarginSamples = 8;
    const int maximumOutputSamples =
        static_cast<int> (std::ceil (static_cast<double> (preparedBlockSize)
                                    * worstCaseRatio))
        + interpolationGuardSamples + safetyMarginSamples;

    inputStagingBuffer_.assign (static_cast<std::size_t> (preparedBlockSize), 0.0f);
    outputBuffer_.assign (static_cast<std::size_t> (maximumOutputSamples), 0.0f);

    constexpr double butterworthQ = 0.7071;
    const double cutoff = std::min (
        9000.0, 0.45 * std::min (hostSampleRate_, analysisSampleRate));
    const double omega = 2.0 * std::numbers::pi * cutoff / hostSampleRate_;
    const double cosine = std::cos (omega);
    const double alpha = std::sin (omega) / (2.0 * butterworthQ);
    const double inverseA0 = 1.0 / (1.0 + alpha);

    BiquadCoefficients coefficients;
    coefficients.b0 = 0.5 * (1.0 - cosine) * inverseA0;
    coefficients.b1 = (1.0 - cosine) * inverseA0;
    coefficients.b2 = coefficients.b0;
    coefficients.a1 = -2.0 * cosine * inverseA0;
    coefficients.a2 = (1.0 - alpha) * inverseA0;
    lowPassCoefficients_.fill (coefficients);

    reset();
}

void AnalysisResampler::reset() noexcept
{
    lowPassState_.fill ({});
    std::fill (inputStagingBuffer_.begin(), inputStagingBuffer_.end(), 0.0f);
    std::fill (outputBuffer_.begin(), outputBuffer_.end(), 0.0f);
    readPhase = 0.0;
    previousInputSample_ = 0.0f;
    hasPreviousInputSample_ = false;
}

int AnalysisResampler::process (const float* monoAnalysisInput,
                                int numSamples,
                                float* outBuffer,
                                int outBufferCapacity) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    if (monoAnalysisInput == nullptr || numSamples <= 0
        || inputStagingBuffer_.empty())
        return 0;

    const int inputCount = std::min (
        numSamples, static_cast<int> (inputStagingBuffer_.size()));
    for (int sample = 0; sample < inputCount; ++sample)
        inputStagingBuffer_[static_cast<std::size_t> (sample)] =
            processLowPass (monoAnalysisInput[sample]);

    const int callerCapacity =
        outBuffer != nullptr ? std::max (0, outBufferCapacity) : 0;
    const int outputLimit = std::min (
        callerCapacity, static_cast<int> (outputBuffer_.size()));

    if (hostSampleRate_ == analysisSampleRate)
    {
        const int outputCount = std::min (inputCount, outputLimit);
        std::copy_n (inputStagingBuffer_.data(), outputCount, outputBuffer_.data());
        if (outputCount > 0)
            std::copy_n (outputBuffer_.data(), outputCount, outBuffer);

        previousInputSample_ =
            inputStagingBuffer_[static_cast<std::size_t> (inputCount - 1)];
        hasPreviousInputSample_ = true;
        readPhase = 0.0;
        return outputCount;
    }

    if (! hasPreviousInputSample_)
    {
        previousInputSample_ = inputStagingBuffer_.front();
        hasPreviousInputSample_ = true;
    }

    const double lastInputPosition = static_cast<double> (inputCount - 1);
    int outputCount = 0;

    for (; outputCount < outputLimit; ++outputCount)
    {
        if (readPhase > lastInputPosition)
            break;

        const int leftIndex = static_cast<int> (std::floor (readPhase));
        float left = previousInputSample_;
        float right = inputStagingBuffer_.front();
        double fraction = readPhase + 1.0;

        if (leftIndex >= 0)
        {
            left = inputStagingBuffer_[static_cast<std::size_t> (leftIndex)];
            right = leftIndex < inputCount - 1
                        ? inputStagingBuffer_[static_cast<std::size_t> (leftIndex + 1)]
                        : left;
            fraction = readPhase - static_cast<double> (leftIndex);
        }

        outputBuffer_[static_cast<std::size_t> (outputCount)] =
            left + static_cast<float> (fraction) * (right - left);
        readPhase += phaseIncrement_;
    }

    if (outputCount > 0)
        std::copy_n (outputBuffer_.data(), outputCount, outBuffer);

    if (readPhase <= lastInputPosition)
    {
        const double remainingOutputIntervals =
            std::floor ((lastInputPosition - readPhase) / phaseIncrement_) + 1.0;
        readPhase += remainingOutputIntervals * phaseIncrement_;
    }

    readPhase -= static_cast<double> (inputCount);
    previousInputSample_ =
        inputStagingBuffer_[static_cast<std::size_t> (inputCount - 1)];
    return outputCount;
}

float AnalysisResampler::processLowPass (float input) noexcept
{
    double value = static_cast<double> (input);

    for (std::size_t section = 0; section < lowPassState_.size(); ++section)
    {
        const auto& coefficients = lowPassCoefficients_[section];
        auto& state = lowPassState_[section];

        const double output = coefficients.b0 * value + state.z1;
        state.z1 = coefficients.b1 * value - coefficients.a1 * output + state.z2;
        state.z2 = coefficients.b2 * value - coefficients.a2 * output;
        value = output;
    }

    return static_cast<float> (value);
}

} // namespace shredspace
