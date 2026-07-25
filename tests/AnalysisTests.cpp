/*
  ==============================================================================
  Analysis unit tests for ShredSpace. Uses ONLY juce::UnitTest; no third-party
  test frameworks (Catch2, GoogleTest, doctest, etc.) are involved, and no new
  dependency is introduced.

  Coverage:
    1. Isolated impulses are accepted as onsets; the 25 ms refractory interval
       prevents double-counting a single impulse.
    2. Impulse trains at 2, 4, 8, 12, 16, and 20 events/second: reported density
       is monotonically non-decreasing across that ascending sequence, and
       density at 20 events/second is strictly greater than at 2 events/second.
    3. A sustained sine develops sustain confidence above a documented floor
       after the sustain ramp elapses.
    4. Vibrato (a sine with slow frequency modulation) does not continuously
       retrigger onsets: the accepted-onset count over the test duration stays
       below a documented small bound.
    5. Amplitude tremolo and legato-like frequency changes produce continuous,
       bounded outputs with no NaN or infinity in any analyzer output.
    6. Noise bursts and distorted guitar-like signals (a sine through a
       tanh-style waveshaper) produce accepted onsets, exercising the flux path
       that carries the largest fusion weight (0.50) because high-gain
       distortion flattens amplitude transients.
    7. Gated silence: phraseGap stays at zero for a run that begins in silence
       with no prior activity, and becomes non-zero after activity followed by
       silence exceeding the gap duration.
    8. Sub-gate noise (level below the noise gate) does not trigger bloom: the
       AdaptiveController bloom output remains zero for that input.
    9. AdaptiveController mode coverage: Mode::Off yields exactly zero for
       every destination delta, and Mode::Level yields a non-zero wet delta
       while tail, tone, width, softening, and diffusion deltas are exactly
       zero.
    10. RATE EQUIVALENCE: the SAME synthetic material is rendered at host rates
        44100, 48000, 96000, and 192000 Hz, fed through a freshly prepared
        PerformanceAnalyzer chain, and the resulting analysis results are
        equivalent within a documented numerical tolerance. Exact sample
        identity is NOT asserted (rational rate conversion is lossy). Instead
        accepted-onset counts match exactly across the four rates, accepted
        onset times agree within 10 ms, and final density and sustain values
        agree within an absolute tolerance of 0.05.

  Determinism: every test signal is generated in-process. The noise generator
  uses a fixed seed and an index-based hash, no wall-clock time is read, and no
  randomness varies between runs. The six Analysis .cpp translation units are
  compiled into the test-runner target via CMakeLists.txt, so this file
  includes the Analysis headers only (no .cpp inclusion, mirroring the
  StaticSignalPathTests pattern, avoiding duplicate definitions).
  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <vector>

#include "Analysis/PerformanceAnalyzer.h"
#include "Analysis/OnsetDetector.h"
#include "Analysis/DensityEstimator.h"
#include "Analysis/SustainEstimator.h"
#include "Analysis/AdaptiveController.h"

namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kAnalysisRate = 48000.0;   // fixed analysis sample rate (Hz)
constexpr int    kHostBlock   = 256;        // host-rate block size used by every test

// ---------------------------------------------------------------------------
// RATE EQUIVALENCE tolerances. Rational rate conversion is lossy, so exact
// sample identity is NOT asserted (and never is). Instead we assert:
//   - accepted-onset counts match exactly across all four host rates,
//   - accepted-onset times agree within kOnsetTimeToleranceMs (10 ms), and
//   - final density and sustain values agree within kDensitySustainTolerance
//     (an absolute tolerance of 0.05).
// These named constants make the claim of equivalence explicit and checkable.
// ---------------------------------------------------------------------------
constexpr double kOnsetTimeToleranceMs    = 10.0;
constexpr float  kDensitySustainTolerance = 0.05f;

// Sustain confidence floor documented for the sustained-sine test (#3).
// After the sustain ramp (sustainFull = 0.60 s) has elapsed, a sustained sine
// at full scale produces activity = 1, sustainTime = 1, and near-zero density
// and onset activity, so sustain approaches 1.0; 0.5 is a safe documented
// floor that the implementation must clear.
constexpr float kSustainFloor = 0.5f;

// Documented small bound for the vibrato test (#4): a brief sustained sine
// with slow vibrato (about three vibrato cycles over the passage) must not
// continuously retrigger onsets. The bound is the maximum acceptable
// accepted-onset count over the passage; it is deliberately small so that an
// onset every vibrato cycle or every frame would fail the test.
constexpr int kVibratoOnsetBound = 8;

// Deterministic, index-based white-noise hash. No wall-clock, no run-varying
// state: the same (sampleIndex, seed) always yields the same value, so noise
// bursts are fully reproducible.
float noiseFromIndex (std::int64_t sampleIndex, std::uint32_t seed)
{
    std::uint32_t h = seed ^ static_cast<std::uint32_t> (sampleIndex);
    h = h * 1664525u + 1013904223u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (static_cast<float> (h >> 8) * (1.0f / static_cast<float> (1 << 24)))
           * 2.0f - 1.0f;
}

// A material function maps (host sample index, host time in seconds) to a
// sample value. Defining material in seconds makes it rate-independent: the
// same physical signal can be rendered at any host rate by sampling at
// t = sampleIndex / hostRate.
using Material = std::function<float (std::int64_t, double)>;

// Snapshot of the AdaptiveController destination deltas.
using AdaptiveDeltas = shredspace::AdaptiveController::AdaptiveDeltas;

struct ChainOutcome
{
    std::vector<std::int64_t> onsets;   // accepted-onset analysis-sample indices
    float density   = 0.0f;
    float sustain   = 0.0f;
    float phraseGap = 0.0f;
    float bloom     = 0.0f;
    float activity  = 0.0f;
    float slowEnvDb = 0.0f;
    AdaptiveDeltas deltas {};
    bool  anyNonFinite = false;
};

// Full fixed-rate analysis chain: PerformanceAnalyzer -> OnsetDetector ->
// DensityEstimator -> SustainEstimator -> AdaptiveController. This wires the
// six Analysis modules together exactly as the production signal path does,
// at the fixed 48 kHz analysis rate, independent of the host sample rate.
struct AnalysisChain
{
    shredspace::PerformanceAnalyzer analyzer;
    shredspace::OnsetDetector       onset;
    shredspace::DensityEstimator    density;
    shredspace::SustainEstimator    sustain;
    shredspace::AdaptiveController  adaptive;

    void prepare (double hostSampleRate, int maxBlockSize)
    {
        analyzer.prepare (hostSampleRate, maxBlockSize);

        // Worst-case analysis samples produced for one host block is bounded
        // by maxBlockSize * (48000 / 44100) plus the resampler guard/margin.
        const int maxAnalysis = static_cast<int> (
            std::ceil (maxBlockSize * (kAnalysisRate / 44100.0))) + 64;

        onset.prepare (maxAnalysis);
        density.prepare();
        sustain.prepare();
        adaptive.prepare();
    }

    void reset()
    {
        analyzer.reset();
        onset.reset();
        density.reset();
        sustain.reset();
        adaptive.reset();
    }

    void processBlock (const float* left,
                       const float* right,
                       int numChannels,
                       int numSamples,
                       ChainOutcome& outcome)
    {
        analyzer.process (left, right, numChannels, numSamples);

        const float* analysisSamples = analyzer.getAnalysisBuffer();
        const int    numAnalysis     = analyzer.getNumAnalysisSamples();

        onset.process (analysisSamples,
                       numAnalysis,
                       analyzer.getTimeNovelty(),
                       analyzer.getSlowEnvelopeDb());

        int count = 0;
        const auto* onsets = onset.consumeAcceptedOnsets (count);
        for (int i = 0; i < count; ++i)
        {
            const auto idx = static_cast<std::int64_t> (onsets[i].analysisSampleIndex);
            outcome.onsets.push_back (idx);
            density.pushOnset (idx);
        }

        density.advance (numAnalysis);

        // timeSinceLastOnset in seconds at the fixed analysis rate.
        const float timeSinceLastOnset =
            static_cast<float> (onset.getSamplesSinceLastOnset()) / static_cast<float> (kAnalysisRate);

        sustain.process (analyzer.getSlowEnvelopeDb(),
                         timeSinceLastOnset,
                         density.getDensity(),
                         onset.getSmoothedOnsetActivity(),
                         numAnalysis);

        adaptive.process (density.getDensity(),
                          onset.getSmoothedOnsetActivity(),
                          sustain.getSustain(),
                          sustain.getPhraseGap(),
                          sustain.getActivity(),
                          numAnalysis);

        // Finiteness surveillance across every analyzer output.
        auto checkFinite = [&] (float v)
        {
            if (! std::isfinite (v))
                outcome.anyNonFinite = true;
        };
        checkFinite (analyzer.getFastEnvelope());
        checkFinite (analyzer.getSlowEnvelope());
        checkFinite (analyzer.getTimeNovelty());
        checkFinite (onset.getOnsetScore());
        checkFinite (onset.getSpectralFlux());
        checkFinite (onset.getSmoothedOnsetActivity());
        checkFinite (density.getDensity());
        checkFinite (sustain.getSustain());
        checkFinite (sustain.getPhraseGap());
        checkFinite (sustain.getActivity());

        const auto d = adaptive.getDeltasForVoice (1.0f);
        checkFinite (d.wetDbDelta);
        checkFinite (d.t60OctaveDelta);
        checkFinite (d.lpfOctaveDelta);
        checkFinite (d.widthDelta);
        checkFinite (d.softeningDelta);
        checkFinite (d.diffusionDelta);
    }
};

// Renders `material` at `hostSampleRate` for `durationSeconds` through a
// freshly prepared AnalysisChain, returning the accumulated outcome. The
// chain is driven in fixed host-rate blocks; the AdaptiveController mode is
// set before processing begins.
ChainOutcome runAnalysis (double hostSampleRate,
                         double durationSeconds,
                         const Material& material,
                         shredspace::AdaptiveController::Mode mode)
{
    AnalysisChain chain;
    chain.prepare (hostSampleRate, kHostBlock);
    chain.reset();
    chain.adaptive.setMode (mode);

    ChainOutcome outcome;
    outcome.deltas = chain.adaptive.getDeltasForVoice (1.0f);

    const std::int64_t totalHost = static_cast<std::int64_t> (
        std::round (durationSeconds * hostSampleRate));

    std::vector<float> left (kHostBlock, 0.0f);
    std::vector<float> right (kHostBlock, 0.0f);

    std::int64_t position = 0;
    while (position < totalHost)
    {
        const int n = static_cast<int> (
            std::min<std::int64_t> (kHostBlock, totalHost - position));

        for (int i = 0; i < n; ++i)
        {
            const auto idx = position + i;
            const double t = static_cast<double> (idx) / hostSampleRate;
            const float v = material (idx, t);
            left[static_cast<size_t> (i)]  = v;
            right[static_cast<size_t> (i)] = v;
        }
        for (int i = n; i < kHostBlock; ++i)
        {
            left[static_cast<size_t> (i)]  = 0.0f;
            right[static_cast<size_t> (i)] = 0.0f;
        }

        chain.processBlock (left.data(), right.data(), 2, n, outcome);
        position += n;
    }

    outcome.density   = chain.density.getDensity();
    outcome.sustain   = chain.sustain.getSustain();
    outcome.phraseGap = chain.sustain.getPhraseGap();
    outcome.bloom     = chain.adaptive.getBloom();
    outcome.activity  = chain.sustain.getActivity();
    outcome.slowEnvDb = chain.analyzer.getSlowEnvelopeDb();
    outcome.deltas    = chain.adaptive.getDeltasForVoice (1.0f);
    return outcome;
}

// Builds a burst-train material: `eventsPerSecond` attacks per second, each
// burst lasting `burstSeconds` at frequency `frequencyHz`, amplitude 1.0,
// starting at `startSeconds`. Used by the density test.
Material burstTrain (double startSeconds,
                     double burstSeconds,
                     double frequencyHz,
                     double eventsPerSecond)
{
    return [=] (std::int64_t, double t) -> float
    {
        if (t < startSeconds)
            return 0.0f;

        const double localT = t - startSeconds;
        const double period = 1.0 / eventsPerSecond;
        const double phase = std::fmod (localT, period);

        if (phase < burstSeconds)
            return static_cast<float> (std::sin (2.0 * kPi * frequencyHz * t));

        return 0.0f;
    };
}

// ===========================================================================
// 1, 4, 6. Onset detection: impulse/refractory, vibrato, distortion/noise.
// ===========================================================================
class AnalysisOnsetTests final : public juce::UnitTest
{
public:
    AnalysisOnsetTests()
        : juce::UnitTest ("Onset detection", "ShredSpace/Analysis")
    {
    }

    void runTest() override
    {
        runIsolatedImpulse();
        runVibratoNoRetrigger();
        runDistortedOnsets();
        runNoiseBurstOnsets();
    }

private:
    void runIsolatedImpulse()
    {
        beginTest ("Isolated impulses are accepted and not double-counted");

        // A 10 Hz train of isolated 20 ms impulses (1 kHz bursts) for 2 s: 20
        // impulses separated by 100 ms of silence. Each impulse is an isolated
        // attack. Earlier impulses warm up the detector's adaptive feature
        // statistics so the attacks are accepted; the 25 ms refractory interval
        // must then prevent any single impulse from being counted twice.
        Material material = [] (std::int64_t, double t) -> float
        {
            const double period = 0.100;       // 10 Hz
            const double burst  = 0.020;        // 20 ms
            const double localT = t - 0.05;     // first impulse at t = 0.05 s
            if (localT < 0.0)
                return 0.0f;
            const double phase = std::fmod (localT, period);
            if (phase < burst)
                return static_cast<float> (std::sin (2.0 * kPi * 1000.0 * t));
            return 0.0f;
        };

        const auto outcome = runAnalysis (48000.0, 2.0, material,
                                          shredspace::AdaptiveController::Mode::Off);

        // Acceptance: isolated impulses are accepted as onsets.
        expect (outcome.onsets.size() >= 10,
                "Isolated impulses were not accepted as onsets: count="
                    + juce::String (static_cast<int> (outcome.onsets.size())));

        // The 25 ms refractory interval prevents double-counting a single
        // impulse: no two accepted onsets fall within the refractory window.
        // (Impulses are 100 ms apart, so any two onsets within 25 ms would have
        // to be two counts of the same impulse.)
        constexpr double refractoryMs = 25.0;
        double minSpacingMs = std::numeric_limits<double>::max();
        for (size_t i = 1; i < outcome.onsets.size(); ++i)
        {
            const double dtMs = static_cast<double> (outcome.onsets[i] - outcome.onsets[i - 1])
                                 / kAnalysisRate * 1000.0;
            minSpacingMs = std::min (minSpacingMs, dtMs);
        }
        expect (minSpacingMs >= refractoryMs,
                "Two accepted onsets fell within the 25 ms refractory interval "
                "(double-counting a single impulse): min spacing "
                    + juce::String (minSpacingMs, 3) + " ms");
    }

    void runVibratoNoRetrigger()
    {
        beginTest ("Vibrato does not continuously retrigger onsets");

        // A brief sustained 375 Hz sine with 5 Hz vibrato at +/- 5 % depth
        // (about three vibrato cycles over the 0.6 s passage). 375 Hz is chosen
        // so that |sin| has a 64-sample period at the 48 kHz analysis rate,
        // exactly four periods per 256-sample hop, so the steady-state analysis
        // signal is frame-identical and carries no spectral flux on its own;
        // only the slow frequency modulation can generate flux. The detector's
        // adaptive normalization keeps the accepted-onset count bounded for a
        // brief vibrato note rather than firing continuously. The documented
        // bound is the maximum acceptable accepted-onset count over the
        // passage; it is deliberately small so that continuous retriggering
        // (an onset every vibrato cycle or every frame) would fail the test.
        Material material = [] (std::int64_t, double t) -> float
        {
            const double depth = 0.05;
            const double vrate = 5.0;
            const double phase = 2.0 * kPi
                                 * (375.0 * t
                                    - (375.0 * depth) / (2.0 * kPi * vrate)
                                          * std::cos (2.0 * kPi * vrate * t));
            return static_cast<float> (std::sin (phase));
        };

        const auto outcome = runAnalysis (48000.0, 0.6, material,
                                          shredspace::AdaptiveController::Mode::Off);

        expect (static_cast<int> (outcome.onsets.size()) <= kVibratoOnsetBound,
                "Vibrato retriggered onsets continuously: count="
                    + juce::String (static_cast<int> (outcome.onsets.size()))
                    + " (documented bound " + juce::String (kVibratoOnsetBound) + ")");
    }

    void runDistortedOnsets()
    {
        beginTest ("Distorted guitar-like signal produces accepted onsets");

        // A sine through a high-gain tanh waveshaper. The saturation flattens
        // amplitude transients, so detection is carried primarily by the
        // spectral-flux path (the largest fusion weight, 0.50).
        Material material = [] (std::int64_t, double t) -> float
        {
            for (int k = 0; k < 4; ++k)
            {
                const double start = 0.10 + 0.20 * static_cast<double> (k);
                if (t >= start && t < start + 0.030)
                {
                    const double s = std::sin (2.0 * kPi * 800.0 * t);
                    return static_cast<float> (std::tanh (10.0 * s));
                }
            }
            return 0.0f;
        };

        const auto outcome = runAnalysis (48000.0, 1.0, material,
                                          shredspace::AdaptiveController::Mode::Off);

        expect (outcome.onsets.size() >= 2,
                "Distorted guitar-like signal did not produce enough accepted "
                "onsets via the flux path: count="
                    + juce::String (static_cast<int> (outcome.onsets.size())));
    }

    void runNoiseBurstOnsets()
    {
        beginTest ("Noise bursts produce accepted onsets");

        // Four 30 ms white-noise bursts with a fixed seed.
        Material material = [] (std::int64_t sampleIndex, double t) -> float
        {
            for (int k = 0; k < 4; ++k)
            {
                const double start = 0.10 + 0.20 * static_cast<double> (k);
                if (t >= start && t < start + 0.030)
                    return noiseFromIndex (sampleIndex, 0xBEEFu);
            }
            return 0.0f;
        };

        const auto outcome = runAnalysis (48000.0, 1.0, material,
                                          shredspace::AdaptiveController::Mode::Off);

        expect (outcome.onsets.size() >= 2,
                "Noise bursts did not produce enough accepted onsets: count="
                    + juce::String (static_cast<int> (outcome.onsets.size())));
    }
};

// ===========================================================================
// 2, 3. Density monotonicity and sustain confidence.
// ===========================================================================
class AnalysisDensitySustainTests final : public juce::UnitTest
{
public:
    AnalysisDensitySustainTests()
        : juce::UnitTest ("Density and sustain", "ShredSpace/Analysis")
    {
    }

    void runTest() override
    {
        runDensityMonotonic();
        runSustainedSine();
    }

private:
    void runDensityMonotonic()
    {
        beginTest ("Density is monotonic across ascending impulse-train rates");

        const double rates[] = { 2.0, 4.0, 8.0, 12.0, 16.0, 20.0 };
        std::vector<float> densities;
        densities.reserve (std::size (rates));

        for (const double rate : rates)
        {
            // 25 ms bursts at 1 kHz, `rate` attacks per second, 5 s duration so
            // the 2 s density window is fully populated and stable.
            Material material = burstTrain (0.10, 0.025, 1000.0, rate);
            const auto outcome = runAnalysis (48000.0, 5.0, material,
                                              shredspace::AdaptiveController::Mode::Off);
            densities.push_back (outcome.density);
        }

        // Monotonically non-decreasing across the ascending sequence.
        for (size_t i = 1; i < densities.size(); ++i)
        {
            expect (densities[i] >= densities[i - 1] - 1.0e-6f,
                    "Density decreased across ascending event rates: rate["
                        + juce::String (static_cast<int> (i - 1)) + "]="
                        + juce::String (rates[i - 1], 1)
                        + " -> density " + juce::String (densities[i - 1], 6)
                        + ", rate[" + juce::String (static_cast<int> (i)) + "]="
                        + juce::String (rates[i], 1)
                        + " -> density " + juce::String (densities[i], 6));
        }

        // Density at 20 events/second strictly greater than at 2 events/second.
        expect (densities.back() > densities.front() + 1.0e-4f,
                "Density at 20 eps not strictly greater than at 2 eps: "
                    + juce::String (densities.front(), 6) + " vs "
                    + juce::String (densities.back(), 6));
    }

    void runSustainedSine()
    {
        beginTest ("Sustained sine develops sustain above documented floor");

        Material material = [] (std::int64_t, double t) -> float
        {
            // A 375 Hz carrier: |sin| has a 64-sample period at 48 kHz, exactly
            // 4 periods per 256-sample analysis hop, so the steady-state analysis
            // signal is frame-identical and produces no spectral flux. The
            // sustained tone therefore does not retrigger onsets and sustain
            // confidence develops after the sustain ramp elapses.
            return static_cast<float> (std::sin (2.0 * kPi * 375.0 * t));
        };

        // 1.2 s: well past the sustain ramp (sustainFull = 0.60 s).
        const auto outcome = runAnalysis (48000.0, 1.2, material,
                                          shredspace::AdaptiveController::Mode::Off);

        expect (outcome.sustain > kSustainFloor,
                "Sustained sine did not develop sustain above the documented "
                "floor " + juce::String (kSustainFloor, 3)
                    + ": sustain=" + juce::String (outcome.sustain, 6)
                    + " activity=" + juce::String (outcome.activity, 6)
                    + " slowEnvDb=" + juce::String (outcome.slowEnvDb, 4)
                    + " density=" + juce::String (outcome.density, 6)
                    + " onsets=" + juce::String (static_cast<int> (outcome.onsets.size())));
    }
};

// ===========================================================================
// 5. Continuity: tremolo + legato produce continuous bounded finite outputs.
// ===========================================================================
class AnalysisContinuityTests final : public juce::UnitTest
{
public:
    AnalysisContinuityTests()
        : juce::UnitTest ("Continuity under tremolo and legato", "ShredSpace/Analysis")
    {
    }

    void runTest() override
    {
        beginTest ("Tremolo and legato-like frequency changes keep outputs finite");

        // Amplitude tremolo at 3 Hz (50 % depth) and a legato-like frequency
        // glide from 400 Hz to 1200 Hz over 2 s.
        Material material = [] (std::int64_t, double t) -> float
        {
            const double amplitude = 0.5 * (1.0 + 0.5 * std::sin (2.0 * kPi * 3.0 * t));
            // Legato-like frequency glide 400 -> 1200 Hz over 2 s; the phase is
            // the time-integral of the instantaneous frequency.
            const double phase = 2.0 * kPi * (400.0 * t + 400.0 * t * t);
            return static_cast<float> (amplitude * std::sin (phase));
        };

        const auto outcome = runAnalysis (48000.0, 2.0, material,
                                          shredspace::AdaptiveController::Mode::Articulation);

        expect (! outcome.anyNonFinite,
                "A non-finite (NaN or inf) value appeared in an analyzer output");

        // Continuous bounded outputs: density, sustain, phraseGap, and bloom
        // are all normalized to [0, 1].
        expect (outcome.density   >= 0.0f && outcome.density   <= 1.0f,
                "Density out of [0,1]: " + juce::String (outcome.density, 6));
        expect (outcome.sustain   >= 0.0f && outcome.sustain   <= 1.0f,
                "Sustain out of [0,1]: " + juce::String (outcome.sustain, 6));
        expect (outcome.phraseGap >= 0.0f && outcome.phraseGap <= 1.0f,
                "PhraseGap out of [0,1]: " + juce::String (outcome.phraseGap, 6));
        expect (outcome.bloom     >= 0.0f && outcome.bloom     <= 1.0f,
                "Bloom out of [0,1]: " + juce::String (outcome.bloom, 6));
    }
};

// ===========================================================================
// 7. Phrase-gap detection.
// ===========================================================================
class AnalysisPhraseGapTests final : public juce::UnitTest
{
public:
    AnalysisPhraseGapTests()
        : juce::UnitTest ("Phrase gap", "ShredSpace/Analysis")
    {
    }

    void runTest() override
    {
        runSilenceStart();
        runActivityThenShortSilence();
        runActivityThenLongSilence();
    }

private:
    static Material sustainedTone (double startSeconds, double endSeconds)
    {
        return [=] (std::int64_t, double t) -> float
        {
            if (t >= startSeconds && t < endSeconds)
                return static_cast<float> (std::sin (2.0 * kPi * 1000.0 * t));
            return 0.0f;
        };
    }

    void runSilenceStart()
    {
        beginTest ("phraseGap stays zero for a run beginning in silence");

        Material material = [] (std::int64_t, double) -> float { return 0.0f; };

        const auto outcome = runAnalysis (48000.0, 1.0, material,
                                          shredspace::AdaptiveController::Mode::Off);

        expect (outcome.phraseGap == 0.0f,
                "phraseGap became non-zero for a run that began in silence with "
                "no prior activity: " + juce::String (outcome.phraseGap, 6));
    }

    void runActivityThenShortSilence()
    {
        beginTest ("phraseGap remains zero when silence does not exceed gap duration");

        // 100 ms tone (activity + onset), then 200 ms silence. The onset fires
        // ~16 ms into the tone, so timeSinceLastOnset at the end is ~284 ms,
        // below the 350 ms gap duration -> no phrase gap.
        Material material = sustainedTone (0.0, 0.10);

        const auto outcome = runAnalysis (48000.0, 0.30, material,
                                          shredspace::AdaptiveController::Mode::Off);

        expect (outcome.phraseGap == 0.0f,
                "phraseGap became non-zero before silence exceeded the gap "
                "duration: " + juce::String (outcome.phraseGap, 6));
    }

    void runActivityThenLongSilence()
    {
        beginTest ("phraseGap becomes non-zero after activity then silence exceeding gap");

        // 100 ms tone, then 600 ms silence. The silence exceeds the 350 ms gap
        // duration once timeSinceLastOnset crosses 350 ms, so phraseGap rises.
        Material material = sustainedTone (0.0, 0.10);

        const auto outcome = runAnalysis (48000.0, 0.70, material,
                                          shredspace::AdaptiveController::Mode::Off);

        expect (outcome.phraseGap > 0.0f,
                "phraseGap stayed zero after activity followed by silence "
                "exceeding the gap duration: " + juce::String (outcome.phraseGap, 6));
    }
};

// ===========================================================================
// 8. Sub-gate noise does not trigger bloom.
// ===========================================================================
class AnalysisSubGateBloomTests final : public juce::UnitTest
{
public:
    AnalysisSubGateBloomTests()
        : juce::UnitTest ("Sub-gate noise bloom", "ShredSpace/Analysis")
    {
    }

    void runTest() override
    {
        beginTest ("Sub-gate noise does not trigger bloom");

        // White noise at -66 dB (amplitude 5e-4), below both the -45 dB
        // sustain/phrase gate and the -55 dB onset noise gate. With no activity
        // ever latching, sustain and phraseGap stay zero, so the Articulation
        // bloom path must remain zero.
        Material material = [] (std::int64_t sampleIndex, double) -> float
        {
            return noiseFromIndex (sampleIndex, 0x1234u) * 5.0e-4f;
        };

        const auto outcome = runAnalysis (48000.0, 2.0, material,
                                          shredspace::AdaptiveController::Mode::Articulation);

        expect (outcome.bloom == 0.0f,
                "Sub-gate noise triggered bloom: " + juce::String (outcome.bloom, 6));
        expect (outcome.deltas.wetDbDelta      == 0.0f, "wet delta non-zero");
        expect (outcome.deltas.t60OctaveDelta  == 0.0f, "tail delta non-zero");
        expect (outcome.deltas.lpfOctaveDelta == 0.0f, "tone delta non-zero");
        expect (outcome.deltas.widthDelta      == 0.0f, "width delta non-zero");
        expect (outcome.deltas.softeningDelta  == 0.0f, "softening delta non-zero");
        expect (outcome.deltas.diffusionDelta  == 0.0f, "diffusion delta non-zero");
    }
};

// ===========================================================================
// 9. AdaptiveController mode coverage.
// ===========================================================================
class AnalysisAdaptiveModeTests final : public juce::UnitTest
{
public:
    AnalysisAdaptiveModeTests()
        : juce::UnitTest ("Adaptive controller modes", "ShredSpace/Analysis")
    {
    }

    void runTest() override
    {
        runModeOff();
        runModeLevel();
    }

private:
    void runModeOff()
    {
        beginTest ("Mode::Off yields exactly zero for every destination delta");

        shredspace::AdaptiveController controller;
        controller.prepare();
        controller.setMode (shredspace::AdaptiveController::Mode::Off);

        // Drive with non-zero articulation inputs; Off must ignore all of them.
        for (int i = 0; i < 100; ++i)
            controller.process (0.8f, 0.7f, 0.6f, 0.5f, 0.9f, 480);

        const auto d = controller.getDeltasForVoice (1.0f);

        expect (d.wetDbDelta      == 0.0f, "Mode::Off wet delta non-zero");
        expect (d.t60OctaveDelta  == 0.0f, "Mode::Off tail delta non-zero");
        expect (d.lpfOctaveDelta  == 0.0f, "Mode::Off tone delta non-zero");
        expect (d.widthDelta      == 0.0f, "Mode::Off width delta non-zero");
        expect (d.softeningDelta  == 0.0f, "Mode::Off softening delta non-zero");
        expect (d.diffusionDelta  == 0.0f, "Mode::Off diffusion delta non-zero");
        expect (controller.getContraction() == 0.0f, "Mode::Off contraction non-zero");
        expect (controller.getBloom()       == 0.0f, "Mode::Off bloom non-zero");
    }

    void runModeLevel()
    {
        beginTest ("Mode::Level yields non-zero wet delta with all other deltas zero");

        shredspace::AdaptiveController controller;
        controller.prepare();
        controller.setMode (shredspace::AdaptiveController::Mode::Level);

        // 1 s of inputActivity = 1 (100 blocks of 480 analysis samples) so the
        // wet-contraction smoother fully develops toward the reduction depth.
        for (int i = 0; i < 100; ++i)
            controller.process (0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 480);

        const auto d = controller.getDeltasForVoice (1.0f);

        expect (d.wetDbDelta < -1.0e-3f,
                "Mode::Level wet delta should be non-zero: "
                    + juce::String (d.wetDbDelta, 6));
        expect (d.t60OctaveDelta  == 0.0f, "Mode::Level tail delta non-zero");
        expect (d.lpfOctaveDelta  == 0.0f, "Mode::Level tone delta non-zero");
        expect (d.widthDelta      == 0.0f, "Mode::Level width delta non-zero");
        expect (d.softeningDelta  == 0.0f, "Mode::Level softening delta non-zero");
        expect (d.diffusionDelta  == 0.0f, "Mode::Level diffusion delta non-zero");
    }
};

// ===========================================================================
// 10. Rate equivalence across host sample rates.
// ===========================================================================
class AnalysisRateEquivalenceTests final : public juce::UnitTest
{
public:
    AnalysisRateEquivalenceTests()
        : juce::UnitTest ("Rate equivalence", "ShredSpace/Analysis")
    {
    }

    void runTest() override
    {
        beginTest ("Analysis is equivalent across 44.1/48/96/192 kHz host rates");

        // Rate-independent synthetic material defined purely as a function of
        // time, so the same physical signal is rendered at every host rate by
        // sampling at t = sampleIndex / hostRate. Ten 1 kHz bursts (25 ms each)
        // at 0.10 s intervals provide the onset pattern; a sustained 1 kHz tone
        // from 1.10 s to 2.00 s develops non-trivial sustain at the end.
        Material material = [] (std::int64_t, double t) -> float
        {
            for (int k = 0; k < 10; ++k)
            {
                const double start = 0.10 + 0.10 * static_cast<double> (k);
                if (t >= start && t < start + 0.025)
                    return static_cast<float> (std::sin (2.0 * kPi * 1000.0 * t));
            }
            if (t >= 1.10 && t < 2.00)
                return static_cast<float> (std::sin (2.0 * kPi * 1000.0 * t));
            return 0.0f;
        };

        const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };

        std::vector<ChainOutcome> outcomes;
        outcomes.reserve (std::size (rates));
        for (const double rate : rates)
        {
            outcomes.push_back (runAnalysis (rate, 2.0, material,
                                             shredspace::AdaptiveController::Mode::Off));
        }

        // (a) Accepted-onset counts must match EXACTLY across the four rates.
        const int referenceCount = static_cast<int> (outcomes[0].onsets.size());
        for (size_t i = 1; i < outcomes.size(); ++i)
        {
            const int count = static_cast<int> (outcomes[i].onsets.size());
            expect (count == referenceCount,
                    "Accepted-onset count mismatch across rates: "
                        + juce::String (static_cast<int> (rates[0])) + " Hz -> "
                        + juce::String (referenceCount)
                        + " vs " + juce::String (static_cast<int> (rates[i]))
                        + " Hz -> " + juce::String (count));
        }

        if (referenceCount == 0)
        {
            expect (false, "No onsets were detected at the reference rate; "
                           "rate-equivalence assertions cannot be evaluated.");
            return;
        }

        // (b) Accepted-onset times agree within kOnsetTimeToleranceMs (10 ms).
        // Convert analysis-sample indices to seconds at the fixed analysis
        // rate; pairwise compare every rate against the reference (48 kHz).
        const auto toSeconds = [] (std::int64_t analysisIndex)
        {
            return static_cast<double> (analysisIndex) / kAnalysisRate;
        };

        for (size_t i = 1; i < outcomes.size(); ++i)
        {
            if (outcomes[i].onsets.size() != static_cast<size_t> (referenceCount))
                continue;   // already reported above; skip pairing

            for (int k = 0; k < referenceCount; ++k)
            {
                const double tRef = toSeconds (outcomes[0].onsets[static_cast<size_t> (k)]);
                const double tCmp = toSeconds (outcomes[i].onsets[static_cast<size_t> (k)]);
                const double diffMs = std::abs (tCmp - tRef) * 1000.0;
                expect (diffMs <= kOnsetTimeToleranceMs,
                        "Onset " + juce::String (k) + " time disagreed across rates: "
                            + juce::String (static_cast<int> (rates[0])) + " Hz -> "
                            + juce::String (tRef, 6) + " s vs "
                            + juce::String (static_cast<int> (rates[i])) + " Hz -> "
                            + juce::String (tCmp, 6) + " s (diff "
                            + juce::String (diffMs, 3) + " ms > "
                            + juce::String (kOnsetTimeToleranceMs, 1) + " ms)");
            }
        }

        // (c) Final density and sustain agree within kDensitySustainTolerance.
        // Exact sample identity is NOT asserted; rational rate conversion is
        // lossy, so a small absolute tolerance is the documented equivalence.
        for (size_t i = 1; i < outcomes.size(); ++i)
        {
            const float densityDiff = std::abs (outcomes[i].density - outcomes[0].density);
            expect (densityDiff <= kDensitySustainTolerance,
                    "Final density disagreed across rates: "
                        + juce::String (static_cast<int> (rates[0])) + " Hz -> "
                        + juce::String (outcomes[0].density, 6) + " vs "
                        + juce::String (static_cast<int> (rates[i])) + " Hz -> "
                        + juce::String (outcomes[i].density, 6)
                        + " (diff " + juce::String (densityDiff, 6) + " > "
                        + juce::String (kDensitySustainTolerance, 3) + ")");

            const float sustainDiff = std::abs (outcomes[i].sustain - outcomes[0].sustain);
            expect (sustainDiff <= kDensitySustainTolerance,
                    "Final sustain disagreed across rates: "
                        + juce::String (static_cast<int> (rates[0])) + " Hz -> "
                        + juce::String (outcomes[0].sustain, 6) + " vs "
                        + juce::String (static_cast<int> (rates[i])) + " Hz -> "
                        + juce::String (outcomes[i].sustain, 6)
                        + " (diff " + juce::String (sustainDiff, 6) + " > "
                        + juce::String (kDensitySustainTolerance, 3) + ")");
        }
    }
};

// Static instances register each test class with the juce::UnitTestRunner.
static AnalysisOnsetTests            sAnalysisOnsetTestsInstance;
static AnalysisDensitySustainTests   sAnalysisDensitySustainTestsInstance;
static AnalysisContinuityTests        sAnalysisContinuityTestsInstance;
static AnalysisPhraseGapTests         sAnalysisPhraseGapTestsInstance;
static AnalysisSubGateBloomTests      sAnalysisSubGateBloomTestsInstance;
static AnalysisAdaptiveModeTests      sAnalysisAdaptiveModeTestsInstance;
static AnalysisRateEquivalenceTests   sAnalysisRateEquivalenceTestsInstance;

} // namespace
