/*
  ==============================================================================
  Safety and Hold unit tests for ShredSpace. Uses ONLY juce::UnitTest; no
  third-party test frameworks (Catch2, GoogleTest, etc.) are involved.

  Coverage:
    SafetyProcessor (mandatory + optional protections):
      1. Stimuli: impulse, full-scale tone, silence, denormal-range values
         (magnitudes near 1e-30), NaN, infinity, maximum feedback, and DC.
         For every stimulus: output is finite and bounded, no runaway
         amplitude growth over a bounded run, the test completes within a
         bounded sample count (no hang), and the atomic safety-intervention
         flag is published exactly when an invalid state was injected.
      2. With the optional RMS limiter disabled, the mandatory layer is still
         active: the 0.985 feedback ceiling still applies, NaN and infinity
         are still rejected, and the DC blocker still removes a constant
         offset.
      3. With the limiter enabled, sustained input above the 0.90 linear
         threshold produces a feedback gain reduction consistent with
         reduction = min(1, 0.90/(energy+epsilon)) and
         g_safe = min(0.985, g_requested*reduction).

    DelayVoice Hold:
      4. Hold kills new delay input: a burst is fed, Hold is engaged, further
         input is fed, and the held tail contains no contribution from the
         post-engage input.
      5. Hold bypasses tone processing so the held tail stays spectrally
         stable: a coarse high-band/low-band energy ratio measured at the
         start and after several seconds of held repeats stays within a
         stated tolerance, demonstrating no progressive darkening.
      6. Hold gain caps at 0.985.
      7. Hold release is click-free across the 30 ms crossfade against an
         explicit sample-delta threshold.

  NaN and infinity injection occurs only inside this test file; production
  code is not modified to enable it.

  Implementation note: SafetyProcessor.cpp and DelayVoice.cpp are compiled
  directly into this translation unit (mirroring the convention used by the
  other DSP test files). SafetyProcessor has no DSP-TU dependencies. DelayVoice
  references FractionalDelay/FeedbackController (DspCoreTests TU),
  RepeatToneProcessor/PresenceGuard/RepeatTransientSoftener (RepeatChainTests
  TU), SoftSaturator (MotionStereoTests TU) and ShortDiffuser (test-runner
  target TU); only their headers are included here, so this introduces no
  duplicate definitions.
  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

#include "../Source/DSP/SafetyProcessor.h"
#include "../Source/DSP/DelayVoice.h"
#include "../Source/DSP/DelayTimeController.h"
#include "../Source/DSP/FractionalDelay.h"
#include "../Source/DSP/FeedbackController.h"

// Compile the implementations this translation unit exercises directly.
#include "../Source/DSP/SafetyProcessor.cpp"
#include "../Source/DSP/DelayVoice.cpp"

namespace
{

constexpr double kSampleRate = 44100.0;
constexpr int    kBlockSize  = 256;
constexpr int    kNumChannels = 2;
constexpr float  kFloatTol = 1.0e-4f;
constexpr double kDoubleTol = 1.0e-6;

// Bounded run length used to prove absence of hangs / runaway growth. Fixed
// and independent of wall-clock time.
constexpr int kBoundedRunBlocks = 40;   // 40 * 256 = 10240 samples (~0.23 s)

// Returns true when every sample in `buffer` is finite and has magnitude no
// greater than `maxAbs`.
bool allFiniteAndBounded (const juce::AudioBuffer<float>& buffer, float maxAbs)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float v = data[i];
            if (! std::isfinite (v) || std::abs (v) > maxAbs)
                return false;
        }
    }
    return true;
}

float maxAbsValue (const juce::AudioBuffer<float>& buffer)
{
    float m = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            m = std::max (m, std::abs (data[i]));
    }
    return m;
}

// Deterministic LCG for broadband bursts. Fixed seed => fully deterministic.
class DeterministicNoise
{
public:
    explicit DeterministicNoise (std::uint32_t seed) : state_ (seed) {}

    float next() noexcept
    {
        state_ = state_ * 1664525u + 1013904223u;
        return static_cast<float> (static_cast<std::int32_t> (state_))
               / static_cast<float> (std::numeric_limits<std::int32_t>::max());
    }

private:
    std::uint32_t state_;
};

// One-pole low-pass / complementary high-pass band split for the coarse
// spectral-stability measurement. Both bands are well above the DC blocker
// corner, so an LTI hold loop preserves their ratio exactly.
struct BandSplitter
{
    double lpState {};
    double lpAlpha {};

    explicit BandSplitter (double cutoffHz, double sampleRate)
    {
        const double tau = 1.0 / (2.0 * juce::MathConstants<double>::pi * cutoffHz);
        lpAlpha = 1.0 - std::exp (-1.0 / (tau * sampleRate));
    }

    void process (double x) noexcept
    {
        lpState += lpAlpha * (x - lpState);
    }
};

struct BandEnergy
{
    double lowRms;
    double highRms;
};

// Measures band-limited RMS of a mono buffer region. Each band is a bandpass
// (LPF(hi) - LPF(lo)) so that the DC region -- where the hold-loop DC blocker
// removes energy every period -- is excluded from both bands. This isolates
// the question of spectral stability (progressive darkening) from the DC
// blocker's intended high-pass behaviour.
BandEnergy measureBandEnergy (const std::vector<float>& mono, int start, int length,
                               double lowLoHz, double lowHiHz,
                               double highLoHz, double highHiHz)
{
    BandSplitter lowLo  { lowLoHz,  kSampleRate };
    BandSplitter lowHi  { lowHiHz,  kSampleRate };
    BandSplitter highLo { highLoHz, kSampleRate };
    BandSplitter highHi { highHiHz, kSampleRate };
    double lowSq = 0.0, highSq = 0.0;
    for (int i = 0; i < length; ++i)
    {
        const double x = static_cast<double> (mono[static_cast<std::size_t> (start + i)]);
        lowLo.process (x);  lowHi.process (x);
        highLo.process (x); highHi.process (x);
        const double lowBand  = lowHi.lpState  - lowLo.lpState;
        const double highBand = highHi.lpState - highLo.lpState;
        lowSq  += lowBand * lowBand;
        highSq += highBand * highBand;
    }
    return { std::sqrt (lowSq / length), std::sqrt (highSq / length) };
}

// Builds a VoiceResult with a fixed integer delay and no transition.
shredspace::VoiceResult makeTimeTarget (double delaySamples)
{
    shredspace::VoiceResult r;
    r.targetDelaySamples = delaySamples;
    r.transitionRequired = false;
    r.transitionMode = shredspace::TransitionMode::smooth;
    return r;
}

// Runs a DelayVoice for `totalSamples` samples in fixed blocks, invoking
// `filler(blockLeft, blockRight, blockIndex)` to supply each block's input.
// The output of every block is appended to `collected` (mono = left channel).
void runVoice (shredspace::DelayVoice& voice,
               int totalSamples,
               const std::function<void (float*, float*, int)>& filler,
               const shredspace::VoiceResult& target,
               double glideMs,
               double tailScale,
               std::vector<float>* collected = nullptr)
{
    std::vector<float> leftBuf (static_cast<std::size_t> (kBlockSize));
    std::vector<float> rightBuf (static_cast<std::size_t> (kBlockSize));

    int processed = 0;
    int blockIndex = 0;
    while (processed < totalSamples)
    {
        const int n = std::min (kBlockSize, totalSamples - processed);
        filler (leftBuf.data(), rightBuf.data(), blockIndex);

        voice.process (leftBuf.data(), rightBuf.data(), n, target, glideMs, tailScale);

        if (collected != nullptr)
            for (int i = 0; i < n; ++i)
                collected->push_back (leftBuf[static_cast<std::size_t> (i)]);

        processed += n;
        ++blockIndex;
    }
}

// ---------------------------------------------------------------------------
// Test 1-3: SafetyProcessor stimuli, mandatory layer, and limiter maths.
// ---------------------------------------------------------------------------
class ShredSpaceSafetyTests final : public juce::UnitTest
{
public:
    ShredSpaceSafetyTests()
        : juce::UnitTest ("Safety processor", "ShredSpace Safety/Hold")
    {
    }

    void runTest() override
    {
        runStimulusTests();
        runMandatoryLayerTests();
        runLimiterMathsTests();
    }

private:
    // -----------------------------------------------------------------
    // Stimulus coverage. Every stimulus is processed over a bounded run
    // (kBoundedRunBlocks blocks). For each we assert:
    //   - output is finite and bounded (no runaway growth),
    //   - the bounded run completes (no hang, by construction of the loop),
    //   - the atomic intervention flag matches the expected value, and
    //   - the buffer-clear request is raised exactly for invalid states.
    // -----------------------------------------------------------------
    void runStimulusTests()
    {
        struct Stimulus
        {
            const char* name;
            bool expectIntervention;
            void (*fill) (juce::AudioBuffer<float>&, int);
        };

        const Stimulus stimuli[] = {
            { "impulse",            false, fillImpulse },
            { "full-scale tone",     false, fillFullScaleTone },
            { "silence",            false, fillSilence },
            { "denormal-range",     false, fillDenormalRange },
            { "NaN",                true,  fillNaN },
            { "infinity",           true,  fillInfinity },
            { "DC",                 false, fillDC },
        };

        for (const auto& s : stimuli)
        {
            beginTest (juce::String ("Safety stimulus: ") + s.name);

            shredspace::SafetyProcessor safety;
            safety.prepare (kSampleRate, kBlockSize, kNumChannels);
            safety.reset();
            safety.setLimiterEnabled (true);

            juce::AudioBuffer<float> buffer (kNumChannels, kBlockSize);

            int blocksCompleted = 0;
            float runMax = 0.0f;

            for (int block = 0; block < kBoundedRunBlocks; ++block)
            {
                s.fill (buffer, block);
                safety.processBlock (buffer);

                ++blocksCompleted; // proves the bounded run completed (no hang)
                runMax = std::max (runMax, maxAbsValue (buffer));

                // After every invalid injection the whole wet block is muted.
                if (s.expectIntervention)
                {
                    expect (maxAbsValue (buffer) == 0.0f,
                            "Invalid-state block was not muted to zero");
                }
            }

            // No hang: the loop ran the full bounded count.
            expectEquals (blocksCompleted, kBoundedRunBlocks,
                          "Bounded run did not complete (potential hang)");

            // Finite and bounded for the whole run.
            expect (runMax <= 2.0f,
                    "Runaway amplitude growth: max=" + juce::String (runMax, 6));

            // Intervention flag must reflect whether an invalid state was
            // injected. With acquire/release semantics.
            expect (safety.safetyInterventionOccurred() == s.expectIntervention,
                    "Intervention flag mismatch for stimulus " + juce::String (s.name));

            // The buffer-clear request is published exactly for invalid states.
            const bool clearRequested = safety.consumeBufferClearRequest();
            expect (clearRequested == s.expectIntervention,
                    "Buffer-clear request mismatch for stimulus " + juce::String (s.name));
            // Consuming is a one-shot exchange.
            expect (! safety.consumeBufferClearRequest(),
                    "Buffer-clear request was not a one-shot exchange");
        }

        // Maximum-feedback stimulus (exercised via applyFeedbackCeiling).
        beginTest ("Safety stimulus: maximum feedback");
        {
            shredspace::SafetyProcessor safety;
            safety.prepare (kSampleRate, kBlockSize, kNumChannels);
            safety.reset();
            safety.setLimiterEnabled (false);

            const float g = safety.applyFeedbackCeiling (1.0e9f);
            expect (std::isfinite (g), "Maximum feedback gain is not finite");
            expect (g <= shredspace::SafetyProcessor::kMaximumFeedbackGain + kFloatTol,
                    "Maximum feedback gain exceeded the 0.985 ceiling: "
                        + juce::String (g, 6));
            expect (std::abs (g - 0.985f) <= kFloatTol,
                    "Maximum feedback gain not at the 0.985 ceiling: "
                        + juce::String (g, 6));
            expect (! safety.safetyInterventionOccurred(),
                    "Finite max-feedback request must not raise intervention");
        }
    }

    // -----------------------------------------------------------------
    // Mandatory layer remains active with the optional limiter disabled.
    // -----------------------------------------------------------------
    void runMandatoryLayerTests()
    {
        beginTest ("Mandatory layer: 0.985 ceiling applies with limiter disabled");
        {
            shredspace::SafetyProcessor safety;
            safety.prepare (kSampleRate, kBlockSize, kNumChannels);
            safety.reset();
            safety.setLimiterEnabled (false);

            // Even after driving the RMS detector to a high energy, the
            // limiter is bypassed so the only ceiling is the mandatory 0.985.
            driveDetectorToHighEnergy (safety);

            expectWithinAbsoluteError (safety.applyFeedbackCeiling (2.0f), 0.985f, kFloatTol,
                                        "Ceiling not 0.985 with limiter disabled (high request)");
            expectWithinAbsoluteError (safety.applyFeedbackCeiling (0.985f), 0.985f, kFloatTol,
                                        "Ceiling not 0.985 with limiter disabled (0.985 request)");
            expectWithinAbsoluteError (safety.applyFeedbackCeiling (0.5f), 0.5f, kFloatTol,
                                        "Sub-ceiling request not passed through");
        }

        beginTest ("Mandatory layer: NaN and infinity rejected with limiter disabled");
        {
            shredspace::SafetyProcessor safety;
            safety.prepare (kSampleRate, kBlockSize, kNumChannels);
            safety.reset();
            safety.setLimiterEnabled (false);

            juce::AudioBuffer<float> buffer (kNumChannels, kBlockSize);
            buffer.clear();
            buffer.getWritePointer (0)[16] = std::numeric_limits<float>::quiet_NaN();
            safety.processBlock (buffer);

            expect (maxAbsValue (buffer) == 0.0f, "NaN not muted with limiter disabled");
            expect (safety.safetyInterventionOccurred(),
                    "NaN did not publish intervention with limiter disabled");

            safety.reset();
            safety.setLimiterEnabled (false);
            buffer.clear();
            buffer.getWritePointer (1)[8] = std::numeric_limits<float>::infinity();
            safety.processBlock (buffer);

            expect (maxAbsValue (buffer) == 0.0f, "Infinity not muted with limiter disabled");
            expect (safety.safetyInterventionOccurred(),
                    "Infinity did not publish intervention with limiter disabled");
        }

        beginTest ("Mandatory layer: DC blocker removes constant offset with limiter disabled");
        {
            shredspace::SafetyProcessor safety;
            safety.prepare (kSampleRate, kBlockSize, kNumChannels);
            safety.reset();
            safety.setLimiterEnabled (false);

            // Feed a constant offset for many blocks; the DC blocker is a
            // one-pole high-pass with R = 0.995, so the offset decays as
            // R^(samples). After kBoundedRunBlocks blocks it is negligible.
            juce::AudioBuffer<float> buffer (kNumChannels, kBlockSize);
            for (int block = 0; block < kBoundedRunBlocks; ++block)
            {
                for (int ch = 0; ch < kNumChannels; ++ch)
                    std::fill (buffer.getWritePointer (ch),
                               buffer.getWritePointer (ch) + kBlockSize,
                               1.0f);
                safety.processBlock (buffer);
            }

            expect (maxAbsValue (buffer) < 1.0e-4f,
                    "DC blocker did not remove constant offset: max="
                        + juce::String (maxAbsValue (buffer), 8));
            expect (! safety.safetyInterventionOccurred(),
                    "Constant offset must not raise intervention");
        }
    }

    // -----------------------------------------------------------------
    // Limiter maths. Drive the RMS detector to a known steady-state energy
    // with a full-scale Nyquist alternating signal, then verify
    //   reduction = min(1, 0.90/(energy+epsilon))
    //   g_safe    = min(0.985, g_requested * reduction)
    // against the analytical steady state.
    // -----------------------------------------------------------------
    void runLimiterMathsTests()
    {
        beginTest ("Limiter: reduction and g_safe match the closed-form expression");

        shredspace::SafetyProcessor safety;
        safety.prepare (kSampleRate, kBlockSize, kNumChannels);
        safety.reset();
        safety.setLimiterEnabled (true);

        // A full-scale alternating (+A,-A,...) signal is a pure-Nyquist tone.
        // The DC blocker (a one-pole high-pass, R = 0.995) has magnitude
        // |H(-1)| = 2/(1+R) at Nyquist, so the steady-state wet output has
        // constant magnitude A*2/(1+R) and the RMS detector settles exactly
        // to energy2 = (A*2/(1+R))^2.
        constexpr double A = 1.0;
        constexpr double R = 0.995; // matches SafetyProcessor::kDcBlockerR
        const double steadyMagnitude = A * 2.0 / (1.0 + R);
        const double steadyEnergy2 = steadyMagnitude * steadyMagnitude;
        constexpr double kEpsilon = 1.0e-12;

        juce::AudioBuffer<float> buffer (kNumChannels, kBlockSize);
        // 100 ms is ~50 attack time-constants: the detector is fully settled.
        const int settlingBlocks = static_cast<int> (std::ceil (0.100 * kSampleRate / kBlockSize));
        for (int block = 0; block < settlingBlocks; ++block)
        {
            fillAlternating (buffer, block);
            safety.processBlock (buffer);
        }

        const auto expectedReduction = [steadyEnergy2](double requested)
        {
            const double energy = std::sqrt (steadyEnergy2 + kEpsilon);
            const double reduction = std::min (1.0, 0.90 / (energy + kEpsilon));
            return std::min (0.985, requested * reduction);
        };

        const float requests[] = { 0.985f, 0.5f, 0.2f, 1.5f, 0.985f * 2.0f };
        for (const float req : requests)
        {
            const float g = safety.applyFeedbackCeiling (req);
            const float expected = static_cast<float> (expectedReduction (req));

            expect (std::isfinite (g), "Limiter gain not finite");
            expect (g <= 0.985f + kFloatTol,
                    "g_safe exceeded 0.985 ceiling for request "
                        + juce::String (req, 4) + ": " + juce::String (g, 6));
            expectWithinAbsoluteError (g, expected, 1.0e-3f,
                                       "g_safe mismatch for request "
                                           + juce::String (req, 4)
                                           + ": got " + juce::String (g, 6)
                                           + " expected " + juce::String (expected, 6));
        }

        // Reduction is strictly below 1 only above the 0.90 linear threshold.
        // Here energy ~= 1.0025 > 0.90, so reduction < 1 and g_safe < request
        // whenever request <= 0.985.
        const float gAtCeiling = safety.applyFeedbackCeiling (0.985f);
        expect (gAtCeiling < 0.985f - 1.0e-3f,
                "Limiter should reduce a 0.985 request below 0.985 when energy > 0.90: "
                    + juce::String (gAtCeiling, 6));
    }

    // Drives the RMS detector to a high but finite energy using a full-scale
    // alternating signal (used by the mandatory-layer ceiling test).
    static void driveDetectorToHighEnergy (shredspace::SafetyProcessor& safety)
    {
        juce::AudioBuffer<float> buffer (kNumChannels, kBlockSize);
        const int blocks = static_cast<int> (std::ceil (0.100 * kSampleRate / kBlockSize));
        for (int block = 0; block < blocks; ++block)
        {
            fillAlternating (buffer, block);
            safety.processBlock (buffer);
        }
    }

    // ---- stimulus fillers -----------------------------------------
    static void fillImpulse (juce::AudioBuffer<float>& b, int block)
    {
        b.clear();
        if (block == 0)
        {
            b.getWritePointer (0)[0] = 1.0f;
            b.getWritePointer (1)[0] = 1.0f;
        }
    }

    static void fillFullScaleTone (juce::AudioBuffer<float>& b, int block)
    {
        const double phase0 = block * kBlockSize;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            auto* data = b.getWritePointer (ch);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double phase = 2.0 * juce::MathConstants<double>::pi * 1000.0
                                     * (phase0 + i) / kSampleRate;
                data[i] = static_cast<float> (std::sin (phase));
            }
        }
    }

    static void fillSilence (juce::AudioBuffer<float>& b, int /*block*/)
    {
        b.clear();
    }

    static void fillDenormalRange (juce::AudioBuffer<float>& b, int /*block*/)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            auto* data = b.getWritePointer (ch);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                // Magnitudes near 1e-30: strictly in the denormal range.
                const float sign = (i % 2 == 0) ? 1.0f : -1.0f;
                data[i] = sign * 1.0e-30f;
            }
        }
    }

    static void fillNaN (juce::AudioBuffer<float>& b, int /*block*/)
    {
        b.clear();
        b.getWritePointer (0)[10] = std::numeric_limits<float>::quiet_NaN();
    }

    static void fillInfinity (juce::AudioBuffer<float>& b, int /*block*/)
    {
        b.clear();
        b.getWritePointer (1)[20] = std::numeric_limits<float>::infinity();
    }

    static void fillDC (juce::AudioBuffer<float>& b, int /*block*/)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            std::fill (b.getWritePointer (ch),
                       b.getWritePointer (ch) + b.getNumSamples(), 1.0f);
    }

    static void fillAlternating (juce::AudioBuffer<float>& b, int /*block*/)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            auto* data = b.getWritePointer (ch);
            for (int i = 0; i < b.getNumSamples(); ++i)
                data[i] = (i % 2 == 0) ? 1.0f : -1.0f;
        }
    }
};

