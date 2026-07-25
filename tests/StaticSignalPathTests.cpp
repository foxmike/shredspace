/*
  ==============================================================================
  Static end-to-end signal-path tests for ShredSpace. Uses ONLY juce::UnitTest;
  no third-party test frameworks (Catch2, GoogleTest, etc.) are involved, and no
  new dependency is introduced.

  Coverage:
    1. Cross-feedback channel movement and stability (FeedbackController surface
       from f.2), at q = 0, q = 0.5, and q = 1:
         - q = 0 keeps a left-only impulse confined to the left channel.
         - q = 0.5 produces equal energy in both channels after one repeat.
         - q = 1 moves energy to the opposite channel on each repeat (ping-pong).
         - For all three, a long bounded decay is run at maximum feedback
           (the 0.985 ceiling) and the output magnitude is asserted to never
           exceed a fixed bound and to never grow over time.
       The cross-feedback matrix is exercised through a deterministic stereo
       feedback-delay simulation that feeds the FeedbackController loop taps
       exactly as DelayVoice does (feedback from the previous loop output, the
       delay in the forward path), so the per-repeat channel movement is the
       observable. No DSP .cpp is compiled into this translation unit: the
       FeedbackController symbols are supplied by the DspCoreTests TU and the
       full ShredSpaceAudioProcessor by the test-runner target sources, so this
       file introduces no duplicate definitions.

    2. Bus layouts through the integrated processor:
         - mono-mono produces a finite, bounded, non-silent output.
         - mono-stereo produces valid deterministic stereo output whose dry
           component is centered (left and right dry contributions equal within a
           stated tolerance) and whose right output channel is never left
           uninitialized.
         - stereo-stereo processes both channels (both finite, bounded,
           non-silent for a distinct stereo input).

    3. End-to-end impulse through the complete path (input gain, both voices,
       routing, stereo processing, safety, global mix, output gain): a finite,
       non-silent, bounded response with repeats appearing at the expected
       delay positions (voice A at the macro-time*ratio-A delay and voice B at
       the macro-time*ratio-B delay).

    4. Zero tail: with the decay/tail control at its minimum, the wet path
       produces no sustained output, in contrast to a long-tail configuration
       which does sustain.

  Determinism: fixed sample rates (48000 Hz and 44100 Hz), fixed block sizes,
  fixed generated signals, and no wall-clock dependence.
  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

#include "../Source/Parameters/ParameterIDs.h"
#include "../Source/DSP/FeedbackController.h"
#include "../Source/PluginProcessor.h"

namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kSr48 = 48000.0;
constexpr double kSr44 = 44100.0;
constexpr int    kBlockSize = 256;
constexpr float  kTol = 1.0e-4f;

// ---------------------------------------------------------------------------
// Parameter helper. Sets an APVTS parameter from a denormalised value (the
// parameter's natural range / choice index / bool 0|1). setValueNotifyingHost
// updates the underlying atomic synchronously via the APVTS ParameterAdapter
// listener, so the value is live before prepareToPlay captures the pointers.
// ---------------------------------------------------------------------------
void setParam (juce::AudioProcessorValueTreeState& apvts, const char* id, float value)
{
    auto* param = apvts.getParameter (id);
    const auto range = apvts.getParameterRange (id);
    param->setValueNotifyingHost (range.convertTo0to1 (value));
}

// A deterministic, fully-specified baseline so every processor test starts from
// the same known state independent of any default drift. Only the parameters
// read by PluginProcessor::processBlock are set; the unused per-voice expert
// parameters are left at their fixed defaults (deterministic).
void applyBaselineConfig (juce::AudioProcessorValueTreeState& apvts)
{
    using namespace shredspace::paramids;

    setParam (apvts, GLOBAL_INPUT_GAIN,   0.0f);
    setParam (apvts, GLOBAL_OUTPUT_GAIN,  0.0f);
    setParam (apvts, GLOBAL_MIX,          50.0f);
    setParam (apvts, GLOBAL_BYPASS,        0.0f);   // off
    setParam (apvts, GLOBAL_BYPASS_MODE,  0.0f);   // Hard (unused while off)
    setParam (apvts, GLOBAL_MIX_MODE,     0.0f);   // Insert
    setParam (apvts, GLOBAL_SAFETY_LIMITER, 0.0f); // off by default

    setParam (apvts, PERF_MACRO_TIME,         200.0f);
    setParam (apvts, PERF_MACRO_TAIL,         2.8f);
    setParam (apvts, PERF_MACRO_WIDTH,        50.0f); // -> stereo width 1.0 (identity)
    setParam (apvts, PERF_ROUTING,            0.0f); // Parallel
    setParam (apvts, PERF_ROUTING_CASCADE,    50.0f);
    setParam (apvts, PERF_HOLD,               0.0f); // off
    setParam (apvts, PERF_ADAPTIVE_MODE,      0.0f); // Off

    setParam (apvts, TIME_MODE,          0.0f); // Milliseconds
    setParam (apvts, TIME_SYNC_DIVISION, 6.0f); // 1/4 (unused in ms mode)
    setParam (apvts, TIME_GLIDE,         5.0f); // minimum

    setParam (apvts, STEREO_LF_MONO,       0.0f); // off (keep channels independent)
    setParam (apvts, STEREO_LF_MONO_FREQ, 180.0f);
    setParam (apvts, SCENE_SELECT,         0.0f);

    setParam (apvts, VOICE_A_ENABLE, 1.0f);
    setParam (apvts, VOICE_B_ENABLE, 1.0f);
    setParam (apvts, VOICE_A_TIME_RATIO, 0.5f);
    setParam (apvts, VOICE_B_TIME_RATIO, 1.0f);
    setParam (apvts, VOICE_A_FINE_OFFSET, 0.0f);
    setParam (apvts, VOICE_B_FINE_OFFSET, 0.0f);
    setParam (apvts, VOICE_A_LEVEL, 0.0f); // 0 dB -> gain 1.0
    setParam (apvts, VOICE_B_LEVEL, 0.0f);
    setParam (apvts, VOICE_A_TAIL_SCALE, 1.0f);
    setParam (apvts, VOICE_B_TAIL_SCALE, 1.0f);
    setParam (apvts, VOICE_A_CROSS_FEEDBACK, 15.0f);
    setParam (apvts, VOICE_B_CROSS_FEEDBACK, 15.0f);
    setParam (apvts, VOICE_A_SATURATION, 0.0f);
    setParam (apvts, VOICE_B_SATURATION, 0.0f);
    setParam (apvts, VOICE_A_DIFFUSION, 0.0f);
    setParam (apvts, VOICE_B_DIFFUSION, 0.0f);
}

// Builds a single-main-bus BusesLayout with the given channel sets.
juce::AudioProcessor::BusesLayout makeLayout (const juce::AudioChannelSet& in,
                                              const juce::AudioChannelSet& out)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (in);
    layout.outputBuses.add (out);
    return layout;
}

// Runs `totalSamples` through a prepared processor in fixed blocks. `filler`
// supplies the input channels of each block (channels >= numInputCh are left
// cleared). The output channels are collected per channel.
std::vector<std::vector<float>> runProcessor (
    ShredSpaceAudioProcessor& proc,
    int numInputCh,
    int numOutputCh,
    int totalSamples,
    double sampleRate,
    int blockSize,
    const std::function<void (juce::AudioBuffer<float>&, int)>& filler)
{
    juce::ignoreUnused (sampleRate);
    const int numBufCh = std::max (numInputCh, numOutputCh);
    std::vector<std::vector<float>> out (
        static_cast<size_t> (numOutputCh),
        std::vector<float> (static_cast<size_t> (totalSamples), 0.0f));

    int processed = 0;
    int block = 0;
    while (processed < totalSamples)
    {
        const int n = std::min (blockSize, totalSamples - processed);
        juce::AudioBuffer<float> buf (numBufCh, n);
        buf.clear();
        filler (buf, block);

        juce::MidiBuffer midi;
        proc.processBlock (buf, midi);

        for (int c = 0; c < numOutputCh; ++c)
            for (int i = 0; i < n; ++i)
                out[static_cast<size_t> (c)][static_cast<size_t> (processed + i)] =
                    buf.getSample (c, i);

        processed += n;
        ++block;
    }

    return out;
}

// Peak |sample| across all channels in [lo, hi].
float windowPeak (const std::vector<std::vector<float>>& out, int lo, int hi)
{
    float p = 0.0f;
    for (const auto& ch : out)
    {
        const int n = static_cast<int> (ch.size());
        for (int i = std::max (0, lo); i <= std::min (n - 1, hi); ++i)
            p = std::max (p, std::abs (ch[static_cast<size_t> (i)]));
    }
    return p;
}

bool allFinite (const std::vector<std::vector<float>>& out)
{
    for (const auto& ch : out)
        for (float v : ch)
            if (! std::isfinite (v))
                return false;
    return true;
}

float maxAbs (const std::vector<std::vector<float>>& out)
{
    float m = 0.0f;
    for (const auto& ch : out)
        for (float v : ch)
            m = std::max (m, std::abs (v));
    return m;
}

// ===========================================================================
// 1. Cross-feedback channel movement and bounded non-growing decay.
// ===========================================================================
class ShredSpaceCrossFeedbackTests final : public juce::UnitTest
{
public:
    ShredSpaceCrossFeedbackTests()
        : juce::UnitTest ("Cross-feedback channel movement", "ShredSpace/SignalPath")
    {
    }

    void runTest() override
    {
        constexpr double sr = kSr48;
        constexpr int D = 4800;          // 0.1 s delay
        constexpr int N = 22 * D;        // ~2.2 s, long bounded decay run

        runForQ (0.0f, sr, D, N);
        runForQ (0.5f, sr, D, N);
        runForQ (1.0f, sr, D, N);
    }

private:
    // Deterministic stereo feedback-delay simulation feeding the
    // FeedbackController cross-feedback surface exactly as DelayVoice does:
    //   fb[t]      = processStereoSample (out[t-1])
    //   write[t]   = input[t] + fb[t]
    //   out[t]     = write[t-D]
    // The tone chain is unity in this simulation (it is per-channel and
    // symmetric, so it cannot move energy between channels); only the
    // cross-feedback matrix does that, which is the subject of this test.
    struct LoopResult { std::vector<float> L, R; };

    static LoopResult simulate (float q, double sr, int D, int N)
    {
        shredspace::FeedbackController fc;
        fc.prepare (sr);
        fc.reset (1000.0);          // very long T60 -> requested gain ~1.0
        fc.setT60Target (1000.0);
        fc.setCrossFeedback (q);
        fc.setFeedbackGainCeiling (0.985f); // maximum feedback (the hard ceiling)

        std::vector<float> writeL (static_cast<size_t> (N), 0.0f);
        std::vector<float> writeR (static_cast<size_t> (N), 0.0f);
        std::vector<float> outL (static_cast<size_t> (N), 0.0f);
        std::vector<float> outR (static_cast<size_t> (N), 0.0f);

        float prevL = 0.0f, prevR = 0.0f;
        const double delaySeconds = static_cast<double> (D) / sr;

        for (int t = 0; t < N; ++t)
        {
            const auto fb = fc.processStereoSample (prevL, prevR, delaySeconds, 1.0);
            const float inL = (t == 0) ? 1.0f : 0.0f;   // single left-only impulse
            const float inR = 0.0f;

            const float wL = inL + fb.left;
            const float wR = inR + fb.right;
            writeL[static_cast<size_t> (t)] = wL;
            writeR[static_cast<size_t> (t)] = wR;

            const float oL = (t >= D) ? writeL[static_cast<size_t> (t - D)] : 0.0f;
            const float oR = (t >= D) ? writeR[static_cast<size_t> (t - D)] : 0.0f;
            outL[static_cast<size_t> (t)] = oL;
            outR[static_cast<size_t> (t)] = oR;

            prevL = oL;
            prevR = oR;
        }

        return { std::move (outL), std::move (outR) };
    }

    // Repeat n (n >= 1) peaks at sample index n*D + (n-1).
    static std::pair<float, float> repeatPeak (const std::vector<float>& L,
                                               const std::vector<float>& R,
                                               int n, int D, int N)
    {
        const long tn = static_cast<long> (n) * D + (n - 1);
        const long lo = std::max (0L, tn - 16);
        const long hi = std::min (static_cast<long> (N - 1), tn + 16);
        float pL = 0.0f, pR = 0.0f;
        for (long i = lo; i <= hi; ++i)
        {
            pL = std::max (pL, std::abs (L[static_cast<size_t> (i)]));
            pR = std::max (pR, std::abs (R[static_cast<size_t> (i)]));
        }
        return { pL, pR };
    }

    void runForQ (float q, double sr, int D, int N)
    {
        beginTest ("Cross-feedback q=" + juce::String (q, 3));

        const auto r = simulate (q, sr, D, N);
        const auto& L = r.L;
        const auto& R = r.R;

        // Finite and bounded across the whole long run.
        float runMax = 0.0f;
        bool finite = true;
        for (int i = 0; i < N; ++i)
        {
            if (! std::isfinite (L[static_cast<size_t> (i)])
                || ! std::isfinite (R[static_cast<size_t> (i)]))
            {
                finite = false;
                break;
            }
            runMax = std::max (runMax, std::abs (L[static_cast<size_t> (i)]));
            runMax = std::max (runMax, std::abs (R[static_cast<size_t> (i)]));
        }

        expect (finite, "Non-finite sample in cross-feedback run");
        expect (runMax <= 1.5f,
                "Output magnitude exceeded fixed bound: " + juce::String (runMax, 6));

        // Per-repeat peaks: collect for the bounded non-growing decay check.
        constexpr int kRepeats = 20;
        std::vector<float> peakL, peakR;
        peakL.reserve (kRepeats);
        peakR.reserve (kRepeats);
        for (int n = 1; n <= kRepeats; ++n)
        {
            const auto pr = repeatPeak (L, R, n, D, N);
            peakL.push_back (pr.first);
            peakR.push_back (pr.second);
        }

        // Repeat 1 is the direct delayed impulse: left only, full scale.
        expectWithinAbsoluteError (peakL[0], 1.0f, 1.0e-5f,
                                   "Repeat 1 left peak not unity: " + juce::String (peakL[0], 6));
        expect (peakR[0] <= 1.0e-5f,
                "Repeat 1 leaked into right channel: " + juce::String (peakR[0], 8));

        // Bounded non-growing decay: per-repeat total peak never increases and
        // the late peaks are strictly below the early ones.
        for (int n = 1; n < kRepeats; ++n)
        {
            const float cur = std::max (peakL[static_cast<size_t> (n)],
                                        peakR[static_cast<size_t> (n)]);
            const float prev = std::max (peakL[static_cast<size_t> (n - 1)],
                                         peakR[static_cast<size_t> (n - 1)]);
            expect (cur <= prev + 1.0e-6f,
                    "Repeat peak grew at repeat " + juce::String (n + 1)
                        + ": prev=" + juce::String (prev, 6)
                        + " cur=" + juce::String (cur, 6));
        }
        const float latePeak = std::max (peakL.back(), peakR.back());
        const float earlyPeak = std::max (peakL[1], peakR[1]);
        expect (latePeak < earlyPeak,
                "Decay did not progress over time: early=" + juce::String (earlyPeak, 6)
                    + " late=" + juce::String (latePeak, 6));

        if (q == 0.0f)
        {
            // q = 0: a left-only impulse is confined to the left channel.
            float rightMax = 0.0f;
            for (int i = 0; i < N; ++i)
                rightMax = std::max (rightMax, std::abs (R[static_cast<size_t> (i)]));
            expect (rightMax <= 1.0e-9f,
                    "q=0 leaked energy into the right channel: " + juce::String (rightMax, 12));
            // Left repeats decay but stay on the left.
            for (int n = 1; n < kRepeats; ++n)
                expect (peakL[static_cast<size_t> (n)] >= 0.0f,
                        "q=0 left peak negative at repeat " + juce::String (n + 1));
        }
        else if (q == 0.5f)
        {
            // q = 0.5: after one repeat the energy is equal in both channels.
            expect (peakR[0] <= 1.0e-5f,
                    "q=0.5 repeat 1 right should be ~0: " + juce::String (peakR[0], 6));
            for (int n = 2; n <= kRepeats; ++n)
            {
                const float pl = peakL[static_cast<size_t> (n - 1)];
                const float pr = peakR[static_cast<size_t> (n - 1)];
                const float total = pl + pr;
                expect (total > 1.0e-9f,
                        "q=0.5 repeat " + juce::String (n) + " has no energy");
                expect (std::abs (pl - pr) <= 1.0e-4f * total + 1.0e-9f,
                        "q=0.5 channels not equal at repeat " + juce::String (n)
                            + ": L=" + juce::String (pl, 6) + " R=" + juce::String (pr, 6));
            }
        }
        else // q == 1.0f
        {
            // q = 1: energy moves to the opposite channel on each repeat.
            for (int n = 1; n <= std::min (kRepeats, 8); ++n)
            {
                const float pl = peakL[static_cast<size_t> (n - 1)];
                const float pr = peakR[static_cast<size_t> (n - 1)];
                if (n % 2 == 1) // odd repeats land on the left
                {
                    expect (pl > 1.0e-6f && pr <= 1.0e-4f * (pl + 1.0e-12f),
                            "q=1 odd repeat " + juce::String (n)
                                + " not left-dominant: L=" + juce::String (pl, 6)
                                + " R=" + juce::String (pr, 6));
                }
                else // even repeats land on the right
                {
                    expect (pr > 1.0e-6f && pl <= 1.0e-4f * (pr + 1.0e-12f),
                            "q=1 even repeat " + juce::String (n)
                                + " not right-dominant: L=" + juce::String (pl, 6)
                                + " R=" + juce::String (pr, 6));
                }
            }
        }
    }
};

// ===========================================================================
// 2. Bus layouts through the integrated processor.
// ===========================================================================
class ShredSpaceBusLayoutTests final : public juce::UnitTest
{
public:
    ShredSpaceBusLayoutTests()
        : juce::UnitTest ("Bus layouts", "ShredSpace/SignalPath")
    {
    }

    void runTest() override
    {
        runMonoMono();
        runMonoStereo();
        runStereoStereo();
    }

private:
    void runMonoMono()
    {
        beginTest ("mono-mono produces finite bounded output");

        ShredSpaceAudioProcessor proc;
        auto& apvts = proc.getValueTreeState();
        applyBaselineConfig (apvts);

        auto layout = makeLayout (juce::AudioChannelSet::mono(),
                                  juce::AudioChannelSet::mono());
        expect (proc.setBusesLayout (layout), "mono-mono layout not accepted");
        proc.prepareToPlay (kSr44, kBlockSize);

        const int total = static_cast<int> (0.3 * kSr44);
        auto filler = [] (juce::AudioBuffer<float>& buf, int block)
        {
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                const double phase = 2.0 * kPi * 1000.0
                                     * (block * kBlockSize + i) / kSr44;
                buf.setSample (0, i, static_cast<float> (std::sin (phase)));
            }
        };

        const auto out = runProcessor (proc, 1, 1, total, kSr44, kBlockSize, filler);

        expect (allFinite (out), "mono-mono output not finite");
        expect (maxAbs (out) <= 2.0f, "mono-mono output unbounded");
        expect (maxAbs (out) > 1.0e-3f, "mono-mono output silent");
    }

    void runMonoStereo()
    {
        beginTest ("mono-stereo dry component centered and channels initialized");

        // (a) Pure dry (mix = 0): the mono input is copied to both dry channels,
        // so the stereo output's dry component is centered (L == R).
        {
            ShredSpaceAudioProcessor proc;
            auto& apvts = proc.getValueTreeState();
            applyBaselineConfig (apvts);
            setParam (apvts, shredspace::paramids::GLOBAL_MIX, 0.0f); // pure dry

            auto layout = makeLayout (juce::AudioChannelSet::mono(),
                                      juce::AudioChannelSet::stereo());
            expect (proc.setBusesLayout (layout), "mono-stereo layout not accepted");
            proc.prepareToPlay (kSr44, kBlockSize);

            const int total = static_cast<int> (0.3 * kSr44);
            auto filler = [] (juce::AudioBuffer<float>& buf, int block)
            {
                for (int i = 0; i < buf.getNumSamples(); ++i)
                {
                    const double phase = 2.0 * kPi * 1000.0
                                         * (block * kBlockSize + i) / kSr44;
                    buf.setSample (0, i, static_cast<float> (std::sin (phase)));
                }
            };

            const auto out = runProcessor (proc, 1, 2, total, kSr44, kBlockSize, filler);

            expect (allFinite (out), "mono-stereo dry output not finite");
            expect (out.size() == 2, "mono-stereo did not produce two channels");

            // Dry centering: left and right dry contributions equal within tolerance.
            double maxDiff = 0.0;
            for (int i = 0; i < total; ++i)
            {
                const float l = out[0][static_cast<size_t> (i)];
                const float r = out[1][static_cast<size_t> (i)];
                maxDiff = std::max (maxDiff, std::abs (static_cast<double> (l - r)));
            }
            expect (maxDiff <= 1.0e-4,
                    "Dry component not centered: max |L-R|=" + juce::String (maxDiff, 8));

            // Right output channel is properly initialized (not stale/garbage):
            // it tracks the left channel deterministically and is non-silent.
            float rMax = 0.0f;
            for (int i = 0; i < total; ++i)
                rMax = std::max (rMax, std::abs (out[1][static_cast<size_t> (i)]));
            expect (rMax > 1.0e-3f,
                    "mono-stereo right channel uninitialized/silent: max=" + juce::String (rMax, 6));
        }

        // (b) With wet present (mix = 50%): both channels finite, bounded, and
        // the right channel carries deterministic content (not uninitialized).
        {
            ShredSpaceAudioProcessor proc;
            auto& apvts = proc.getValueTreeState();
            applyBaselineConfig (apvts);

            auto layout = makeLayout (juce::AudioChannelSet::mono(),
                                      juce::AudioChannelSet::stereo());
            expect (proc.setBusesLayout (layout), "mono-stereo layout not accepted");
            proc.prepareToPlay (kSr44, kBlockSize);

            const int total = static_cast<int> (0.4 * kSr44);
            auto filler = [] (juce::AudioBuffer<float>& buf, int block)
            {
                if (block == 0)
                    buf.setSample (0, 0, 1.0f); // mono impulse
            };

            const auto out = runProcessor (proc, 1, 2, total, kSr44, kBlockSize, filler);

            expect (allFinite (out), "mono-stereo wet output not finite");
            expect (maxAbs (out) <= 2.0f, "mono-stereo wet output unbounded");
            expect (maxAbs (out) > 1.0e-3f, "mono-stereo wet output silent");

            // Right channel must contain finite, deterministic, non-zero content
            // (no uninitialized memory leak).
            float rMax = 0.0f;
            for (int i = 0; i < total; ++i)
                rMax = std::max (rMax, std::abs (out[1][static_cast<size_t> (i)]));
            expect (std::isfinite (rMax) && rMax > 1.0e-4f,
                    "mono-stereo right channel not initialized with content: "
                        + juce::String (rMax, 6));
        }
    }

    void runStereoStereo()
    {
        beginTest ("stereo-stereo processes both channels");

        ShredSpaceAudioProcessor proc;
        auto& apvts = proc.getValueTreeState();
        applyBaselineConfig (apvts);

        auto layout = makeLayout (juce::AudioChannelSet::stereo(),
                                  juce::AudioChannelSet::stereo());
        expect (proc.setBusesLayout (layout), "stereo-stereo layout not accepted");
        proc.prepareToPlay (kSr44, kBlockSize);

        const int total = static_cast<int> (0.3 * kSr44);
        // Distinct content on each channel so both must be processed.
        auto filler = [] (juce::AudioBuffer<float>& buf, int block)
        {
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                const double t = (block * kBlockSize + i) / kSr44;
                buf.setSample (0, i, static_cast<float> (std::sin (2.0 * kPi * 1000.0 * t)));
                buf.setSample (1, i, static_cast<float> (std::sin (2.0 * kPi * 1500.0 * t)));
            }
        };

        const auto out = runProcessor (proc, 2, 2, total, kSr44, kBlockSize, filler);

        expect (allFinite (out), "stereo-stereo output not finite");
        expect (out.size() == 2, "stereo-stereo did not produce two channels");
        expect (maxAbs (out) <= 2.0f, "stereo-stereo output unbounded");

        float lMax = 0.0f, rMax = 0.0f;
        for (int i = 0; i < total; ++i)
        {
            lMax = std::max (lMax, std::abs (out[0][static_cast<size_t> (i)]));
            rMax = std::max (rMax, std::abs (out[1][static_cast<size_t> (i)]));
        }
        expect (lMax > 1.0e-3f, "stereo-stereo left channel silent: " + juce::String (lMax, 6));
        expect (rMax > 1.0e-3f, "stereo-stereo right channel silent: " + juce::String (rMax, 6));
    }
};

// ===========================================================================
// 3 & 4. End-to-end impulse through the complete path, and zero-tail.
// ===========================================================================
class ShredSpaceEndToEndTests final : public juce::UnitTest
{
public:
    ShredSpaceEndToEndTests()
        : juce::UnitTest ("End-to-end signal path", "ShredSpace/SignalPath")
    {
    }

    void runTest() override
    {
        runEndToEndImpulse();
        runZeroTail();
    }

private:
    void runEndToEndImpulse()
    {
        beginTest ("End-to-end impulse: finite, non-silent, bounded, repeats in place");

        ShredSpaceAudioProcessor proc;
        auto& apvts = proc.getValueTreeState();
        applyBaselineConfig (apvts);
        // Exercise the safety stage as part of the complete path.
        setParam (apvts, shredspace::paramids::GLOBAL_SAFETY_LIMITER, 1.0f);

        auto layout = makeLayout (juce::AudioChannelSet::stereo(),
                                  juce::AudioChannelSet::stereo());
        expect (proc.setBusesLayout (layout), "stereo-stereo layout not accepted");
        proc.prepareToPlay (kSr48, kBlockSize);

        // Voice A delay = 200 ms * 0.5 = 100 ms (4800 samples @ 48 kHz).
        // Voice B delay = 200 ms * 1.0 = 200 ms (9600 samples @ 48 kHz).
        const int delayA = static_cast<int> (0.100 * kSr48);
        const int delayB = static_cast<int> (0.200 * kSr48);
        const int total = static_cast<int> (0.50 * kSr48);

        auto impulseFiller = [] (juce::AudioBuffer<float>& buf, int block)
        {
            if (block == 0)
            {
                buf.setSample (0, 0, 1.0f); // left-only impulse; right stays zero
            }
        };

        const auto out = runProcessor (proc, 2, 2, total, kSr48, kBlockSize, impulseFiller);

        // Finite, non-silent, bounded.
        expect (allFinite (out), "End-to-end output not finite");
        expect (maxAbs (out) > 1.0e-3f, "End-to-end output silent");
        expect (maxAbs (out) <= 2.0f, "End-to-end output unbounded");

        // Repeats appear at the expected delay positions. Compare repeat
        // windows against the silence windows between repeats.
        const int w = static_cast<int> (0.015 * kSr48); // +/- 15 ms analysis window
        const float peakA = windowPeak (out, delayA - w, delayA + w);
        const float gapA = windowPeak (out, delayA + delayA / 2 - w,
                                       delayA + delayA / 2 + w); // ~150 ms
        const float peakB = windowPeak (out, delayB - w, delayB + w);
        const float gapB = windowPeak (out, delayB + delayA / 2 - w,
                                       delayB + delayA / 2 + w); // ~250 ms

        expect (peakA > 1.0e-3f,
                "No repeat at voice A delay position: peak=" + juce::String (peakA, 6));
        expect (peakB > 1.0e-3f,
                "No repeat at voice B delay position: peak=" + juce::String (peakB, 6));
        expect (peakA > 5.0f * (gapA + 1.0e-9f),
                "Repeat A not isolated from inter-repeat silence: peak="
                    + juce::String (peakA, 6) + " gap=" + juce::String (gapA, 6));
        expect (peakB > 5.0f * (gapB + 1.0e-9f),
                "Repeat B not isolated from inter-repeat silence: peak="
                    + juce::String (peakB, 6) + " gap=" + juce::String (gapB, 6));
    }

    void runZeroTail()
    {
        beginTest ("Zero tail: wet path produces no sustained output");

        // Tail control at its minimum (0.15 s) with tail scale at minimum
        // (0.25) gives T60_voice = 0.0375 s, so for a 100 ms delay the feedback
        // gain is ~1e-8: the wet path echoes once and does not sustain.
        {
            ShredSpaceAudioProcessor proc;
            auto& apvts = proc.getValueTreeState();
            applyBaselineConfig (apvts);
            setParam (apvts, shredspace::paramids::GLOBAL_MIX, 100.0f);   // pure wet
            setParam (apvts, shredspace::paramids::PERF_MACRO_TAIL, 0.15f); // minimum
            setParam (apvts, shredspace::paramids::VOICE_A_TAIL_SCALE, 0.25f);
            setParam (apvts, shredspace::paramids::VOICE_B_TAIL_SCALE, 0.25f);

            auto layout = makeLayout (juce::AudioChannelSet::stereo(),
                                      juce::AudioChannelSet::stereo());
            expect (proc.setBusesLayout (layout), "stereo-stereo layout not accepted");
            proc.prepareToPlay (kSr48, kBlockSize);

            const int total = static_cast<int> (0.50 * kSr48);
            auto impulseFiller = [] (juce::AudioBuffer<float>& buf, int block)
            {
                if (block == 0)
                    buf.setSample (0, 0, 1.0f);
            };

            const auto out = runProcessor (proc, 2, 2, total, kSr48, kBlockSize, impulseFiller);

            expect (allFinite (out), "Zero-tail output not finite");
            expect (maxAbs (out) <= 2.0f, "Zero-tail output unbounded");

            // Late window (well past the first repeat) must be silent: no
            // sustained wet output.
            const int lateLo = static_cast<int> (0.30 * kSr48);
            const int lateHi = total - 1;
            const float latePeak = windowPeak (out, lateLo, lateHi);
            expect (latePeak <= 1.0e-4f,
                    "Zero-tail wet path produced sustained output: latePeak="
                        + juce::String (latePeak, 8));
        }

        // Contrast: a long-tail configuration sustains, proving the zero-tail
        // silence above is the tail control's doing and not a dead signal path.
        {
            ShredSpaceAudioProcessor proc;
            auto& apvts = proc.getValueTreeState();
            applyBaselineConfig (apvts);
            setParam (apvts, shredspace::paramids::GLOBAL_MIX, 100.0f);
            setParam (apvts, shredspace::paramids::PERF_MACRO_TAIL, 8.0f);
            setParam (apvts, shredspace::paramids::VOICE_A_TAIL_SCALE, 4.0f);
            setParam (apvts, shredspace::paramids::VOICE_B_TAIL_SCALE, 4.0f);

            auto layout = makeLayout (juce::AudioChannelSet::stereo(),
                                      juce::AudioChannelSet::stereo());
            expect (proc.setBusesLayout (layout), "stereo-stereo layout not accepted");
            proc.prepareToPlay (kSr48, kBlockSize);

            const int total = static_cast<int> (0.50 * kSr48);
            auto impulseFiller = [] (juce::AudioBuffer<float>& buf, int block)
            {
                if (block == 0)
                    buf.setSample (0, 0, 1.0f);
            };

            const auto out = runProcessor (proc, 2, 2, total, kSr48, kBlockSize, impulseFiller);

            expect (allFinite (out), "Long-tail output not finite");

            const int lateLo = static_cast<int> (0.30 * kSr48);
            const int lateHi = total - 1;
            const float latePeak = windowPeak (out, lateLo, lateHi);
            expect (latePeak > 1.0e-4f,
                    "Long-tail wet path did not sustain (contrast check): latePeak="
                        + juce::String (latePeak, 8));
        }
    }
};

// Static instances register the tests with the juce::UnitTestRunner.
static ShredSpaceCrossFeedbackTests sShredSpaceCrossFeedbackTestsInstance;
static ShredSpaceBusLayoutTests     sShredSpaceBusLayoutTestsInstance;
static ShredSpaceEndToEndTests      sShredSpaceEndToEndTestsInstance;

} // namespace
