#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <complex>

namespace shredspace
{

/**
 * Supporting struct for normalized biquad coefficients.
 * All coefficients are normalized by dividing by the original a0 coefficient.
 */
struct BiquadCoefficients
{
    double b0 { 0.0 };
    double b1 { 0.0 };
    double b2 { 0.0 };
    double a1 { 0.0 };
    double a2 { 0.0 };
};

/**
 * Per-channel state variables for Transposed Direct Form II biquad filter.
 */
struct BiquadState
{
    double s1 { 0.0 };
    double s2 { 0.0 };
};

/**
 * RepeatToneProcessor implements the repeat tone chain's RBJ biquad filters
 * (low-pass, high-pass, peaking) used inside each delay voice's feedback loop.
 */
class RepeatToneProcessor
{
public:
    static constexpr int kControlRateDivider = 32;

    RepeatToneProcessor();
    ~RepeatToneProcessor() = default;

    RepeatToneProcessor (const RepeatToneProcessor&) = delete;
    RepeatToneProcessor& operator= (const RepeatToneProcessor&) = delete;
    RepeatToneProcessor (RepeatToneProcessor&&) = delete;
    RepeatToneProcessor& operator= (RepeatToneProcessor&&) = delete;

    /**
     * Prepares the processor. Configures the smoothers and sets default values.
     */
    void prepare (double sampleRate, int maxBlockSize, int numChannels);

    /**
     * Clears the filter state variables for all channels and snaps smoothers to target.
     */
    void reset();

    /** Sets the high-pass cutoff frequency in Hz. */
    void setHighPassFrequency (float hz);

    /** Sets the low-pass cutoff frequency in Hz. */
    void setLowPassFrequency (float hz);

    /** Sets the peaking filter center frequency, gain, and Q. */
    void setPeakingParameters (float hz, float gainDb, float q);

    /** Processes one sample through the high-pass filter for the given channel. */
    float processHighPassSample (int channel, float x) noexcept;

    /** Processes one sample through the low-pass filter for the given channel. */
    float processLowPassSample (int channel, float x) noexcept;

    /** Processes one sample through the peaking filter for the given channel. */
    float processPeakingSample (int channel, float x) noexcept;

    /** Advances smoothing by the control-rate block (32 samples) and recomputes coefficients. */
    void updateCoefficients();

    /** Factory to make low-pass coefficients. */
    static BiquadCoefficients makeLowPass (double sampleRate, double frequency, double q);

    /** Factory to make high-pass coefficients. */
    static BiquadCoefficients makeHighPass (double sampleRate, double frequency, double q);

    /** Factory to make peaking filter coefficients. */
    static BiquadCoefficients makePeaking (double sampleRate, double frequency, double q, double gainDb);

    /** Calculates the complex magnitude response |H(e^{jw})| at the given frequency. */
    static double magnitudeAt (const BiquadCoefficients& c, double sampleRate, double frequency);

private:
    void recomputeCoefficients();

    double sampleRate_ { 44100.0 };
    int numChannels_ { 2 };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> highPassFreq_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> lowPassFreq_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> peakingGainDb_;

    float peakingFrequency_ { 1000.0f };
    float peakingQ_ { 0.70710678f };

    BiquadCoefficients hpfCoeffs_;
    BiquadCoefficients lpfCoeffs_;
    BiquadCoefficients peakingCoeffs_;

    // Fixed-size members sized for the maximum channel count (2)
    BiquadState hpfState_[2];
    BiquadState lpfState_[2];
    BiquadState peakingState_[2];
};

} // namespace shredspace
