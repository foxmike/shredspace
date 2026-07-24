#include "DSP/RepeatToneProcessor.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace shredspace
{

RepeatToneProcessor::RepeatToneProcessor()
{
    highPassFreq_.setCurrentAndTargetValue (20.0f);
    lowPassFreq_.setCurrentAndTargetValue (20000.0f);
    peakingGainDb_.setCurrentAndTargetValue (0.0f);

    recomputeCoefficients();
}

void RepeatToneProcessor::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    juce::ignoreUnused (maxBlockSize);

    sampleRate_ = std::isfinite (sampleRate) ? std::max (1.0, sampleRate) : 44100.0;
    numChannels_ = std::clamp (numChannels, 1, 2);

    highPassFreq_.reset (sampleRate_, 0.05); // 50 ms
    lowPassFreq_.reset (sampleRate_, 0.05);  // 50 ms
    peakingGainDb_.reset (sampleRate_, 0.02); // 20 ms

    reset();
}

void RepeatToneProcessor::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        hpfState_[ch] = {};
        lpfState_[ch] = {};
        peakingState_[ch] = {};
    }

    highPassFreq_.setCurrentAndTargetValue (highPassFreq_.getTargetValue());
    lowPassFreq_.setCurrentAndTargetValue (lowPassFreq_.getTargetValue());
    peakingGainDb_.setCurrentAndTargetValue (peakingGainDb_.getTargetValue());

    recomputeCoefficients();
}

void RepeatToneProcessor::setHighPassFrequency (float hz)
{
    highPassFreq_.setTargetValue (std::max (20.0f, hz));
}

void RepeatToneProcessor::setLowPassFrequency (float hz)
{
    lowPassFreq_.setTargetValue (std::max (20.0f, hz));
}

void RepeatToneProcessor::setPeakingParameters (float hz, float gainDb, float q)
{
    peakingFrequency_ = std::max (20.0f, hz);
    peakingGainDb_.setTargetValue (gainDb);
    peakingQ_ = std::max (0.001f, q);
}

float RepeatToneProcessor::processHighPassSample (int channel, float x) noexcept
{
    jassert (channel >= 0 && channel < 2);
    auto& state = hpfState_[channel];
    const auto& coeffs = hpfCoeffs_;

    const double xd = static_cast<double> (x);
    const double yd = coeffs.b0 * xd + state.s1;
    state.s1 = coeffs.b1 * xd - coeffs.a1 * yd + state.s2;
    state.s2 = coeffs.b2 * xd - coeffs.a2 * yd;

    return static_cast<float> (yd);
}

float RepeatToneProcessor::processLowPassSample (int channel, float x) noexcept
{
    jassert (channel >= 0 && channel < 2);
    auto& state = lpfState_[channel];
    const auto& coeffs = lpfCoeffs_;

    const double xd = static_cast<double> (x);
    const double yd = coeffs.b0 * xd + state.s1;
    state.s1 = coeffs.b1 * xd - coeffs.a1 * yd + state.s2;
    state.s2 = coeffs.b2 * xd - coeffs.a2 * yd;

    return static_cast<float> (yd);
}

float RepeatToneProcessor::processPeakingSample (int channel, float x) noexcept
{
    jassert (channel >= 0 && channel < 2);
    auto& state = peakingState_[channel];
    const auto& coeffs = peakingCoeffs_;

    const double xd = static_cast<double> (x);
    const double yd = coeffs.b0 * xd + state.s1;
    state.s1 = coeffs.b1 * xd - coeffs.a1 * yd + state.s2;
    state.s2 = coeffs.b2 * xd - coeffs.a2 * yd;

    return static_cast<float> (yd);
}

void RepeatToneProcessor::updateCoefficients()
{
    juce::ScopedNoDenormals noDenormals;

    highPassFreq_.skip (kControlRateDivider);
    lowPassFreq_.skip (kControlRateDivider);
    peakingGainDb_.skip (kControlRateDivider);

    recomputeCoefficients();
}