// ---------------------------------------------------------------------------
// Test 4-7: DelayVoice Hold behaviour.
// ---------------------------------------------------------------------------
class ShredSpaceHoldTests final : public juce::UnitTest
{
public:
    ShredSpaceHoldTests()
        : juce::UnitTest ("Hold behaviour", "ShredSpace Safety/Hold")
    {
    }

    void runTest() override
    {
        runHoldKillsNewInputTest();
        runHoldSpectralStabilityTest();
        runHoldGainCapTest();
        runHoldReleaseClickFreeTest();
    }

private:
    static constexpr double kDelaySeconds = 0.10;            // 0.1 s
    static constexpr int    kDelaySamples = static_cast<int> (kDelaySeconds * kSampleRate);
    static constexpr double kTailScale = 1.0;
    static constexpr double kGlideMs = 0.0;
    static constexpr double kT60 = 10.0;                     // long but finite
    static constexpr float  kHoldGain = 0.985f;
    // 30 ms crossfade in samples.
    static constexpr int kCrossfadeSamples = static_cast<int> (0.030 * kSampleRate);

    static void prepareVoice (shredspace::DelayVoice& voice)
    {
        voice.prepare (kSampleRate, kBlockSize);
        voice.reset();
        voice.setT60Target (kT60);
        voice.setCrossFeedback (0.5f);
        voice.setFeedbackGainCeiling (kHoldGain);
        voice.setSaturationAmount (0.0f);
        voice.setDiffusionAmount (0.0f);
        voice.setHold (false);
    }

