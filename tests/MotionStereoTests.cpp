/*
  ==============================================================================
  Motion (modulation) and stereo tests for ShredSpace. Uses ONLY
  juce::UnitTest; no third-party test frameworks (Catch2, GoogleTest, etc.)
  are involved.

  Coverage:
    1. Modulation LFO rate honored: the produced period matches Fs/rate, and
       the right-channel phase equals the left-channel phase plus the
       configured offset.
    2. Modulation continuity and bounds: successive modulated delay positions
       change continuously and every position stays within
       [8 samples, 8.25 seconds].
    3. Rack pitch cap: at a fast rate the effective peak rate deviation
       2*pi*rate*depthEffective does not exceed 2^(25/1200) - 1; at a slow
       rate the full requested depth is honored (depthEffective == depthUser).
    4. Saturator: transparent at amount 0 (exact); unity small-signal slope
       near x = 0 for several amounts; finite output for large inputs.
    5. Diffuser: bounded output for bounded input; near-unity magnitude
       (input/output RMS) for a broadband signal; an amount above 0.40 is
       clamped to 0.40 (output numerically identical to an explicit 0.40 run).
    6. Stereo: mid/side width at w = 1 is an exact round-trip; low-frequency
       mono collapses a below-crossover out-of-phase tone toward mono while an
       above-crossover tone retains its stereo difference.

  Implementation note: ModulationEngine.cpp and SoftSaturator.cpp are compiled
  directly into this translation unit (mirroring the convention used by the
  other DSP test files). ShortDiffuser.cpp is linked via the test-runner
  target instead: its anonymous-namespace `defaultAmount` collides with the
  same-named definition in SoftSaturator.cpp when both are placed in a single
  translation unit, so ShortDiffuser.cpp is kept as its own translation unit.
  StereoProcessor.cpp is compiled into the DspCoreTests translation unit of
  this same executable; only its header is included here.
  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <utility>
#include <vector>

#include "../Source/DSP/ModulationEngine.h"
#include "../Source/DSP/SoftSaturator.h"
#include "../Source/DSP/ShortDiffuser.h"
#include "../Source/DSP/StereoProcessor.h"

// Compile the DSP implementations this translation unit exercises directly.
#include "../Source/DSP/ModulationEngine.cpp"
#include "../Source/DSP/SoftSaturator.cpp"

namespace
{
constexpr double kPi = 3.141592653589793238462643383279502884;

template <typename T>
double rmsRange (const std::vector<T>& v, int lo, int hi)
{
    double acc = 0.0;
    for (int i = lo; i < hi; ++i)
        acc += static_cast<double> (v[i]) * static_cast<double> (v[i]);
    return std::sqrt (acc / static_cast<double> (hi - lo));
}

// Indices where the signal crosses zero going upward.
std::vector<int> risingZeroCrossings (const std::vector<double>& sig)
{
    std::vector<int> idx;
    for (int i = 1; i < static_cast<int> (sig.size()); ++i)
        if (sig[i - 1] <= 0.0 && sig[i] > 0.0)
            idx.push_back (i);
    return idx;
}

// Advances the SoftSaturator amount smoother to its target so that
// processSample() (which reads the settled value) operates at the requested
// amount. The block process() path calls getNextValue() during smoothing.
void settleSaturator (shredspace::SoftSaturator& sat, float amount, double Fs)
{
    sat.setAmount (amount);
    const int settle = static_cast<int> (0.2 * Fs); // 200 ms >> 30 ms smoothing
    std::vector<float> buf (settle, 0.0f);
    sat.process (buf.data(), settle);
}

class MotionStereoTests final : public juce::UnitTest
{
public:
    MotionStereoTests()
        : juce::UnitTest ("Motion & Stereo", "ShredSpace DSP")
    {
    }

    void runTest() override
    {
        runModulationRateAndPhaseTest();
        runModulationContinuityAndBoundsTest();
        runRackPitchCapTest();
        runSaturatorTest();
        runDiffuserTest();
        runStereoTest();
    }

private:
    // -----------------------------------------------------------------------
    // 1. LFO rate and right-channel phase offset.
    //    Drift disabled (deterministic pure LFO). After the rate/depth smoothers
    //    settle, the left-channel offset is depthEff*sin(2*pi*rate*n/Fs)*Fs, so
    //    its period is Fs/rate and consecutive rising zero crossings are
    //    exactly that far apart. With a 90 degree phase offset, at a left
    //    rising zero crossing (phase ~= 0) the right channel reads
    //    depthEff*sin(pi/2)*Fs = depthEff*Fs.
    // -----------------------------------------------------------------------
    void runModulationRateAndPhaseTest()
    {
        beginTest ("Modulation LFO rate and right-channel phase offset");

        const double Fs = 48000.0;
        const float rateHz = 1.0f;
        const float depthMs = 1.0f;        // 1 ms -> 0.001 s (below pitch cap at 1 Hz)
        const double depthSeconds = 0.001;
        const float phaseOffsetDeg = 90.0f;

        shredspace::ModulationEngine engine;
        engine.prepare (Fs, 256);
        engine.setDriftAmount (0.0f);
        engine.setRate (rateHz);
        engine.setDepth (depthMs);
        engine.setPhaseOffset (phaseOffsetDeg);

        const int settle = 6000; // settles rate (50 ms) and depth (30 ms) smoothers
        for (int n = 0; n < settle; ++n)
            (void) engine.processSample();

        const int collect = 150000; // ~3.1 periods at 1 Hz
        std::vector<double> offsetL (collect), offsetR (collect);
        for (int n = 0; n < collect; ++n)
        {
            const auto o = engine.processSample();
            offsetL[n] = o[0];
            offsetR[n] = o[1];
        }

        // 1a. Produced period == Fs / rate.
        const auto crossings = risingZeroCrossings (offsetL);
        expect (crossings.size() >= 2,
                "Need >= 2 rising zero crossings, found " + juce::String (crossings.size()));
        if (crossings.size() >= 2)
        {
            const double expectedPeriod = Fs / static_cast<double> (rateHz);
            for (size_t i = 1; i < crossings.size(); ++i)
            {
                const double period = static_cast<double> (crossings[i] - crossings[i - 1]);
                expect (std::abs (period - expectedPeriod) <= 2.0,
                        "Period " + juce::String (period, 4) + " not within 2 samples of "
                        + juce::String (expectedPeriod, 4));
            }
        }

        // 1b. Right-channel phase == left + offset.
        const double expectedRightAtLeftZero = depthSeconds * Fs; // depthEff == depthUser here
        int checked = 0;
        for (int idx : crossings)
        {
            expect (std::abs (offsetR[idx] - expectedRightAtLeftZero) <= 0.1,
                    "Right offset at left rising zero " + juce::String (idx) + ": "
                    + juce::String (offsetR[idx], 6) + " not within 0.1 of "
                    + juce::String (expectedRightAtLeftZero, 6));
            if (++checked >= 3)
                break;
        }
    }

    // -----------------------------------------------------------------------
    // 2. Continuity and bounds of modulated delay positions.
    //    Three base delays exercise the min clamp (4 samples), the max clamp
    //    (8.25 s), and a nominal interior value. Every position must lie in
    //    [8, 8.25 s] and successive positions must move by < 1 sample.
    // -----------------------------------------------------------------------
    void runModulationContinuityAndBoundsTest()
    {
        beginTest ("Modulated delay positions: continuity and [8, 8.25 s] bounds");

        const double Fs = 48000.0;
        const double maxDelaySamples =
            shredspace::ModulationEngine::kMaximumDelaySeconds * Fs;
        const float rateHz = 2.0f;
        const float depthMs = 1.0f; // depthEff == depthUser at 2 Hz (below pitch cap)

        const std::vector<double> baseDelays = { 4.0, maxDelaySamples, 24000.0 };

        for (double baseDelay : baseDelays)
        {
            shredspace::ModulationEngine engine;
            engine.prepare (Fs, 256);
            engine.setDriftAmount (0.0f);
            engine.setRate (rateHz);
            engine.setDepth (depthMs);
            engine.setPhaseOffset (45.0f); // exercise both channels distinctly

            const int N = 50000;
            double prevPosL = 0.0, prevPosR = 0.0;
            bool first = true;

            for (int n = 0; n < N; ++n)
            {
                const auto o = engine.processSample();
                const double posL = engine.applyToBaseDelay (baseDelay, o[0]);
                const double posR = engine.applyToBaseDelay (baseDelay, o[1]);

                expect (posL >= shredspace::ModulationEngine::kMinimumDelaySamples - 1.0e-9,
                        "L position " + juce::String (posL, 6) + " below 8 at sample "
                        + juce::String (n));
                expect (posL <= maxDelaySamples + 1.0e-9,
                        "L position " + juce::String (posL, 6) + " above max at sample "
                        + juce::String (n));
                expect (posR >= shredspace::ModulationEngine::kMinimumDelaySamples - 1.0e-9,
                        "R position " + juce::String (posR, 6) + " below 8 at sample "
                        + juce::String (n));
                expect (posR <= maxDelaySamples + 1.0e-9,
                        "R position " + juce::String (posR, 6) + " above max at sample "
                        + juce::String (n));

                if (! first)
                {
                    const double dL = std::abs (posL - prevPosL);
                    const double dR = std::abs (posR - prevPosR);
                    expect (dL <= 1.0,
                            "L discontinuity " + juce::String (dL, 6) + " > 1.0 at sample "
                            + juce::String (n) + " (base " + juce::String (baseDelay, 1) + ")");
                    expect (dR <= 1.0,
                            "R discontinuity " + juce::String (dR, 6) + " > 1.0 at sample "
                            + juce::String (n) + " (base " + juce::String (baseDelay, 1) + ")");
                }
                prevPosL = posL;
                prevPosR = posR;
                first = false;
            }
        }
    }

    // -----------------------------------------------------------------------
    // 3. Rack pitch cap.
    //    The effective depth is observed as the peak |offset|/Fs (drift off,
    //    so offset amplitude == depthEff*Fs). At 5 Hz with 8 ms requested the
    //    depth is capped so 2*pi*rate*depthEff <= 2^(25/1200) - 1. At 0.1 Hz
    //    the cap is inactive and depthEff == depthUser (0.008 s).
    // -----------------------------------------------------------------------
    void runRackPitchCapTest()
    {
        beginTest ("Rack pitch cap: fast rate clamped, slow rate honors full depth");

        const double Fs = 48000.0;
        const double maxRateDeviation = std::pow (2.0, 25.0 / 1200.0) - 1.0;
        const float maxDepthMs = static_cast<float> (
            shredspace::ModulationEngine::kMaximumDepthSeconds * 1000.0); // 8 ms

        // Fast rate: peak rate deviation must not exceed the cap.
        {
            const float rateHz = 5.0f;
            shredspace::ModulationEngine engine;
            engine.prepare (Fs, 256);
            engine.setDriftAmount (0.0f);
            engine.setRate (rateHz);
            engine.setDepth (maxDepthMs);
            engine.setPhaseOffset (0.0f);

            const int settle = 10000;
            for (int n = 0; n < settle; ++n)
                (void) engine.processSample();

            const int collect = 20000; // ~2.08 periods at 5 Hz
            double peak = 0.0;
            for (int n = 0; n < collect; ++n)
            {
                const auto o = engine.processSample();
                peak = std::max (peak, std::abs (o[0]));
            }

            const double depthEffective = peak / Fs;
            const double peakRateDeviation =
                2.0 * kPi * static_cast<double> (rateHz) * depthEffective;

            expect (peakRateDeviation <= maxRateDeviation * (1.0 + 1.0e-4),
                    "Fast-rate peak rate deviation " + juce::String (peakRateDeviation, 8)
                    + " exceeds cap " + juce::String (maxRateDeviation, 8));

            const double depthUser = static_cast<double> (maxDepthMs) * 0.001; // 0.008
            expect (depthEffective < depthUser - 1.0e-9,
                    "Fast-rate depth " + juce::String (depthEffective, 8)
                    + " not reduced below requested " + juce::String (depthUser, 8));

            const double expectedDepth =
                maxRateDeviation / (2.0 * kPi * static_cast<double> (rateHz));
            expect (std::abs (depthEffective - expectedDepth) <= 1.0e-6,
                    "Fast-rate depth " + juce::String (depthEffective, 8)
                    + " not equal to clamped " + juce::String (expectedDepth, 8));
        }

        // Slow rate: full requested depth honored.
        {
            const float rateHz = 0.1f;
            shredspace::ModulationEngine engine;
            engine.prepare (Fs, 256);
            engine.setDriftAmount (0.0f);
            engine.setRate (rateHz);
            engine.setDepth (maxDepthMs);
            engine.setPhaseOffset (0.0f);

            const int settle = 10000;
            for (int n = 0; n < settle; ++n)
                (void) engine.processSample();

            const int collect = 500000; // ~1.04 periods at 0.1 Hz -> captures the sine peak
            double peak = 0.0;
            for (int n = 0; n < collect; ++n)
            {
                const auto o = engine.processSample();
                peak = std::max (peak, std::abs (o[0]));
            }

            const double depthEffective = peak / Fs;
            const double depthUser = static_cast<double> (maxDepthMs) * 0.001; // 0.008

            expect (std::abs (depthEffective - depthUser) <= 1.0e-5,
                    "Slow-rate depth " + juce::String (depthEffective, 8)
                    + " not equal to requested " + juce::String (depthUser, 8));
        }
    }

    // -----------------------------------------------------------------------
    // 4. SoftSaturator.
    //    4a. At amount 0 the stage is exactly transparent (y == x).
    //    4b. The small-signal slope near x = 0 is unity for several amounts.
    //    4c. Output is finite for large inputs.
    // -----------------------------------------------------------------------
    void runSaturatorTest()
    {
        beginTest ("SoftSaturator: transparent at 0, unity slope, finite for large input");

        const double Fs = 48000.0;

        // 4a. Transparent at amount 0 (exact equality).
        {
            shredspace::SoftSaturator sat;
            sat.prepare (Fs, 256);
            settleSaturator (sat, 0.0f, Fs);

            const float inputs[] = { 0.0f, 0.5f, -0.5f, 0.99f, -0.99f, 1.0e-5f, -1.0e-5f };
            for (float x : inputs)
            {
                const float y = sat.processSample (x);
                expect (y == x,
                        "Transparent: y=" + juce::String (y, 8) + " != x="
                        + juce::String (x, 8));
            }
        }

        // 4b. Unity small-signal slope near x = 0 for several amounts.
        {
            const float amounts[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
            const float eps = 1.0e-4f;
            for (float s : amounts)
            {
                shredspace::SoftSaturator sat;
                sat.prepare (Fs, 256);
                settleSaturator (sat, s, Fs);

                const float yP = sat.processSample (eps);
                const float yM = sat.processSample (-eps);
                const double slope =
                    (static_cast<double> (yP) - static_cast<double> (yM))
                    / (2.0 * static_cast<double> (eps));
                expect (std::abs (slope - 1.0) <= 1.0e-4,
                        "Slope at amount " + juce::String (s, 3) + ": "
                        + juce::String (slope, 8) + " not within 1e-4 of unity");
            }
        }

        // 4c. Finite output for large inputs.
        {
            shredspace::SoftSaturator sat;
            sat.prepare (Fs, 256);
            settleSaturator (sat, 1.0f, Fs);

            const float largeInputs[] = { 1.0e6f, -1.0e6f, 1.0e12f, 1.0e30f, 1.0e38f, -1.0e38f };
            for (float x : largeInputs)
            {
                const float y = sat.processSample (x);
                expect (std::isfinite (y),
                        "Non-finite output " + juce::String (y) + " for input "
                        + juce::String (x));
            }
        }
    }

    // -----------------------------------------------------------------------
    // 5. ShortDiffuser.
    //    5a. Bounded output for bounded input.
    //    5b. Near-unity magnitude (input/output RMS within tolerance) for a
    //        deterministic broadband signal.
    //    5c. An amount above 0.40 is clamped to 0.40: output is numerically
    //        identical to an explicit amount-0.40 run.
    // -----------------------------------------------------------------------
    void runDiffuserTest()
    {
        beginTest ("ShortDiffuser: bounded, near-unity magnitude, amount clamp at 0.40");

        const double Fs = 48000.0;
        const int preRoll = 4096;  // fills longest stage (~7.9 ms ~= 379 samples)
        const int window = 16384;

        // Deterministic broadband test signal, uniform in [-1, 1].
        std::mt19937 rng (20240724u);
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
        std::vector<float> input (preRoll + window);
        for (auto& v : input)
            v = dist (rng);

        // 5a + 5b. Bounded output and near-unity RMS at amount 0.40.
        {
            shredspace::ShortDiffuser diff;
            diff.prepare (Fs, 256);
            diff.reset();
            diff.setAmount (0.40f);

            std::vector<float> buf = input;
            diff.process (buf.data(), 0, static_cast<int> (buf.size()));

            double maxAbs = 0.0;
            for (float v : buf)
            {
                maxAbs = std::max (maxAbs, std::abs (static_cast<double> (v)));
                expect (std::isfinite (v), "Non-finite diffuser output");
            }
            expect (maxAbs <= 6.0,
                    "Diffuser output exceeds 6.0 for bounded input: "
                    + juce::String (maxAbs, 6));

            const double inRms = rmsRange (input, preRoll, preRoll + window);
            const double outRms = rmsRange (buf, preRoll, preRoll + window);
            expect (inRms > 0.0, "Input RMS is zero; signal malformed");
            expect (std::abs (outRms - inRms) <= 0.06 * inRms,
                    "Diffuser RMS " + juce::String (outRms, 6) + " vs input "
                    + juce::String (inRms, 6) + " not within 6% (near-unity magnitude)");
        }

        // 5c. Amount above 0.40 clamps to 0.40: identical output sample-for-sample.
        {
            std::vector<float> in0 = input;
            std::vector<float> in1 = input;

            shredspace::ShortDiffuser d0;
            d0.prepare (Fs, 256);
            d0.reset();
            d0.setAmount (0.40f);

            shredspace::ShortDiffuser d1;
            d1.prepare (Fs, 256);
            d1.reset();
            d1.setAmount (1.0f); // above 0.40 -> clamped to 0.40 on entry

            d0.process (in0.data(), 0, static_cast<int> (in0.size()));
            d1.process (in1.data(), 0, static_cast<int> (in1.size()));

            for (size_t i = 0; i < in0.size(); ++i)
            {
                expect (in0[i] == in1[i],
                        "Amount clamp mismatch at " + juce::String (static_cast<int> (i))
                        + ": " + juce::String (in0[i], 8) + " != " + juce::String (in1[i], 8));
            }
        }
    }

    // -----------------------------------------------------------------------
    // 6. StereoProcessor.
    //    6a. Mid/side width at w = 1 reproduces L/R. Low-frequency mono is
    //        disabled and pan = 0 (center). The constant-power pan center
    //        gain is 1/sqrt(2); the input is pre-scaled by sqrt(2) so that
    //        gain cancels and the processed output reproduces the original
    //        L/R sample-for-sample within floating-point tolerance.
    //    6b. Low-frequency mono: a below-crossover out-of-phase tone collapses
    //        toward mono; an above-crossover tone retains its stereo difference.
    // -----------------------------------------------------------------------
    void runStereoTest()
    {
        beginTest ("StereoProcessor: width round-trip at w=1, low-frequency mono");

        const double Fs = 48000.0;
        const int N = 8192;

        // 6a. Width round-trip at w = 1.
        {
            shredspace::StereoProcessor stereo;
            stereo.prepare (Fs, 256);
            stereo.reset();
            stereo.setWidth (1.0f);
            stereo.setPan (0.0f);
            stereo.setLowFrequencyMonoEnabled (false);

            const float sqrt2 = std::sqrt (2.0f);
            std::vector<float> L (N), R (N), Lproc (N), Rproc (N);
            for (int n = 0; n < N; ++n)
            {
                const double t = static_cast<double> (n) / Fs;
                L[n] = static_cast<float> (0.5 * std::sin (2.0 * kPi * 440.0 * t));
                R[n] = static_cast<float> (0.3 * std::sin (2.0 * kPi * 660.0 * t + 0.7));
                Lproc[n] = sqrt2 * L[n];
                Rproc[n] = sqrt2 * R[n];
            }
            stereo.process (Lproc.data(), Rproc.data(), N);

            for (int n = 0; n < N; ++n)
            {
                expect (std::abs (Lproc[n] - L[n]) <= 1.0e-5f,
                        "Width round-trip L mismatch at " + juce::String (n) + ": "
                        + juce::String (Lproc[n], 8) + " vs " + juce::String (L[n], 8));
                expect (std::abs (Rproc[n] - R[n]) <= 1.0e-5f,
                        "Width round-trip R mismatch at " + juce::String (n) + ": "
                        + juce::String (Rproc[n], 8) + " vs " + juce::String (R[n], 8));
            }
        }

        // 6b. Low-frequency mono below and above the crossover.
        {
            const float crossover = 200.0f;
            const int preRoll = 6000;
            const int winLen = 24000;
            const float panGain = 1.0f / std::sqrt (2.0f);

            auto runTone = [&] (double freq)
            {
                shredspace::StereoProcessor stereo;
                stereo.prepare (Fs, 256);
                stereo.reset();
                stereo.setWidth (1.0f);
                stereo.setPan (0.0f);
                stereo.setLowFrequencyMonoEnabled (true);
                stereo.setLowFrequencyMonoCrossover (crossover);

                const int total = preRoll + winLen;
                std::vector<float> Lin (total), Rin (total);
                for (int n = 0; n < total; ++n)
                {
                    const double t = static_cast<double> (n) / Fs;
                    const double a = 0.5 * std::sin (2.0 * kPi * freq * t);
                    Lin[n] = static_cast<float> (a);
                    Rin[n] = static_cast<float> (-a); // out of phase
                }

                std::vector<float> Lout = Lin, Rout = Rin;
                stereo.process (Lout.data(), Rout.data(), total);

                double inDiffRms = 0.0, outDiffRms = 0.0;
                for (int i = 0; i < winLen; ++i)
                {
                    const int idx = preRoll + i;
                    const double id = static_cast<double> (Lin[idx]) - static_cast<double> (Rin[idx]);
                    const double od = static_cast<double> (Lout[idx]) - static_cast<double> (Rout[idx]);
                    inDiffRms += id * id;
                    outDiffRms += od * od;
                }
                inDiffRms = std::sqrt (inDiffRms / static_cast<double> (winLen));
                outDiffRms = std::sqrt (outDiffRms / static_cast<double> (winLen));
                return std::make_pair (inDiffRms, outDiffRms);
            };

            // Below crossover: out-of-phase tone collapses toward mono.
            {
                const auto pr = runTone (80.0);
                expect (pr.first > 0.0, "Below-crossover input diff RMS is zero");
                expect (pr.second < 0.12 * pr.first,
                        "Below-crossover output diff " + juce::String (pr.second, 6)
                        + " not < 12% of input " + juce::String (pr.first, 6)
                        + " (not collapsed toward mono)");
            }

            // Above crossover: stereo difference retained (~ pan center gain).
            {
                const auto pr = runTone (2000.0);
                expect (pr.first > 0.0, "Above-crossover input diff RMS is zero");
                expect (pr.second > 0.5 * pr.first,
                        "Above-crossover output diff " + juce::String (pr.second, 6)
                        + " not > 50% of input " + juce::String (pr.first, 6)
                        + " (stereo difference not retained)");
                expect (std::abs (pr.second - panGain * pr.first) <= 0.05 * pr.first,
                        "Above-crossover output diff " + juce::String (pr.second, 6)
                        + " not within 5% of pan-gain-scaled input "
                        + juce::String (panGain * pr.first, 6));
            }
        }
    }
};

// Static instance registers the test with juce::UnitTestRunner.
static MotionStereoTests sMotionStereoTestsInstance;

} // namespace