void RepeatToneProcessor::recomputeCoefficients()
{
    const double Fs = sampleRate_;
    const double maxFreq = std::min (20000.0, 0.45 * Fs);

    const double hpFreq = std::clamp (static_cast<double> (highPassFreq_.getCurrentValue()), 20.0, maxFreq);
    const double lpFreq = std::clamp (static_cast<double> (lowPassFreq_.getCurrentValue()), 20.0, maxFreq);
    const double peakFreq = std::clamp (static_cast<double> (peakingFrequency_), 20.0, maxFreq);
    const double peakGainDb = static_cast<double> (peakingGainDb_.getCurrentValue());

    hpfCoeffs_ = makeHighPass (Fs, hpFreq, 0.70710678);
    lpfCoeffs_ = makeLowPass (Fs, lpFreq, 0.70710678);
    peakingCoeffs_ = makePeaking (Fs, peakFreq, peakingQ_, peakGainDb);
}

BiquadCoefficients RepeatToneProcessor::makeLowPass (double sampleRate, double frequency, double q)
{
    const double Fs = (std::isfinite (sampleRate) && sampleRate > 0.0) ? sampleRate : 44100.0;
    const double maxFreq = std::min (20000.0, 0.45 * Fs);
    const double f0 = std::clamp (frequency, 20.0, maxFreq);
    const double Q = (std::isfinite (q) && q > 0.0) ? std::max (0.001, q) : 0.70710678;

    const double omega0 = 2.0 * juce::MathConstants<double>::pi * f0 / Fs;
    const double alpha = std::sin (omega0) / (2.0 * Q);
    const double cosOmega = std::cos (omega0);

    const double b0 = (1.0 - cosOmega) / 2.0;
    const double b1 = 1.0 - cosOmega;
    const double b2 = b0;
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * cosOmega;
    const double a2 = 1.0 - alpha;

    return BiquadCoefficients { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

BiquadCoefficients RepeatToneProcessor::makeHighPass (double sampleRate, double frequency, double q)
{
    const double Fs = (std::isfinite (sampleRate) && sampleRate > 0.0) ? sampleRate : 44100.0;
    const double maxFreq = std::min (20000.0, 0.45 * Fs);
    const double f0 = std::clamp (frequency, 20.0, maxFreq);
    const double Q = (std::isfinite (q) && q > 0.0) ? std::max (0.001, q) : 0.70710678;

    const double omega0 = 2.0 * juce::MathConstants<double>::pi * f0 / Fs;
    const double alpha = std::sin (omega0) / (2.0 * Q);
    const double cosOmega = std::cos (omega0);

    const double b0 = (1.0 + cosOmega) / 2.0;
    const double b1 = -(1.0 + cosOmega);
    const double b2 = b0;
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * cosOmega;
    const double a2 = 1.0 - alpha;

    return BiquadCoefficients { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

BiquadCoefficients RepeatToneProcessor::makePeaking (double sampleRate, double frequency, double q, double gainDb)
{
    const double Fs = (std::isfinite (sampleRate) && sampleRate > 0.0) ? sampleRate : 44100.0;
    const double maxFreq = std::min (20000.0, 0.45 * Fs);
    const double f0 = std::clamp (frequency, 20.0, maxFreq);
    const double Q = (std::isfinite (q) && q > 0.0) ? std::max (0.001, q) : 0.70710678;
    const double G_dB = std::isfinite (gainDb) ? gainDb : 0.0;

    const double omega0 = 2.0 * juce::MathConstants<double>::pi * f0 / Fs;
    const double alpha = std::sin (omega0) / (2.0 * Q);
    const double cosOmega = std::cos (omega0);

    const double A = std::pow (10.0, G_dB / 40.0);
    const double b0 = 1.0 + alpha * A;
    const double b1 = -2.0 * cosOmega;
    const double b2 = 1.0 - alpha * A;
    const double a0 = 1.0 + alpha / A;
    const double a1 = -2.0 * cosOmega;
    const double a2 = 1.0 - alpha / A;

    return BiquadCoefficients { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

double RepeatToneProcessor::magnitudeAt (const BiquadCoefficients& c, double sampleRate, double frequency)
{
    const double Fs = (std::isfinite (sampleRate) && sampleRate > 0.0) ? sampleRate : 44100.0;
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / Fs;
    const std::complex<double> z1 (std::cos (omega), -std::sin (omega));
    const std::complex<double> z2 (std::cos (2.0 * omega), -std::sin (2.0 * omega));

    const std::complex<double> num = c.b0 + c.b1 * z1 + c.b2 * z2;
    const std::complex<double> den = 1.0 + c.a1 * z1 + c.a2 * z2;

    const double denAbs = std::abs (den);
    if (denAbs <= 0.0)
        return 0.0;

    return std::abs (num) / denAbs;
}

} // namespace shredspace