    // -----------------------------------------------------------------
    // 4. Hold kills new delay input.
    //    Seed a burst, engage Hold, then feed a distinctive full-scale
    //    alternating signal in scenario A and silence in scenario B. Because
    //    Hold zeroes the new input, the held tails must be identical, proving
    //    the post-engage input contributes nothing.
    // -----------------------------------------------------------------
    void runHoldKillsNewInputTest()
    {
        beginTest ("Hold kills new delay input");

        const auto target = makeTimeTarget (static_cast<double> (kDelaySamples));

        auto runScenario = [&] (bool feedDistinctive) -> std::vector<float>
        {
            shredspace::DelayVoice voice;
            prepareVoice (voice);

            std::vector<float> collected;
            collected.reserve (static_cast<std::size_t> (4 * kDelaySamples + kBlockSize));

            // Seed: a 64-sample burst of deterministic noise so the loop has
            // broadband energy to recirculate.
            DeterministicNoise noise { 0xC0FFEE };
            auto seedFiller = [&] (float* left, float* right, int block)
            {
                const int base = block * kBlockSize;
                for (int i = 0; i < kBlockSize; ++i)
                {
                    const float v = (base + i) < 64 ? noise.next() * 0.8f : 0.0f;
                    left[i] = v;
                    right[i] = v;
                }
            };

            // Seed the loop and let the burst enter the delay line.
            runVoice (voice, 2 * kDelaySamples, seedFiller, target, kGlideMs, kTailScale, &collected);

            // Engage Hold at the block boundary.
            voice.setHold (true);

            // Feed further input: distinctive alternating ±0.9 (A) or silence (B).
            auto postFiller = [&] (float* left, float* right, int /*block*/)
            {
                for (int i = 0; i < kBlockSize; ++i)
                {
                    const float v = feedDistinctive ? ((i % 2 == 0) ? 0.9f : -0.9f) : 0.0f;
                    left[i] = v;
                    right[i] = v;
                }
            };
            runVoice (voice, 3 * kDelaySamples, postFiller, target, kGlideMs, kTailScale, &collected);

            return collected;
        };

        const auto tailA = runScenario (true);  // distinctive post-engage input
        const auto tailB = runScenario (false); // silent post-engage input

        expect (tailA.size() == tailB.size(),
                "Scenario output lengths differ: "
                    + juce::String (tailA.size()) + " vs " + juce::String (tailB.size()));

        double maxDiff = 0.0;
        // Skip the seed region (the pre-engage part legitimately differs only
        // in input zeroing... actually pre-engage both used the same seedFiller,
        // so they're identical there too). Compare the full tail.
        for (std::size_t i = 0; i < tailA.size() && i < tailB.size(); ++i)
            maxDiff = std::max (maxDiff, std::abs (static_cast<double> (tailA[i])
                                                     - static_cast<double> (tailB[i])));

        expect (maxDiff <= 1.0e-6,
                "Post-engage input leaked into the held tail: max diff="
                    + juce::String (maxDiff, 8));

        // The held tail must remain finite and bounded regardless.
        float tailMax = 0.0f;
        for (auto v : tailA) tailMax = std::max (tailMax, std::abs (v));
        expect (std::isfinite (tailMax) && tailMax <= 2.0f,
                "Held tail not finite/bounded: max=" + juce::String (tailMax, 6));
    }

