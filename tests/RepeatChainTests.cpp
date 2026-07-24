/*
  ==============================================================================
  Repeat-chain response and transparency tests for ShredSpace. Uses ONLY
  juce::UnitTest; no third-party test frameworks (Catch2, GoogleTest, etc.)
  are involved.

  Coverage:
    1. RBJ low-pass magnitude response via RepeatToneProcessor::magnitudeAt.
    2. RBJ high-pass magnitude response.
    3. RBJ peaking magnitude response (+/-6 dB, symmetry).
    4. Transient softener bit-transparency at zero amount (complementary
       subtraction reconstruction identity).
    5. Transient softener action at 100% amount: high-band attack peak reduced
       by >= 1.5 dB while steady low-band RMS preserved within 0.2 dB.
    6. PresenceGuard applies no cut below the -24 dB threshold.
    7. PresenceGuard computeCutDb: zero at/below threshold, monotonically
       non-increasing across the knee, clamps at -maxCutDb above the knee.
    8. PresenceGuard never exceeds the configured maximum cut for loud input.
    9. PresenceGuard squared-energy detector shows < 0.01 dB drift between
       the 1 s and 2 s marks of a constant presence tone (no epsilon drift).

  Implementation note: RepeatToneProcessor.cpp, PresenceGuard.cpp and
  RepeatTransientSoftener.cpp are compiled directly into this translation
  unit (mirroring the pattern used by DspCoreTests.cpp for the other DSP
  translations). They are not part of the ShredSpaceTests target_sources
  list, so this avoids duplicate definitions.
  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "../Source/DSP/RepeatToneProcessor.h"
#include "../Source/DSP/PresenceGuard.h"
#include "../Source/DSP/RepeatTransientSoftener.h"

// Compile the DSP implementations this translation unit exercises directly.
#include "../Source/DSP/RepeatToneProcessor.cpp"
#include "../Source/DSP/PresenceGuard.cpp"
#include "../Source/DSP/RepeatTransientSoftener.cpp"

namespace
{
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr float  kButterworthQ = 0.70710678f;
constexpr double kDetectorHalfBandRatio = 1.4142135623730951;
constexpr double kDetectorQ = 0.7071067811865476;

double toDb (double linearMagnitude)
{
    return 20.0 * std::log10 (std::max (linearMagnitude, 1.0e-12));
}

// Inverts the smoothstep mapping used by PresenceGuard::computeCutDb.
// Given s = -cutDb / maxCutDb in [0, 1], returns u in [0, 1] such that
// 3u^2 - 2u^3 = s. Newton iteration from u = 0.5.
double invertSmoothstep (double s)
{
    if (s <= 0.0) return 0.0;
    if (s >= 1.0) return 1.0;

    double u = 0.5;
    for (int i = 0; i < 30; ++i)
    {
        const double f  = 3.0 * u * u - 2.0 * u * u * u - s;
        const double fp = 6.0 * u * (1.0 - u);
        if (std::abs (fp) < 1.0e-15)
            break;
        u -= f / fp;
        u = std::clamp (u, 0.0, 1.0);
    }
    return u;
}

class RepeatChainTests final : public juce::UnitTest
{
public:
    RepeatChainTests()
        : juce::UnitTest ("Repeat Chain", "ShredSpace DSP")
    {
    }

    void runTest() override
    {
        runLowPassResponseTest();
        runHighPassResponseTest();
        runPeakingResponseTest();
        runSoftenerTransparencyTest();
        runSoftenerActionTest();
        runGuardBelowThresholdTest();
        runGuardKneeMonotonicTest();
        runGuardMaxCutTest();
        runGuardEpsilonDriftTest();
    }

private:
    // -----------------------------------------------------------------------
    // 1. Low-pass magnitude response.
    //    Fs = 48000, cutoff = 4000 Hz, Butterworth Q. At the cutoff the
    //    magnitude is 1/sqrt(2); well below cutoff it is ~unity; well above
    //    it is strongly attenuated.
    // -----------------------------------------------------------------------
    void runLowPassResponseTest()
    {
        beginTest ("Low-pass magnitude response");

        const double Fs = 48000.0;
        const double cutoff = 4000.0;
        const auto c = shredspace::RepeatToneProcessor::makeLowPass (
            Fs, cutoff, kButterworthQ);

        const double atCutoff = shredspace::RepeatToneProcessor::magnitudeAt (c, Fs, cutoff);
        const double at200    = shredspace::RepeatToneProcessor::magnitudeAt (c, Fs, 200.0);
        const double at16000  = shredspace::RepeatToneProcessor::magnitudeAt (c, Fs, 16000.0);

        const double half = 1.0 / std::sqrt (2.0);

        expect (std::abs (atCutoff - half) <= 0.05,
                "LP cutoff magnitude " + juce::String (atCutoff, 6)
                + " not within 0.05 of 1/sqrt(2) (" + juce::String (half, 6) + ")");

        expect (at200 > 0.95,
                "LP 200 Hz magnitude " + juce::String (at200, 6) + " not above 0.95");

        expect (at16000 < 0.15,
                "LP 16000 Hz magnitude " + juce::String (at16000, 6) + " not below 0.15");
    }

    // -----------------------------------------------------------------------
    // 2. High-pass magnitude response.
    //    Fs = 48000, cutoff = 200 Hz, Butterworth Q.
    // -----------------------------------------------------------------------
    void runHighPassResponseTest()
    {
        beginTest ("High-pass magnitude response");

        const double Fs = 48000.0;
        const double cutoff = 200.0;
        const auto c = shredspace::RepeatToneProcessor::makeHighPass (
            Fs, cutoff, kButterworthQ);

        const double atCutoff = shredspace::RepeatToneProcessor::magnitudeAt (c, Fs, cutoff);
        const double at20     = shredspace::RepeatToneProcessor::magnitudeAt (c, Fs, 20.0);
        const double at4000   = shredspace::RepeatToneProcessor::magnitudeAt (c, Fs, 4000.0);

        const double half = 1.0 / std::sqrt (2.0);

        expect (std::abs (atCutoff - half) <= 0.05,
                "HP cutoff magnitude " + juce::String (atCutoff, 6)
                + " not within 0.05 of 1/sqrt(2) (" + juce::String (half, 6) + ")");

        expect (at20 < 0.15,
                "HP 20 Hz magnitude " + juce::String (at20, 6) + " not below 0.15");

        expect (at4000 > 0.95,
                "HP 4000 Hz magnitude " + juce::String (at4000, 6) + " not above 0.95");
    }

    // -----------------------------------------------------------------------
    // 3. Peaking magnitude response.
    //    +6 dB / -6 dB at 3000 Hz, Q = 1.0. Center gain matches the requested
    //    gain; far from center the response is unity. The -6 dB case is the
    //    symmetric reciprocal of the +6 dB case.
    // -----------------------------------------------------------------------
    void runPeakingResponseTest()
    {
        beginTest ("Peaking magnitude response (+/-6 dB, symmetry)");

        const double Fs = 48000.0;
        const double fc = 3000.0;
        const double q  = 1.0;

        for (const double gainDb : { 6.0, -6.0 })
        {
            const auto c = shredspace::RepeatToneProcessor::makePeaking (
                Fs, fc, q, gainDb);

            const double magCenter = shredspace::RepeatToneProcessor::magnitudeAt (c, Fs, fc);
            const double mag100    = shredspace::RepeatToneProcessor::magnitudeAt (c, Fs, 100.0);
            const double mag16000  = shredspace::RepeatToneProcessor::magnitudeAt (c, Fs, 16000.0);

            expect (std::abs (toDb (magCenter) - gainDb) <= 0.1,
                    "Peak " + juce::String (gainDb, 1) + " dB: center "
                    + juce::String (toDb (magCenter), 4) + " dB not within 0.1 dB");

            expect (std::abs (toDb (mag100) - 0.0) <= 0.5,
                    "Peak " + juce::String (gainDb, 1) + " dB: 100 Hz "
                    + juce::String (toDb (mag100), 4) + " dB not within 0.5 dB of unity");

            expect (std::abs (toDb (mag16000) - 0.0) <= 0.5,
                    "Peak " + juce::String (gainDb, 1) + " dB: 16000 Hz "
                    + juce::String (toDb (mag16000), 4) + " dB not within 0.5 dB of unity");
        }

        // Symmetry: |H_{+6}| * |H_{-6}| == 1 at every frequency (reciprocal).
        const auto cPos = shredspace::RepeatToneProcessor::makePeaking (Fs, fc, q,  6.0);
        const auto cNeg = shredspace::RepeatToneProcessor::makePeaking (Fs, fc, q, -6.0);

        const double freqs[] = { 100.0, 1000.0, 3000.0, 16000.0 };
        for (const double f : freqs)
        {
            const double product = shredspace::RepeatToneProcessor::magnitudeAt (cPos, Fs, f)
                                 * shredspace::RepeatToneProcessor::magnitudeAt (cNeg, Fs, f);
            expect (std::abs (toDb (product) - 0.0) <= 0.1,
                    "Symmetry at " + juce::String (f, 1) + " Hz: product "
                    + juce::String (toDb (product), 4) + " dB not within 0.1 dB of unity");
        }
    }

    // -----------------------------------------------------------------------
    // 4. Softener transparency at zero amount.
    //    With setAmountPercent(0), the complementary-subtraction reconstruction
    //    identity (zLow + 1.0 * zHigh == x) makes the processor bit-transparent
    //    within floating-point tolerance for every sample.
    // -----------------------------------------------------------------------
    void runSoftenerTransparencyTest()
    {
        beginTest ("Softener transparency at zero amount (1.0e-4 per sample)");

        const double Fs = 48000.0;
        const int N = 2048;

        shredspace::RepeatTransientSoftener softener;
        softener.prepare (Fs, 256, 1);
        softener.reset();
        softener.setAmountPercent (0.0f);

        for (int n = 0; n < N; ++n)
        {
            const double t = static_cast<double> (n) / Fs;
            const double step = (n >= N / 2) ? 0.4 : 0.0; // abrupt step transient mid-signal
            const double x = 0.5 * std::sin (2.0 * kPi * 200.0 * t)
                           + 0.3 * std::sin (2.0 * kPi * 4000.0 * t)
                           + step;

            const float xf = static_cast<float> (x);
            const float y  = softener.processSample (0, xf);

            expect (std::abs (y - xf) <= 1.0e-4f,
                    "Sample " + juce::String (n)
                    + ": |out - in| = " + juce::String (std::abs (y - xf), 8)
                    + " exceeds 1.0e-4");
        }
    }

    // -----------------------------------------------------------------------
    // 5. Softener action at 100% amount.
    //    An abrupt high-frequency attack is attenuated by >= 1.5 dB (peak in
    //    the first 5 ms after onset) while a steady 200 Hz low-band tone has
    //    its RMS preserved within 0.2 dB.
    // -----------------------------------------------------------------------
    void runSoftenerActionTest()
    {
        beginTest ("Softener action at 100% amount (attack reduced, low band preserved)");

        const double Fs = 48000.0;

        // Build the attack signal: silence pre-roll (so the detector's slow
        // envelope is at zero and the full attenuation engages from the first
        // attack sample), then an abrupt 6000 Hz high-frequency burst. A burst
        // well above the 2500 Hz crossover exercises the high-band path.
        const int preRoll = 1000;
        const int attackLen = 240; // 5 ms at 48000 Hz
        const int totalAttack = preRoll + attackLen;
        const double attackFreq = 6000.0;

        std::vector<float> attackSignal (totalAttack, 0.0f);
        for (int n = preRoll; n < totalAttack; ++n)
        {
            const double t = static_cast<double> (n - preRoll) / Fs;
            attackSignal[n] = static_cast<float> (0.8 * std::sin (2.0 * kPi * attackFreq * t));
        }

        // Low-band-only steady signal for the RMS-preservation check.
        const int lowTotal = 3000;
        std::vector<float> lowSignal (lowTotal, 0.0f);
        for (int n = 0; n < lowTotal; ++n)
        {
            const double t = static_cast<double> (n) / Fs;
            lowSignal[n] = static_cast<float> (0.5 * std::sin (2.0 * kPi * 200.0 * t));
        }

        // Helper to run a signal through a fresh softener at a given amount.
        auto run = [&] (float amountPercent, const std::vector<float>& signal)
        {
            shredspace::RepeatTransientSoftener s;
            s.prepare (Fs, 256, 1);
            s.reset();
            s.setAmountPercent (amountPercent);

            std::vector<float> out (signal.size(), 0.0f);
            for (size_t i = 0; i < signal.size(); ++i)
                out[i] = s.processSample (0, signal[i]);
            return out;
        };

        const auto outAttack0   = run (0.0f,   attackSignal);
        const auto outAttack100 = run (100.0f, attackSignal);

        // Peak absolute output in the first 5 ms after the attack onset.
        float peak0 = 0.0f, peak100 = 0.0f;
        for (int i = 0; i < attackLen; ++i)
        {
            const int idx = preRoll + i;
            peak0   = std::max (peak0,   std::abs (outAttack0[idx]));
            peak100 = std::max (peak100, std::abs (outAttack100[idx]));
        }

        expect (peak0 > 0.0f, "Zero-amount attack peak is zero; signal malformed");

        const double reductionDb = toDb (static_cast<double> (peak100)
                                         / static_cast<double> (peak0));
        expect (reductionDb <= -1.5,
                "Attack peak reduction " + juce::String (reductionDb, 4)
                + " dB not <= -1.5 dB (peak0=" + juce::String (peak0, 6)
                + " peak100=" + juce::String (peak100, 6) + ")");

        // Low-band RMS preservation over a steady window (post-settling).
        const auto outLow0   = run (0.0f,   lowSignal);
        const auto outLow100 = run (100.0f, lowSignal);

        auto rms = [] (const std::vector<float>& v, int lo, int hi) -> double
        {
            double acc = 0.0;
            for (int i = lo; i < hi; ++i)
                acc += static_cast<double> (v[i]) * static_cast<double> (v[i]);
            return std::sqrt (acc / static_cast<double> (hi - lo));
        };

        const int lo = 2000, hi = 3000;
        const double rms0   = rms (outLow0,   lo, hi);
        const double rms100 = rms (outLow100, lo, hi);

        expect (rms0 > 0.0, "Low-band RMS is zero; signal malformed");

        const double rmsDeltaDb = toDb (rms100 / rms0);
        expect (std::abs (rmsDeltaDb) <= 0.2,
                "Low-band RMS delta " + juce::String (rmsDeltaDb, 4)
                + " dB not within 0.2 dB (rms0=" + juce::String (rms0, 6)
                + " rms100=" + juce::String (rms100, 6) + ")");
    }

    // -----------------------------------------------------------------------
    // 6. Guard below threshold.
    //    A presence-band tone well below the -24 dB threshold produces no cut.
    // -----------------------------------------------------------------------
    void runGuardBelowThresholdTest()
    {
        beginTest ("Guard below threshold: no cut after settling");

        const double Fs = 48000.0;
        const double freq = 3000.0;
        const double amplitude = 0.01; // ~ -40 dBFS, well below -24 dB threshold

        shredspace::PresenceGuard guard;
        guard.prepare (Fs, 256, 1);
        guard.reset();

        const int N = static_cast<int> (0.5 * Fs); // 0.5 s
        for (int n = 0; n < N; ++n)
        {
            const double t = static_cast<double> (n) / Fs;
            const float x = static_cast<float> (amplitude * std::sin (2.0 * kPi * freq * t));
            guard.processSample (0, x);

            if ((n + 1) % 32 == 0)
                guard.updateCoefficients();
        }

        const float cutDb = guard.getCurrentCutDb (0);
        expect (std::abs (cutDb) <= 0.05f,
                "Below-threshold cut " + juce::String (cutDb, 6)
                + " dB not within 0.05 dB of 0");
    }

    // -----------------------------------------------------------------------
    // 7. Guard knee monotonicity and clamping (computeCutDb).
    //    Zero at/below threshold, non-increasing across the knee, exactly
    //    -maxCutDb above threshold + kneeWidth.
    // -----------------------------------------------------------------------
    void runGuardKneeMonotonicTest()
    {
        beginTest ("Guard computeCutDb: monotonic knee, clamp at -maxCutDb");

        const float thresholdDb = -24.0f;
        const float kneeWidthDb = 12.0f;
        const float maxCutDb    = 6.0f;

        // Zero at and below threshold.
        for (float levelDb : { -100.0f, -30.0f, -24.001f, -24.0f })
        {
            const float cut = shredspace::PresenceGuard::computeCutDb (
                levelDb, thresholdDb, kneeWidthDb, maxCutDb);
            expect (cut == 0.0f,
                    "computeCutDb at level " + juce::String (levelDb, 3)
                    + " dB is " + juce::String (cut, 6) + ", expected 0");
        }

        // Non-increasing as levelDb rises across the knee.
        float prev = 0.0f;
        const int steps = 64;
        for (int i = 0; i <= steps; ++i)
        {
            const float levelDb = thresholdDb + (kneeWidthDb * static_cast<float> (i) / steps);
            const float cut = shredspace::PresenceGuard::computeCutDb (
                levelDb, thresholdDb, kneeWidthDb, maxCutDb);

            expect (cut <= prev + 1.0e-6f,
                    "computeCutDb not non-increasing at level " + juce::String (levelDb, 4)
                    + " dB: cut=" + juce::String (cut, 6) + " prev=" + juce::String (prev, 6));
            prev = cut;
        }

        // Clamps at exactly -maxCutDb at and above threshold + kneeWidth.
        for (float levelDb : { thresholdDb + kneeWidthDb,
                               thresholdDb + kneeWidthDb + 0.001f,
                               thresholdDb + kneeWidthDb + 1.0f,
                               0.0f,
                               12.0f })
        {
            const float cut = shredspace::PresenceGuard::computeCutDb (
                levelDb, thresholdDb, kneeWidthDb, maxCutDb);
            expect (cut == -maxCutDb,
                    "computeCutDb at level " + juce::String (levelDb, 3)
                    + " dB is " + juce::String (cut, 6)
                    + ", expected " + juce::String (-maxCutDb, 6));
        }
    }

    // -----------------------------------------------------------------------
    // 8. Guard max-cut respect.
    //    A very loud presence-band input never drives getCurrentCutDb() below
    //    -maxCutDb.
    // -----------------------------------------------------------------------
    void runGuardMaxCutTest()
    {
        beginTest ("Guard never exceeds configured maximum cut");

        const double Fs = 48000.0;
        const double freq = 3000.0;
        const double amplitude = 1.0; // loud presence tone

        shredspace::PresenceGuard guard;
        guard.prepare (Fs, 256, 1);
        guard.reset();
        guard.setMaxCutDb (6.0f);

        const float maxCutDb = 6.0f;
        const int N = static_cast<int> (0.5 * Fs);
        float minCutDb = 0.0f;

        for (int n = 0; n < N; ++n)
        {
            const double t = static_cast<double> (n) / Fs;
            const float x = static_cast<float> (amplitude * std::sin (2.0 * kPi * freq * t));
            guard.processSample (0, x);

            if ((n + 1) % 32 == 0)
                guard.updateCoefficients();

            const float cut = guard.getCurrentCutDb (0);
            minCutDb = std::min (minCutDb, cut);
            expect (cut >= -maxCutDb - 1.0e-4f,
                    "Cut " + juce::String (cut, 6) + " dB below -maxCutDb ("
                    + juce::String (-maxCutDb, 6) + ") at sample " + juce::String (n));
        }

        expect (minCutDb >= -maxCutDb - 1.0e-4f,
                "Minimum cut " + juce::String (minCutDb, 6)
                + " dB below -maxCutDb (" + juce::String (-maxCutDb, 6) + ")");
    }

    // -----------------------------------------------------------------------
    // 9. Guard epsilon-accumulation drift.
    //    A constant-amplitude presence tone for >= 2 s. The detector level
    //    implied by getCurrentCutDb() at the 1 s and 2 s marks must differ by
    //    less than 0.01 dB, proving the squared-energy state carries no
    //    epsilon drift.
    // -----------------------------------------------------------------------
    void runGuardEpsilonDriftTest()
    {
        beginTest ("Guard epsilon drift < 0.01 dB between 1 s and 2 s");

        const double Fs = 48000.0;
        const double freq = 3000.0;
        const float thresholdDb = -24.0f;
        const float kneeWidthDb = 12.0f;
        const float maxCutDb    = 6.0f;

        // Calibrate the input amplitude so the detector level lands mid-knee
        // (~ -18 dB), where the cut mapping is invertible and sensitive.
        const auto hp = shredspace::RepeatToneProcessor::makeHighPass (
            Fs, freq / kDetectorHalfBandRatio, kDetectorQ);
        const auto lp = shredspace::RepeatToneProcessor::makeLowPass (
            Fs, freq * kDetectorHalfBandRatio, kDetectorQ);
        const double bandpassGain =
            shredspace::RepeatToneProcessor::magnitudeAt (hp, Fs, freq)
            * shredspace::RepeatToneProcessor::magnitudeAt (lp, Fs, freq);

        expect (bandpassGain > 0.0, "Detector bandpass gain is non-positive");

        // Detector energy for a sine of amplitude A converges to A*gain/sqrt(2).
        // Choose A so that level ~= -18 dB (mid-knee).
        const double targetLevelDb = -18.0;
        const double amplitude =
            std::pow (10.0, targetLevelDb / 20.0) * std::sqrt (2.0) / bandpassGain;

        shredspace::PresenceGuard guard;
        guard.prepare (Fs, 256, 1);
        guard.reset();
        guard.setMaxCutDb (maxCutDb);

        const int N = static_cast<int> (2.0 * Fs); // 2 s
        float cutAt1s = 0.0f;
        float cutAt2s = 0.0f;

        for (int n = 0; n < N; ++n)
        {
            const double t = static_cast<double> (n) / Fs;
            const float x = static_cast<float> (amplitude * std::sin (2.0 * kPi * freq * t));
            guard.processSample (0, x);

            if ((n + 1) % 32 == 0)
                guard.updateCoefficients();

            if (n == static_cast<int> (1.0 * Fs) - 1)
                cutAt1s = guard.getCurrentCutDb (0);
            if (n == N - 1)
                cutAt2s = guard.getCurrentCutDb (0);
        }

        // Both cuts must lie strictly inside the knee so the level mapping is
        // invertible and meaningful.
        expect (cutAt1s < 0.0f && cutAt1s > -maxCutDb,
                "Cut at 1 s (" + juce::String (cutAt1s, 6)
                + " dB) not strictly inside knee (-" + juce::String (maxCutDb, 3) + ", 0)");
        expect (cutAt2s < 0.0f && cutAt2s > -maxCutDb,
                "Cut at 2 s (" + juce::String (cutAt2s, 6)
                + " dB) not strictly inside knee (-" + juce::String (maxCutDb, 3) + ", 0)");

        // Invert cut -> detector levelDb via the smoothstep mapping.
        const double s1 = static_cast<double> (-cutAt1s) / maxCutDb;
        const double s2 = static_cast<double> (-cutAt2s) / maxCutDb;
        const double u1 = invertSmoothstep (s1);
        const double u2 = invertSmoothstep (s2);
        const double level1 = thresholdDb + u1 * kneeWidthDb;
        const double level2 = thresholdDb + u2 * kneeWidthDb;

        const double driftDb = std::abs (level1 - level2);
        expect (driftDb < 0.01,
                "Detector level drift " + juce::String (driftDb, 8)
                + " dB between 1 s (" + juce::String (level1, 6)
                + ") and 2 s (" + juce::String (level2, 6) + ") not < 0.01 dB");
    }
};

// Static instance registers the test with juce::UnitTestRunner.
static RepeatChainTests sRepeatChainTestsInstance;

} // namespace
