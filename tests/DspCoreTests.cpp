/*
  ==============================================================================
  DSP core unit tests for ShredSpace. Uses ONLY juce::UnitTest; no third-party
  test frameworks (Catch2, GoogleTest, etc.) are involved.

  Coverage:
    1. Impulse first-repeat placement at round(Fs * T) for several sample rates.
    2. FractionalDelay interpolation matches independently computed 3rd-order
       Lagrange basis values for several mu in [0, 1).
    3. Clamp behaviour: minimum read position of 8 samples and maximum of
       8.25 seconds.
    4. T60 decay: 20*log10(A(T60)/A(0)) ~= -60 dB and feedback gain never
       exceeds 0.985 (including Hold-like infinite-tail conditions).
    5. Tap-tempo median with one outlier outside [100, 3000] ms rejected.
    6. BPM fallback: 120 BPM when no valid BPM has been seen; last valid BPM
       retained once bpmValid goes false.

  Implementation note: FractionalDelay.cpp and FeedbackController.cpp are
  compiled directly into this translation unit (mirroring the pattern used by
  ParameterTests.cpp for DelayTimeController.cpp). DelayTimeController is
  compiled into the ParameterTests translation unit of the same test-runner
  executable, so its symbols are available here via the header only; this
  avoids duplicate definitions of shredspace::DelayTimeController members.
  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "../Source/DSP/FractionalDelay.h"
#include "../Source/DSP/FeedbackController.h"
#include "../Source/DSP/DelayTimeController.h"
#include "../Source/DSP/StereoProcessor.h"

// Compile the DSP implementations this translation unit exercises directly.
#include "../Source/DSP/FractionalDelay.cpp"
#include "../Source/DSP/FeedbackController.cpp"
#include "../Source/DSP/StereoProcessor.cpp"

namespace
{

constexpr float kFloatTol = 1.0e-4f;
constexpr double kDoubleTol = 1.0e-6;

class ShredSpaceDspCoreTests final : public juce::UnitTest
{
public:
    ShredSpaceDspCoreTests()
        : juce::UnitTest ("DSP Core", "ShredSpace DSP")
    {
    }

    void runTest() override
    {
        runImpulsePlacementTest();
        runFractionalInterpolationTest();
        runClampTests();
        runT60DecayTest();
        runTapMedianTest();
        runBpmFallbackTest();
        runStereoProcessorTests();
    }

private:
    // -----------------------------------------------------------------------
    // 1. Impulse first-repeat placement.
    //    For each sample rate, set an integer delay of round(Fs * T) samples,
    //    feed a single impulse, and assert the first repeat appears at exactly
    //    that many samples later.
    // -----------------------------------------------------------------------
    void runImpulsePlacementTest()
    {
        beginTest ("Impulse first-repeat placement at round(Fs * T)");

        const double sampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
        const double T = 0.5; // seconds

        for (const double sampleRate : sampleRates)
        {
            shredspace::FractionalDelay delay;
            delay.prepare (sampleRate, 256);
            delay.reset();

            const int expectedRepeat = static_cast<int> (std::round (sampleRate * T));
            delay.setDelaySamples (static_cast<double> (expectedRepeat));

            int firstRepeatIndex = -1;
            const int totalSamples = expectedRepeat + 32;

            for (int n = 0; n < totalSamples; ++n)
            {
                const float input = (n == 0) ? 1.0f : 0.0f;
                const float output = delay.processSample (input);

                if (firstRepeatIndex < 0 && std::abs (output) > 0.5f)
                    firstRepeatIndex = n;
            }

            expect (firstRepeatIndex == expectedRepeat,
                    "Fs=" + juce::String (sampleRate, 0)
                    + ": first repeat at " + juce::String (firstRepeatIndex)
                    + " expected " + juce::String (expectedRepeat));
        }
    }

    // -----------------------------------------------------------------------
    // 2. Fractional interpolation.
    //    For several mu in [0, 1), place four known samples at buffer indices
    //    0..3 and read at a delay whose internal fractional position equals mu,
    //    then compare against Lagrange basis values computed independently.
    // -----------------------------------------------------------------------
    void runFractionalInterpolationTest()
    {
        beginTest ("FractionalDelay interpolation matches Lagrange basis");

        const double sampleRate = 44100.0;
        const int D = 100; // base integer portion of the delay (well above the 8-sample floor)

        const double muValues[] = { 0.0, 0.1, 0.25, 0.333333, 0.5, 0.666667, 0.75, 0.9, 0.999 };

        // Four distinct known samples placed at buffer indices 0, 1, 2, 3.
        const double c0 = 0.2;
        const double c1 = 0.7;
        const double c2 = -0.4;
        const double c3 = 0.9;

        for (const double mu : muValues)
        {
            shredspace::FractionalDelay delay;
            delay.prepare (sampleRate, 256);
            delay.reset();

            // delay = D + 1 - mu places the read head so that, at sample n = D + 2,
            // absolutePosition = 1 + mu and therefore the internal fractional
            // position used by the interpolator equals mu. The four samples
            // consumed are then exactly buffer[0], buffer[1], buffer[2], buffer[3].
            const double delaySamples = static_cast<double> (D) + 1.0 - mu;
            delay.setDelaySamples (delaySamples);

            // Write the four known samples, then zeros until the read head lines up.
            const int readSampleIndex = D + 2;
            float actualOutput = 0.0f;

            for (int n = 0; n <= readSampleIndex; ++n)
            {
                float input = 0.0f;
                if (n == 0) input = static_cast<float> (c0);
                else if (n == 1) input = static_cast<float> (c1);
                else if (n == 2) input = static_cast<float> (c2);
                else if (n == 3) input = static_cast<float> (c3);

                actualOutput = delay.processSample (input);
            }

            // Independently computed 3rd-order Lagrange basis at mu.
            const double L_minus1 = -mu * (mu - 1.0) * (mu - 2.0) / 6.0;
            const double L_0      =  (mu + 1.0) * (mu - 1.0) * (mu - 2.0) / 2.0;
            const double L_1      = -(mu + 1.0) * mu * (mu - 2.0) / 2.0;
            const double L_2      =  (mu + 1.0) * mu * (mu - 1.0) / 6.0;

            const double expectedOutput =
                L_minus1 * c0 + L_0 * c1 + L_1 * c2 + L_2 * c3;

            expect (std::abs (actualOutput - static_cast<float> (expectedOutput)) <= kFloatTol,
                    "mu=" + juce::String (mu, 6)
                    + ": output=" + juce::String (actualOutput, 6)
                    + " expected=" + juce::String (expectedOutput, 6));
        }
    }

    // -----------------------------------------------------------------------
    // 3. Clamp tests.
    //    A negative effective delay clamps to 8 samples; an over-long request
    //    clamps to 8.25 seconds. Verified on FractionalDelay directly and via
    //    DelayTimeController's resolved target samples.
    // -----------------------------------------------------------------------
    void runClampTests()
    {
        beginTest ("Clamp: minimum 8 samples");

        {
            shredspace::FractionalDelay delay;
            delay.prepare (44100.0, 256);
            delay.reset();

            delay.setDelaySamples (-1.0e6);
            expectEquals (delay.getCurrentDelaySamples(), 8.0,
                          "Negative delay did not clamp to 8 samples");

            delay.setDelaySamples (0.0);
            expectEquals (delay.getCurrentDelaySamples(), 8.0,
                          "Zero delay did not clamp to 8 samples");

            delay.setDelaySamples (4.0);
            expectEquals (delay.getCurrentDelaySamples(), 8.0,
                          "Sub-floor delay did not clamp to 8 samples");
        }

        beginTest ("Clamp: maximum 8.25 seconds");

        const double rates[] = { 44100.0, 48000.0, 96000.0 };
        for (const double sampleRate : rates)
        {
            shredspace::FractionalDelay delay;
            delay.prepare (sampleRate, 256);
            delay.reset();

            delay.setDelaySamples (1.0e12);
            const double expected = shredspace::FractionalDelay::kMaximumDelaySeconds * sampleRate;
            expect (std::abs (delay.getCurrentDelaySamples() - expected) <= kDoubleTol,
                    "Fs=" + juce::String (sampleRate, 0)
                    + ": over-long delay did not clamp to 8.25 s");
        }

        beginTest ("Clamp: DelayTimeController resolved targets");

        {
            shredspace::DelayTimeController controller;
            controller.prepare (44100.0);

            shredspace::ControllerInput input {};
            input.timeMode = 0; // Milliseconds
            input.macroTimeMs = 0.0;
            input.voiceA.timeRatio = 0.0;
            input.voiceA.fineOffsetMs = -1000.0; // strongly negative effective time

            shredspace::HostTiming timing { 120.0, true, false, 0 };

            auto results = controller.update (input, timing);
            expectEquals (results[0].targetDelaySamples, 8.0,
                          "Negative effective time did not resolve to 8 samples");

            input.macroTimeMs = 90000.0;        // 90 s
            input.voiceA.timeRatio = 2.0;
            input.voiceA.fineOffsetMs = 100000.0; // +100 s -> far above the ceiling
            results = controller.update (input, timing);
            const double expectedMax = shredspace::FractionalDelay::kMaximumDelaySeconds * 44100.0;
            expect (std::abs (results[0].targetDelaySamples - expectedMax) <= 1.0e-2,
                    "Over-long effective time did not resolve to 8.25 s");
        }
    }

    // -----------------------------------------------------------------------
    // 4. T60 decay.
    //    With feedback gain g = 10^(-3*T/T60), amplitude after T60 seconds is
    //    g^(T60/T) = 10^-3, so 20*log10(A(T60)/A(0)) = -60 dB. Also assert the
    //    returned feedback gain never exceeds 0.985, including Hold-like
    //    (effectively infinite T60) conditions.
    // -----------------------------------------------------------------------
    void runT60DecayTest()
    {
        beginTest ("T60 decay is approximately -60 dB");

        const double T60 = 1.0;
        const double T = 0.05;
        const double tailScaleVoice = 1.0;

        const double gain = shredspace::FeedbackController::calculateGain (T60, tailScaleVoice, T);
        const double amplitudeRatio = std::pow (gain, T60 / T);
        const double decayDb = 20.0 * std::log10 (amplitudeRatio);

        expect (std::abs (decayDb - (-60.0)) <= 0.5,
                "T60 decay: " + juce::String (decayDb, 4) + " dB (expected ~-60)");

        beginTest ("Feedback gain never exceeds 0.985");

        expectEquals (shredspace::FeedbackController::kMaxFeedbackGain, 0.985);

        const double t60Values[]   = { 1.0e-3, 0.1, 1.0, 10.0, 1000.0, 1.0e6 };
        const double tailValues[]  = { 0.1, 1.0, 10.0 };
        const double timeValues[]  = { 1.0e-4, 0.01, 0.1, 1.0 };

        for (const double t60 : t60Values)
            for (const double tail : tailValues)
                for (const double tt : timeValues)
                {
                    const double g = shredspace::FeedbackController::calculateGain (t60, tail, tt);
                    expect (g <= shredspace::FeedbackController::kMaxFeedbackGain + 1.0e-9,
                            "Gain exceeded ceiling: " + juce::String (g, 6)
                            + " for T60=" + juce::String (t60, 3)
                            + " tail=" + juce::String (tail, 3)
                            + " T=" + juce::String (tt, 3));
                }

        // Hold simulates an arbitrarily long tail; gain must still be capped.
        const double holdGain = shredspace::FeedbackController::calculateGain (1.0e9, 100.0, 1.0e-12);
        expect (holdGain <= shredspace::FeedbackController::kMaxFeedbackGain + 1.0e-9,
                "Hold-like gain exceeded ceiling: " + juce::String (holdGain, 6));

        // Smoothing path must also respect the ceiling sample-by-sample.
        shredspace::FeedbackController controller;
        controller.prepare (44100.0);
        controller.reset (1.0);
        controller.setT60Target (1.0e9); // Hold-like infinite tail

        for (int i = 0; i < 2000; ++i)
        {
            const double g = controller.processSample (0.05, 1.0);
            expect (g <= shredspace::FeedbackController::kMaxFeedbackGain + 1.0e-9,
                    "Smoothed gain exceeded ceiling at sample " + juce::String (i));
        }
    }

    // -----------------------------------------------------------------------
    // 5. Tap median.
    //    Five tap intervals, one of which is an outlier outside [100, 3000] ms,
    //    produce the median of the accepted intervals. The outlier is rejected.
    // -----------------------------------------------------------------------
    void runTapMedianTest()
    {
        beginTest ("Tap median rejects outlier outside 100-3000 ms");

        shredspace::DelayTimeController controller;
        controller.prepare (44100.0);

        // Intervals: 0.4, 0.5, 0.6, 0.7 (all accepted) and 5.0 (rejected).
        controller.tap (0.0);
        controller.tap (0.4);
        controller.tap (0.9);
        controller.tap (1.5);
        controller.tap (2.2);
        controller.tap (7.2); // interval 5.0 s -> rejected

        const double interval = controller.getTapInterval();
        const double expectedMedian = 0.55; // median of [0.4, 0.5, 0.6, 0.7]
        expect (std::abs (interval - expectedMedian) <= kDoubleTol,
                "Tap interval=" + juce::String (interval, 6)
                + " expected median=" + juce::String (expectedMedian, 6));

        // If the 5.0 s outlier had been accepted, the median of
        // [0.4, 0.5, 0.6, 0.7, 5.0] would be 0.6, so 0.55 proves rejection.
        expect (std::abs (interval - 0.6) > 1.0e-3,
                "Outlier appears to have been accepted (interval ~= 0.6)");

        shredspace::ControllerInput input {};
        input.timeMode = 2; // Tap
        input.voiceA.timeRatio = 1.0;
        input.voiceA.fineOffsetMs = 0.0;

        shredspace::HostTiming timing { 120.0, true, false, 0 };
        const auto results = controller.update (input, timing);
        expect (std::abs (results[0].targetDelaySamples - 0.55 * 44100.0) <= 1.0e-2,
                "Tap target samples=" + juce::String (results[0].targetDelaySamples, 2)
                + " expected " + juce::String (0.55 * 44100.0, 2));
    }

    // -----------------------------------------------------------------------
    // 6. BPM fallback.
    //    With bpmValid false and no prior valid BPM, the resolved tempo is
    //    120 BPM. After a valid BPM is seen, that last valid BPM is retained
    //    when bpmValid goes false again.
    // -----------------------------------------------------------------------
    void runBpmFallbackTest()
    {
        beginTest ("BPM fallback to 120 and last-valid-BPM retention");

        shredspace::DelayTimeController controller;
        controller.prepare (48000.0);

        shredspace::ControllerInput input {};
        input.timeMode = 1; // Host Sync
        input.syncDivision = 6; // 1/4 -> ratio 1.0
        input.voiceA.timeRatio = 1.0;
        input.voiceA.fineOffsetMs = 0.0;

        // 1. No valid BPM yet -> fallback to 120 BPM (quarter = 0.5 s -> 24000).
        shredspace::HostTiming timing { 0.0, false, false, 0 };
        auto results = controller.update (input, timing);
        expect (std::abs (results[0].targetDelaySamples - 0.5 * 48000.0) <= 1.0e-2,
                "Initial fallback: " + juce::String (results[0].targetDelaySamples, 2)
                + " expected " + juce::String (0.5 * 48000.0, 2));

        // 2. A valid BPM is reported (100 BPM -> quarter = 0.6 s -> 28800).
        timing.bpm = 100.0;
        timing.bpmValid = true;
        results = controller.update (input, timing);
        expect (std::abs (results[0].targetDelaySamples - 0.6 * 48000.0) <= 1.0e-2,
                "Valid BPM: " + juce::String (results[0].targetDelaySamples, 2)
                + " expected " + juce::String (0.6 * 48000.0, 2));

        // 3. bpmValid goes false with a different bpm value -> last valid (100) retained.
        timing.bpm = 200.0;
        timing.bpmValid = false;
        results = controller.update (input, timing);
        expect (std::abs (results[0].targetDelaySamples - 0.6 * 48000.0) <= 1.0e-2,
                "Retained BPM: " + juce::String (results[0].targetDelaySamples, 2)
                + " expected " + juce::String (0.6 * 48000.0, 2)
                + " (200 BPM would give " + juce::String (0.3 * 48000.0, 2) + ")");
    }

    // -----------------------------------------------------------------------
    // 7. StereoProcessor tests.
    //    Verifies constant-power pan, mid/side width, low-frequency mono crossover,
    //    and correlation telemetry.
    // -----------------------------------------------------------------------
    void runStereoProcessorTests()
    {
        beginTest ("StereoProcessor - Constant-power pan");
        {
            shredspace::StereoProcessor sp;
            sp.prepare (44100.0, 256);
            sp.setPan (0.0f);
            sp.setWidth (1.0f);
            sp.setLowFrequencyMonoEnabled (false);

            sp.reset();

            // Check center pan (p = 0)
            float left[1] = { 1.0f };
            float right[1] = { 1.0f };
            sp.process (left, right, 1);
            
            // At p = 0, gain_L = gain_R = cos(pi/4) = 1/sqrt(2) = 0.70710678
            expect (std::abs (left[0] - 0.70710678f) <= kFloatTol, "Center pan left mismatch");
            expect (std::abs (right[0] - 0.70710678f) <= kFloatTol, "Center pan right mismatch");

            // Test power conservation for various pan positions
            for (float p = -1.0f; p <= 1.0f; p += 0.2f)
            {
                sp.setPan (p);
                // Run enough samples for the 30 ms pan smoother to reach target
                // 30 ms at 44100 Hz is 1323 samples. Run 2000 samples.
                float lBuf[2000];
                float rBuf[2000];
                std::fill (std::begin (lBuf), std::end (lBuf), 1.0f);
                std::fill (std::begin (rBuf), std::end (rBuf), 1.0f);
                sp.process (lBuf, rBuf, 2000);

                const float finalL = lBuf[1999];
                const float finalR = rBuf[1999];
                const float power = finalL * finalL + finalR * finalR;
                
                expect (std::abs (power - 1.0f) <= kFloatTol, "Power conservation failed at p=" + juce::String (p, 2));

                if (p == -1.0f)
                {
                    expect (std::abs (finalL - 1.0f) <= kFloatTol, "Left-pan finalL mismatch");
                    expect (std::abs (finalR - 0.0f) <= kFloatTol, "Left-pan finalR mismatch");
                }
                else if (p == 1.0f)
                {
                    expect (std::abs (finalL - 0.0f) <= kFloatTol, "Right-pan finalL mismatch");
                    expect (std::abs (finalR - 1.0f) <= kFloatTol, "Right-pan finalR mismatch");
                }
            }
        }

        beginTest ("StereoProcessor - Mid/side width");
        {
            shredspace::StereoProcessor sp;
            sp.prepare (44100.0, 256);
            sp.setPan (0.0f); // Center pan -> gain = 1/sqrt(2)
            sp.setLowFrequencyMonoEnabled (false);

            // Check identity round-trip at w = 1
            sp.setWidth (1.0f);
            sp.reset();
            
            float left[1] = { 1.0f };
            float right[1] = { 0.0f };
            sp.process (left, right, 1);
            expect (std::abs (left[0] - 0.70710678f) <= kFloatTol, "Width identity left mismatch");
            expect (std::abs (right[0] - 0.0f) <= kFloatTol, "Width identity right mismatch");

            // Check mono sum at w = 0 (mono width)
            sp.setWidth (0.0f);
            float lBuf[3000];
            float rBuf[3000];
            std::fill (std::begin (lBuf), std::end (lBuf), 1.0f);
            std::fill (std::begin (rBuf), std::end (rBuf), 0.0f);
            sp.process (lBuf, rBuf, 3000);
            
            // At w = 0, M = (1+0)/sqrt(2) = 1/sqrt(2), L_w = 0.5, R_w = 0.5
            // After pan: L_out = 0.5 * 1/sqrt(2) = 0.35355339
            expect (std::abs (lBuf[2999] - 0.35355339f) <= kFloatTol, "Width mono left mismatch");
            expect (std::abs (rBuf[2999] - 0.35355339f) <= kFloatTol, "Width mono right mismatch");
        }

        beginTest ("StereoProcessor - Low-frequency mono");
        {
            const double sampleRate = 44100.0;
            shredspace::StereoProcessor sp;
            sp.prepare (sampleRate, 256);
            sp.setPan (0.0f); // Center pan -> gain = 1/sqrt(2)
            sp.setWidth (1.0f); // Identity width
            sp.setLowFrequencyMonoCrossover (180.0f);
            sp.setLowFrequencyMonoEnabled (true);
            sp.reset();

            // Run a low frequency sine wave (50 Hz) on Left only
            float lBuf[4000];
            float rBuf[4000];
            for (int n = 0; n < 4000; ++n)
            {
                lBuf[n] = std::sin (2.0f * static_cast<float> (juce::MathConstants<double>::pi) * 50.0f * static_cast<float> (n) / static_cast<float> (sampleRate));
                rBuf[n] = 0.0f;
            }
            sp.process (lBuf, rBuf, 4000);

            // Should collapse to mono, Left and Right should be very close
            for (int n = 3900; n < 4000; ++n)
            {
                expect (std::abs (lBuf[n] - rBuf[n]) <= 0.05f, "Low frequency did not collapse to mono");
            }

            // Run a high frequency sine wave (5000 Hz) on Left only
            sp.reset();
            for (int n = 0; n < 4000; ++n)
            {
                lBuf[n] = std::sin (2.0f * static_cast<float> (juce::MathConstants<double>::pi) * 5000.0f * static_cast<float> (n) / static_cast<float> (sampleRate));
                rBuf[n] = 0.0f;
            }
            sp.process (lBuf, rBuf, 4000);

            // Right channel should be mostly muted
            float rMax = 0.0f;
            for (int n = 3900; n < 4000; ++n)
            {
                rMax = std::max (rMax, std::abs (rBuf[n]));
            }
            expect (rMax < 0.05f, "High frequency on right channel is too large: " + juce::String (rMax));
        }

        beginTest ("StereoProcessor - Correlation telemetry");
        {
            shredspace::StereoProcessor sp;
            sp.prepare (44100.0, 256);
            sp.setPan (0.0f);
            sp.setWidth (1.0f);
            sp.setLowFrequencyMonoEnabled (false);
            sp.reset();

            // Test 1: In-phase (L == R)
            float lBuf[4000];
            float rBuf[4000];
            std::fill (std::begin (lBuf), std::end (lBuf), 1.0f);
            std::fill (std::begin (rBuf), std::end (rBuf), 1.0f);
            
            for (int start = 0; start < 4000; start += 256)
            {
                int num = std::min (256, 4000 - start);
                sp.process (lBuf + start, rBuf + start, num);
            }
            expect (std::abs (sp.getCorrelation() - 1.0f) <= kFloatTol, "Correlation L==R mismatch: " + juce::String (sp.getCorrelation()));

            // Test 2: Out-of-phase (L == -R)
            sp.reset();
            std::fill (std::begin (lBuf), std::end (lBuf), 1.0f);
            std::fill (std::begin (rBuf), std::end (rBuf), -1.0f);
            for (int start = 0; start < 4000; start += 256)
            {
                int num = std::min (256, 4000 - start);
                sp.process (lBuf + start, rBuf + start, num);
            }
            expect (std::abs (sp.getCorrelation() - -1.0f) <= kFloatTol, "Correlation L==-R mismatch: " + juce::String (sp.getCorrelation()));

            // Test 3: Orthogonal (L only)
            sp.reset();
            std::fill (std::begin (lBuf), std::end (lBuf), 1.0f);
            std::fill (std::begin (rBuf), std::end (rBuf), 0.0f);
            for (int start = 0; start < 4000; start += 256)
            {
                int num = std::min (256, 4000 - start);
                sp.process (lBuf + start, rBuf + start, num);
            }
            expect (std::abs (sp.getCorrelation() - 0.0f) <= kFloatTol, "Correlation L only mismatch: " + juce::String (sp.getCorrelation()));
        }
    }
};

// Static instance registers the test with juce::UnitTestRunner.
static ShredSpaceDspCoreTests sShredSpaceDspCoreTestsInstance;

} // namespace