    // -----------------------------------------------------------------
    // 5. Hold bypasses tone processing: the held tail stays spectrally
    //    stable over several seconds of repeats (no progressive darkening).
    // -----------------------------------------------------------------
    void runHoldSpectralStabilityTest()
    {
        beginTest ("Hold spectral stability over several seconds");

        // A longer delay keeps the number of cascaded hold-loop DC-blocker
        // applications over the ~3 s measurement span small (6 periods), so
        // the band ratio is preserved to within a tight tolerance.
        constexpr int kSpectralDelaySamples = static_cast<int> (0.50 * kSampleRate);
        const auto target = makeTimeTarget (static_cast<double> (kSpectralDelaySamples));

        shredspace::DelayVoice voice;
        prepareVoice (voice);

        std::vector<float> collected;
        const int seedSamples = 2 * kSpectralDelaySamples;
        const int holdSamples = static_cast<int> (4.5 * kSampleRate);
        collected.reserve (static_cast<std::size_t> (seedSamples + holdSamples));

        DeterministicNoise noise { 0x5EED1234 };
        auto seedFiller = [&] (float* left, float* right, int block)
        {
            const int base = block * kBlockSize;
            for (int i = 0; i < kBlockSize; ++i)
            {
                const float v = (base + i) < 600 ? noise.next() * 0.7f : 0.0f;
                left[i] = v;
                right[i] = v;
            }
        };
        runVoice (voice, seedSamples, seedFiller, target, kGlideMs, kTailScale, &collected);

        voice.setHold (true);
        auto silenceFiller = [&] (float* left, float* right, int /*block*/)
        {
            std::fill (left, left + kBlockSize, 0.0f);
            std::fill (right, right + kBlockSize, 0.0f);
        };
        runVoice (voice, holdSamples, silenceFiller, target, kGlideMs, kTailScale, &collected);

        // Skip the crossfade region (first 30 ms after Hold) so we measure a
        // fully-held tail. Each measurement window spans one delay period.
        const int holdStart = seedSamples;
        const int crossfadeEnd = holdStart + kCrossfadeSamples + kBlockSize;
        const int windowLen = kSpectralDelaySamples;

        // Early window: the first full held period after the crossfade.
        const int earlyStart = crossfadeEnd;
        // Late window: ~3 s later, well within the bounded 4.5 s hold run.
        const int lateStart = earlyStart + static_cast<int> (3.0 * kSampleRate);

        expect (lateStart + windowLen <= static_cast<int> (collected.size()),
                "Collected tail too short for late spectral window");

        // Two bandpass bands, both above the DC-blocker corner, excluding the
        // DC region so the measurement isolates tone-processing darkening.
        const BandEnergy early = measureBandEnergy (collected, earlyStart, windowLen,
                                                    500.0, 2000.0,
                                                    4000.0, 9000.0);
        const BandEnergy late  = measureBandEnergy (collected, lateStart, windowLen,
                                                    500.0, 2000.0,
                                                    4000.0, 9000.0);

        expect (early.lowRms > 1.0e-5 && early.highRms > 1.0e-5,
                "Held tail has no measurable band energy (early)");

        const double earlyRatio = early.highRms / early.lowRms;
        const double lateRatio  = late.highRms / late.lowRms;

        // The Hold loop bypasses all tone processing; only the mild DC blocker
        // is in the loop, so the band ratio is preserved to within a small
        // tolerance. A progressive darkening (high-band attenuation from the
        // bypassed HPF/LPF) would push the late ratio far below this band.
        constexpr double kSpectralTolerance = 0.05;
        const double relError = std::abs (lateRatio - earlyRatio) / earlyRatio;

        expect (relError <= kSpectralTolerance,
                "Spectral drift exceeded tolerance (progressive darkening): early ratio="
                    + juce::String (earlyRatio, 5)
                    + " late ratio=" + juce::String (lateRatio, 5)
                    + " relError=" + juce::String (relError, 5));
    }

    // -----------------------------------------------------------------
    // 6. Hold gain caps at 0.985: the held tail decays by 0.985 per delay
    //    period and never grows.
    // -----------------------------------------------------------------
    void runHoldGainCapTest()
    {
        beginTest ("Hold gain caps at 0.985");

        const auto target = makeTimeTarget (static_cast<double> (kDelaySamples));

        shredspace::DelayVoice voice;
        prepareVoice (voice);

        std::vector<float> collected;
        const int seedSamples = kDelaySamples; // seed a single impulse
        collected.reserve (static_cast<std::size_t> (seedSamples + 6 * kDelaySamples));

        auto impulseFiller = [&] (float* left, float* right, int /*block*/)
        {
            std::fill (left, left + kBlockSize, 0.0f);
            std::fill (right, right + kBlockSize, 0.0f);
            left[0] = 1.0f;
            right[0] = 1.0f;
        };
        runVoice (voice, kBlockSize, impulseFiller, target, kGlideMs, kTailScale, &collected);

        // Flush remaining seed silence so the impulse is well inside the loop.
        auto silenceFiller = [&] (float* left, float* right, int /*block*/)
        {
            std::fill (left, left + kBlockSize, 0.0f);
            std::fill (right, right + kBlockSize, 0.0f);
        };
        runVoice (voice, kDelaySamples, silenceFiller, target, kGlideMs, kTailScale, &collected);

        voice.setHold (true);
        // Run enough periods for several ratio measurements, past the crossfade.
        const int holdSamples = kCrossfadeSamples + 6 * kDelaySamples;
        runVoice (voice, holdSamples, silenceFiller, target, kGlideMs, kTailScale, &collected);

        // Measure per-period peak (max |sample|). The Hold loop's feedback
        // gain is the unconditional 0.985 ceiling; the per-period peak of the
        // recirculating impulse tracks this directly because the hold-loop DC
        // blocker's direct term is unity (y[0] = x[0]). RMS would be inflated
        // by the DC-blocker ring, so peak is the faithful observable here.
        const int holdStart = kDelaySamples + kBlockSize; // after seed region
        const int measureStart = holdStart + kCrossfadeSamples + kBlockSize;

        auto periodPeak = [&] (int periodIndex) -> double
        {
            const int s = measureStart + periodIndex * kDelaySamples;
            if (s + kDelaySamples > static_cast<int> (collected.size()))
                return -1.0;
            double m = 0.0;
            for (int i = 0; i < kDelaySamples; ++i)
                m = std::max (m, std::abs (static_cast<double> (collected[static_cast<std::size_t> (s + i)])));
            return m;
        };

        const double firstPeak = periodPeak (0);
        expect (firstPeak > 0.0, "First held period has no energy");

        double previousPeak = firstPeak;
        for (int p = 1; p < 5; ++p)
        {
            const double currentPeak = periodPeak (p);
            if (currentPeak <= 0.0 || previousPeak <= 0.0)
                break;

            const double ratio = currentPeak / previousPeak;

            // The Hold gain must never exceed the 0.985 cap (no growth), and
            // the loop sustains at the cap.
            expect (ratio <= static_cast<double> (kHoldGain) + 2.0e-3,
                    "Hold gain exceeded 0.985 at period " + juce::String (p)
                        + ": ratio=" + juce::String (ratio, 6));
            expect (ratio >= 0.98,
                    "Hold gain below expected cap at period " + juce::String (p)
                        + ": ratio=" + juce::String (ratio, 6)
                        + " (expected ~0.985)");

            previousPeak = currentPeak;
        }

        // No runaway growth: every period peak stays below the first.
        expect (previousPeak <= firstPeak + 1.0e-6,
                "Held tail peak grew across periods (runaway)");
    }

    // -----------------------------------------------------------------
    // 7. Hold release is click-free across the 30 ms crossfade.
    // -----------------------------------------------------------------
    void runHoldReleaseClickFreeTest()
    {
        beginTest ("Hold release is click-free across 30 ms crossfade");

        // Explicit sample-delta amplitude threshold applied to the crossfade
        // artifact (see below). A click is a step discontinuity of order the
        // held-tail amplitude, well above this.
        constexpr float kClickDeltaThreshold = 0.05f;

        const auto target = makeTimeTarget (static_cast<double> (kDelaySamples));

        // Scenario A releases Hold; scenario B stays held. Both are driven
        // identically up to the release point, so their outputs are identical
        // there. The difference A - B over the 30 ms crossfade isolates the
        // crossfade artifact from the held tail's own signal content (which
        // both scenarios share), so a click shows up as a spike in the
        // difference rather than as ordinary signal deltas.
        auto buildVoice = [this, &target] (shredspace::DelayVoice& v, std::vector<float>& collected)
        {
            prepareVoice (v);
            // Quiesce the normal-loop feedback so that, on release, the normal
            // path simply reads the shared delay line through its tone filters
            // without any feedback-loop buildup. This isolates the 30 ms hold/
            // normal crossfade itself from the normal loop's T60 dynamics, which
            // are exercised elsewhere. The held tail still recirculates at the
            // fixed 0.985 Hold gain independently of this setting.
            v.setT60Target (1.0e-6);

            // Seed a smooth 1 kHz sine so the held tail has no sharp edges of
            // its own; any discontinuity must come from the crossfade.
            auto sineFiller = [&] (float* left, float* right, int block)
            {
                const int base = block * kBlockSize;
                for (int i = 0; i < kBlockSize; ++i)
                {
                    const double phase = 2.0 * juce::MathConstants<double>::pi * 1000.0
                                         * (base + i) / kSampleRate;
                    const float s = static_cast<float> (0.5 * std::sin (phase));
                    left[i] = s;
                    right[i] = s;
                }
            };
            // 0.6 s of sine fills the delay loop with recirculating energy.
            runVoice (v, static_cast<int> (0.6 * kSampleRate), sineFiller,
                      target, kGlideMs, kTailScale, &collected);

            v.setHold (true);
            auto silenceFiller = [&] (float* left, float* right, int /*block*/)
            {
                std::fill (left, left + kBlockSize, 0.0f);
                std::fill (right, right + kBlockSize, 0.0f);
            };
            // Hold for 0.5 s so the tail is fully held and still has energy.
            runVoice (v, static_cast<int> (0.5 * kSampleRate), silenceFiller,
                      target, kGlideMs, kTailScale, &collected);
        };

        auto silenceFiller = [&] (float* left, float* right, int /*block*/)
        {
            std::fill (left, left + kBlockSize, 0.0f);
            std::fill (right, right + kBlockSize, 0.0f);
        };

        std::vector<float> collectedA, collectedB;
        shredspace::DelayVoice voiceA, voiceB;
        buildVoice (voiceA, collectedA);
        buildVoice (voiceB, collectedB);

        const auto releaseStartIndex = collectedA.size();
        expect (collectedA.size() == collectedB.size(),
                "Scenarios diverged in length before release");

        // A releases; B stays held. Both are fed silence so no new-input
        // click is introduced; the assertion targets the crossfade itself.
        voiceA.setHold (false);
        const int releaseSamples = kCrossfadeSamples + kBlockSize;
        runVoice (voiceA, releaseSamples, silenceFiller,
                  target, kGlideMs, kTailScale, &collectedA);
        runVoice (voiceB, releaseSamples, silenceFiller,
                  target, kGlideMs, kTailScale, &collectedB);

        // Inspect the 30 ms crossfade window plus one block of margin.
        const std::size_t lo = releaseStartIndex;
        const std::size_t hi = std::min (collectedA.size(),
                                         releaseStartIndex
                                             + static_cast<std::size_t> (releaseSamples));

        float maxDiff = 0.0f;
        float maxDiffDelta = 0.0f;
        float prevDiff = 0.0f;
        bool first = true;
        for (std::size_t i = lo; i < hi; ++i)
        {
            const float diff = collectedA[i] - collectedB[i];
            maxDiff = std::max (maxDiff, std::abs (diff));
            if (! first)
                maxDiffDelta = std::max (maxDiffDelta, std::abs (diff - prevDiff));
            prevDiff = diff;
            first = false;
        }

        expect (maxDiffDelta <= kClickDeltaThreshold,
                "Hold release click detected: max sample-delta of the crossfade "
                "artifact=" + juce::String (maxDiffDelta, 6)
                + " threshold=" + juce::String (kClickDeltaThreshold, 6));

        // The crossfade artifact must stay bounded.
        expect (maxDiff <= 0.5f,
                "Crossfade artifact unbounded: max |A-B|=" + juce::String (maxDiff, 6));

        // The released tail must remain finite and bounded.
        float tailMax = 0.0f;
        for (std::size_t i = lo; i < hi; ++i)
            tailMax = std::max (tailMax, std::abs (collectedA[i]));
        expect (std::isfinite (tailMax) && tailMax <= 2.0f,
                "Released tail not finite/bounded: max=" + juce::String (tailMax, 6));
    }
};

// Static instances register the tests with juce::UnitTestRunner.
static ShredSpaceSafetyTests sShredSpaceSafetyTestsInstance;
static ShredSpaceHoldTests   sShredSpaceHoldTestsInstance;

} // namespace
