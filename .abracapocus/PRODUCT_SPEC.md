# Shred-focused adaptive lead delay
## Buildable product and DSP specification
**Working title:** `ShredSpace`
**Product category:** Performance-responsive dual delay for articulate high-gain lead guitar.
This specification deliberately avoids becoming a universal delay laboratory. Every major subsystem must answer one question:
> Does it help high-gain lead guitar remain clear, large, sustained, and expressive without turning fast playing into repeated-note clutter?
The result is not an MVP and not a simplified pedal. It is a focused rack-grade processor with a fast Performance page and substantial expert control underneath.
The existing Aural Maximizer Pro project gives us a useful architectural precedent: independent DSP classes, a routing engine, APVTS state, isolated GUI components, metering, real-time-safe processing, and native JUCE rendering.
---
# 1. Product definition
ShredSpace is a dual-voice stereo delay whose behavior changes continuously according to the guitarist's performance.
During dense or transient-heavy playing, it should:
* reduce conspicuous repeats;
* shorten the effective tail;
* soften repeated pick attacks;
* reduce harsh upper-mid accumulation;
* contract the wet stereo field;
* keep the dry guitar clear and centered.
During held notes, bends, phrase endings, and pauses, it should:
* raise the longer delay voice;
* extend decay;
* widen the wet image;
* allow more high-frequency openness;
* add controlled smoothness or diffusion;
* reveal the space that was present but restrained during the run.
The listener should hear a large, polished lead tone—not an obvious automation system.
## Product promise
> **Fast in front. Huge behind.**
## What it is not
It is not intended to replace:
* tape-delay specialists;
* ambient multi-tap workstations;
* reverb;
* granular delay;
* intelligent harmonizers;
* chorus, flanger, or phaser plugins;
* general-purpose modulation matrices;
* complex rhythmic delay sequencers.
Subtle modulation, diffusion, and special delay-time movement are included only where they directly improve lead guitar.
---
# 2. High-level architecture
```text
Audio Input
    │
    ├── Input Gain
    │
    ├── Dry Path ───────────────────────────────────────────┐
    │                                                       │
    ├── Performance Analysis                               │
    │       ├── Envelope and activity                      │
    │       ├── Transient detection                        │
    │       ├── Spectral onset detection                   │
    │       ├── Note-density estimate                      │
    │       ├── Sustain estimate                           │
    │       └── Phrase-gap detector                        │
    │                                                       │
    └── Delay Routing                                      │
            ├── Voice A: Definition                        │
            └── Voice B: Flight                            │
                    │                                      │
                    ├── Fractional delay                    │
                    ├── Feedback matrix                    │
                    ├── HPF / LPF                          │
                    ├── Presence Guard                     │
                    ├── Repeat transient softening         │
                    ├── Mild repeat character              │
                    ├── Modulation / drift                  │
                    └── Restricted diffusion               │
                            │                               │
                    Adaptive Voice Mix                      │
                            │                               │
                    Wet Stereo Processing                  │
                            │                               │
                    Global Dry/Wet Mix ◄────────────────────┘
                            │
                    Output Gain and Metering
                            │
                         Output
```
JUCE's `dsp::DelayLine` already supports fractional-delay interpolation, sample-by-sample reads, real-time modulation, and feedback-loop use. Its documentation explicitly recommends smoothing live delay changes, and maximum buffer allocation must be done outside the audio thread. (https://docs.juce.com/master/classjuce_1_1dsp_1_1DelayLine.html)
---
# 3. User interface structure
The plugin should have four principal views.
## Performance
The everyday page:
```text
TIME
SPACE
DEFINITION
BLOOM
TAIL
FOCUS
WIDTH
CHARACTER
```
Also visible:
* profile browser;
* four performance scenes;
* global mix;
* input and output;
* bypass;
* tap;
* hold;
* activity, density, and bloom indicators;
* stereo output meters.
## Voices
Detailed editing for:
* Voice A — Definition;
* Voice B — Flight;
* timing;
* levels;
* decay;
* stereo;
* filtering;
* repeat attack;
* motion;
* diffusion;
* adaptive role.
## Intelligence
Detailed behavior of:
* onset detection;
* density estimation;
* sustain recognition;
* phrase-gap behavior;
* active-phrase contraction;
* sustained-note bloom.
## Tone & Motion
Detailed control over:
* HPF and LPF;
* dynamic presence control;
* repeat transient softening;
* saturation;
* modulation;
* drift;
* restricted diffusion;
* stereo behavior.
This keeps the plugin extensive without presenting one undifferentiated wall of knobs.
---
# 4. Supported audio layouts
Recommended layouts:
```text
Mono input  → Mono output
Mono input  → Stereo output
Stereo input → Stereo output
```
Mono-to-stereo support is important because a mono lead source should be able to generate stereo delay width.
The dry signal remains centered and unaltered unless the user explicitly changes its routing. Stereo creation happens primarily in the wet path.
---
# 5. Global controls
These are proposed 1.0 ranges. Exact defaults should be tuned using real guitar material.
| Control       |                      Range |      Default | Function                                                 |
| ------------- | -------------------------: | -----------: | -------------------------------------------------------- |
| Bypass        |                   Off / On |          Off | Bypasses processing                                      |
| Bypass Mode   |              Hard / Trails |       Trails | Determines whether existing echoes continue after bypass |
| Input         |              −24 to +24 dB |         0 dB | Pre-analysis and pre-delay gain                          |
| Output        |              −24 to +24 dB |         0 dB | Final output gain                                        |
| Mix           |                     0–100% |          22% | Global dry/wet                                           |
| Mix Mode      |              Insert / Send |       Insert | Equal-power mix or wet-only output                       |
| Adaptive Mode | Off / Level / Articulation | Articulation | Static, traditional ducking, or full behavior analysis   |
| Routing       |       Parallel / A→B / B→A |     Parallel | Relationship between delay voices                        |
| Tap           |                  Momentary |            — | Learns delay time from taps                              |
| Hold          |         Momentary or Latch |            — | Freezes the current delay tail safely                    |
| Spillover     |                   Off / On |           On | Preserves tails during scene changes                     |
| Safety        |                   Off / On |           On | Prevents unstable feedback escalation                    |
## Input and output gain
For gain G_dB:

    g = 10^(G_dB / 20)

For channel c:

    x_0,c[n] = g_in * x_c[n]

Final output:

    y_c[n] = g_out * y_mix,c[n]

---
# 6. The eight performance controls
The eight controls are consistent macros. Profiles change their base mappings, but not their fundamental meanings.
## TIME
Controls the fundamental delay period while preserving voice ratios.
Modes:
```text
Milliseconds
Host Sync
Tap
```
Proposed range:
```text
20–2000 ms
```
The control displays milliseconds or the current rhythmic subdivision.
## SPACE
Controls how much processed ambience surrounds the dry guitar.
It coordinates:
* Voice A level;
* Voice B level;
* diffusion contribution;
* wet stereo presence;
* baseline bloom availability.
It does not replace global Mix. `Mix` controls total dry/wet balance; `Space` controls the internal size and prominence of the delay environment.
## DEFINITION
Controls how strongly the system contracts during active, dense playing.
It affects:
* wet-level reduction;
* tail reduction;
* repeat transient softening;
* presence suppression;
* stereo-width contraction.
At zero, the delay behaves conventionally. At maximum, the processor aggressively protects articulation.
## BLOOM
Controls how much the space expands during sustained notes and phrase endings.
It affects:
* Voice B level;
* tail extension;
* wet width;
* tone opening;
* restricted diffusion;
* bloom rise time.
## TAIL
Controls nominal decay time rather than a raw feedback percentage.
Proposed displayed range:
```text
0.15–12 seconds
```
Profiles may restrict the useful portion of this range.
## FOCUS
Moves repeats behind the dry guitar.
It coordinates:
* high-pass filtering;
* low-pass filtering;
* dynamic upper-mid suppression;
* repeat transient softening.
Low values are bright and forward. High values are smoother and more recessed.
## WIDTH
Controls wet-only stereo size.
It coordinates:
* voice pan;
* inter-channel delay offset;
* modulation phase difference;
* stereo width;
* cross-feedback;
* low-frequency mono behavior.
The dry center is preserved.
## CHARACTER
Controls repeat texture without pretending to emulate a particular vintage device.
It coordinates:
* mild saturation;
* modulation depth;
* slow drift;
* repeat diffusion;
* attack softness;
* slight bandwidth reduction.
The intended continuum is:
```text
Precise → Polished Rack → Smooth / Soft
```
It is not specifically a tape-age control.
---
# 7. Timing system
## Time modes
### Milliseconds
Direct delay setting:

    D = F_s * (T_ms / 1000)

where:
* D is delay in samples;
* F_s is sample rate;
* T_ms is delay in milliseconds.
### Host sync
Quarter-note duration:

    T_q = 60 / BPM

For subdivision multiplier r:

    T = r * T_q

Common multipliers:
| Division  |                r |
| --------- | ---------------: |
| Whole     |                4 |
| Half      |                2 |
| Quarter   |                1 |
| Eighth    |              0.5 |
| Sixteenth |             0.25 |
| Dotted    | base value × 1.5 |
| Triplet   | base value × 2/3 |
### Tap tempo
Store the latest valid tap intervals:

    Delta_i = t_i - t_(i-1)

Use the median rather than the mean:

    T_tap = median(Delta_1, ..., Delta_K)

The median rejects one accidental early or late tap more effectively than a simple average.
## Voice timing
Voice A:

    T_A = T_base * R_A + delta_A

Voice B:

    T_B = T_base * R_B + delta_B

where:
* R_j is the voice ratio;
* delta_j is a fine offset.
Proposed ratio range:
```text
0.25× to 4×
```
Common selectable values:
```text
1/4
1/3
1/2
2/3
3/4
1
4/3
3/2
2
3
4
Free
```
---
# 8. Fractional delay implementation
Delay times will rarely land on integer sample positions. Each voice therefore needs fractional interpolation.
JUCE provides no interpolation, linear interpolation, third-order Lagrange, and first-order Thiran choices. Third-order Lagrange is the preferred initial choice for the main delay because it offers higher fidelity than linear interpolation while remaining inexpensive enough for continuous modulation. (https://docs.juce.com/master/namespacejuce_1_1dsp_1_1DelayLineInterpolationTypes.html)
Linear interpolation is inexpensive but introduces a measurable low-pass effect, especially during modulation.
## Third-order Lagrange interpolation
Given fractional position mu in [0,1) between samples x[m] and x[m+1], use four surrounding samples:

    x_hat(m + mu) = L_(-1)*x[m-1] + L_0*x[m] + L_1*x[m+1] + L_2*x[m+2]

with:

    L_(-1) = -mu(mu-1)(mu-2)/6
    L_0    = (mu+1)(mu-1)(mu-2)/2
    L_1    = -(mu+1)mu(mu-2)/2
    L_2    = (mu+1)mu(mu-1)/6

The implementation should allocate the maximum delay buffers during `prepareToPlay()`, never while audio is running. JUCE notes that resizing a delay line may allocate internally and must not happen on the audio thread.
---
# 9. Delay-time changes
Two behaviors are useful.
## Smooth
Large delay-time changes use two read heads:

    r_0[n] = Read(D_old)
    r_1[n] = Read(D_new)

Equal-power crossfade:

    r[n] = cos(pi*lambda/2) * r_0[n] + sin(pi*lambda/2) * r_1[n]

where lambda ramps from 0 to 1.
This avoids the dramatic pitch sweep caused by moving one read head.
## Bend
Delay time is ramped continuously:

    D[n] = D_0 + lambda[n] * (D_1 - D_0)

For a continuous-time delay:

    y(t) = x(t - D(t))

The approximate instantaneous playback-rate factor is:

    p(t) = 1 - dD(t)/dt

Therefore:
* decreasing delay time raises pitch;
* increasing delay time lowers pitch.
This creates the intentional buffer-bend behavior useful for cadenzas and special effects.
## Glide
Proposed range:
```text
5–1000 ms
```
Small modulation always uses a continuously moving fractional read head. Large manual or automated changes use the selected Smooth or Bend mode.
---
# 10. Dual delay voices
## Voice A — Definition
Purpose:
* remain relatively close to the dry tone;
* add size during fast passages;
* retain rhythmic precision;
* use lower feedback and narrower width.
Suggested initial profile tendencies:
```text
Shorter delay
Lower level
Shorter decay
Higher focus
Less diffusion
Less modulation
Lower adaptive role
```
## Voice B — Flight
Purpose:
* provide width, sustain, and dramatic tails;
* remain restrained during dense playing;
* expand during held notes and phrase endings.
Suggested tendencies:
```text
Longer delay
Wider stereo image
Longer decay
More modulation
More diffusion
Higher adaptive role
```
---
# 11. Per-voice settings
Each voice has the following expert controls.
## Core
| Setting       |         Range |
| ------------- | ------------: |
| Enable        |      Off / On |
| Time Ratio    |      0.25×–4× |
| Fine Offset   | −50 to +50 ms |
| Level         |   −60 to 0 dB |
| Tail Scale    |      0.25×–4× |
| Adaptive Role |        0–100% |
`Adaptive Role` determines how strongly the global Definition and Bloom engines affect the voice.
Voice A defaults lower. Voice B defaults higher.
## Stereo
| Setting          |        Range |
| ---------------- | -----------: |
| Pan              | −100 to +100 |
| Width            |       0–200% |
| Stereo Offset    |      0–30 ms |
| Cross Feedback   |       0–100% |
| Modulation Phase |       0–180° |
## Repeat tone
| Setting            |      Range |
| ------------------ | ---------: |
| High-Pass          | 40–1500 Hz |
| Low-Pass           |   2–20 kHz |
| Presence Frequency |  1.5–6 kHz |
| Presence Guard     |    0–12 dB |
| Attack Softening   |     0–100% |
## Motion
| Setting          |     Range |
| ---------------- | --------: |
| Modulation Rate  | 0.05–5 Hz |
| Modulation Depth |    0–8 ms |
| Drift            |    0–100% |
| Diffusion        |     0–40% |
| Saturation       |    0–100% |
The restricted diffusion ceiling prevents the plugin from becoming a substitute reverb.
---
# 12. Fundamental delay and feedback equations
For voice j and channel c, let r_j,c[n] be the delayed sample read from the circular buffer.
After repeat processing:

    z_j,c[n] = P_j{ r_j,c[n] }

where P_j contains filtering, dynamic presence control, transient softening, saturation, modulation-related processing, and diffusion.
The buffer write is:

    w_j,c[n] = u_j,c[n] + f_j,c[n]

where u_j is the voice input and f_j is feedback.
The classic scalar feedback-delay equation is:

    y[n] = x[n] + g * y[n - M]

which creates regularly spaced, exponentially decaying echoes. (https://www.dsprelated.com/freebooks/pasp/Feedback_Comb_Filters.html)
---
# 13. Decay-time-based feedback
Raw feedback percentage is not ideal because the audible decay changes when the delay time changes.
Instead, expose or derive a nominal T60: the time required for repeat amplitude to fall by 60 dB.
After k repeats:

    A_k = g^k

At T60:

    g^(T60/T) = 10^(-3)

Therefore:

    g = 10^(-3T / T60)

where:
* T is the delay period in seconds;
* T60 is desired decay time;
* g is feedback gain.
This means that changing delay time does not unexpectedly alter the overall tail duration.
Because filters and saturation occur inside the feedback loop, the actual decay is frequency-dependent. The formula is the nominal low-level broadband decay.
## Tail scale per voice

    T60_j = T60_global * S_j

where S_j is the voice tail-scale control.
---
# 14. Stereo cross-feedback
For each voice, let z = [z_L, z_R]^T.
Use:

    f = g * [ 1-q  q ; q  1-q ] * z

where q in [0,1].
Behavior:
* q = 0: independent left/right feedback;
* q = 0.5: each side receives the stereo average;
* q = 1: full channel exchange, producing ping-pong behavior.
The matrix eigenvalues are:

    lambda_1 = g
    lambda_2 = g(1 - 2q)

Therefore, for 0 <= q <= 1, the feedback remains linearly stable when:

    |g| < 1

The production implementation should cap ordinary feedback below approximately unity and add an independent safety mechanism.
---
# 15. Voice routing
## Parallel

    u_A = x_0
    u_B = x_0
    w = g_A * z_A + g_B * z_B

## A into B
Voice A receives the input normally.

    u_A = x_0

Voice B input crossfades between dry input and Voice A:

    u_B = cos(pi*s/2) * x_0 + sin(pi*s/2) * z_A

where s is the cascade amount.
## B into A
The same relationship is reversed.
The design intentionally avoids arbitrary bidirectional voice feedback. That would add substantial stability, UI, and testing problems without being central to shred delay.
---
# 16. Wet voice summing
Let the linear voice levels be l_A and l_B.
A useful energy normalization is:

    N = sqrt(max(1, l_A^2 + l_B^2))
    w[n] = (l_A * z_A[n] + l_B * z_B[n]) / N

This reduces large level jumps when both voices are raised simultaneously. It does not guarantee peak protection for highly correlated signals, so output headroom remains necessary.
---
# 17. Repeat filtering
The repeat chain contains:
1. high-pass filter;
2. low-pass filter;
3. dynamic presence guard;
4. repeat transient softener;
5. optional saturation;
6. restricted diffusion.
The filters are inside the feedback loop so successive repeats become progressively more controlled.
## Biquad equation
For input x[n] and output y[n]:

    y[n] = b_0*x[n] + b_1*x[n-1] + b_2*x[n-2] - a_1*y[n-1] - a_2*y[n-2]

This is equivalent to:

    H(z) = (b_0 + b_1*z^-1 + b_2*z^-2) / (1 + a_1*z^-1 + a_2*z^-2)

The RBJ Audio EQ Cookbook is an appropriate coefficient reference for the HPF, LPF, and peaking filters. (https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html)
## Shared definitions

    omega_0 = 2*pi*f_0/F_s
    alpha = sin(omega_0) / (2*Q)

All coefficients are normalized by a_0.
## Low-pass coefficients

    b_0 = (1 - cos(omega_0)) / 2
    b_1 = 1 - cos(omega_0)
    b_2 = (1 - cos(omega_0)) / 2
    a_0 = 1 + alpha
    a_1 = -2*cos(omega_0)
    a_2 = 1 - alpha

## High-pass coefficients

    b_0 = (1 + cos(omega_0)) / 2
    b_1 = -(1 + cos(omega_0))
    b_2 = (1 + cos(omega_0)) / 2
    a_0 = 1 + alpha
    a_1 = -2*cos(omega_0)
    a_2 = 1 - alpha

## Peaking EQ coefficients
For gain G_dB:

    A = 10^(G_dB / 40)
    b_0 = 1 + alpha*A
    b_1 = -2*cos(omega_0)
    b_2 = 1 - alpha*A
    a_0 = 1 + alpha/A
    a_1 = -2*cos(omega_0)
    a_2 = 1 - alpha/A

Filter frequencies and gains must be smoothed. Coefficients should be recalculated at a control rate rather than for every sample.
---
# 18. Dynamic Presence Guard
High-gain repeated notes often accumulate energy in the upper mids. The Presence Guard dynamically reduces this region only when the delayed signal becomes excessive.
## Band detector
Extract the presence band:

    p[n] = BPF{ z[n] }

Calculate an RMS envelope:

    e_p^2[n] = alpha * e_p^2[n-1] + (1 - alpha) * p^2[n]
    e_p[n] = sqrt(e_p^2[n] + epsilon)

Convert to dB:

    L_p[n] = 20*log10(e_p[n] + epsilon)

Normalize excess above threshold theta:

    u[n] = clamp((L_p[n] - theta) / W, 0, 1)

Use a smoothstep curve:

    s(u) = 3u^2 - 2u^3

Dynamic cut:

    G_p[n] = -G_max * s(u[n])

This gain drives a negative peaking filter around the selected presence frequency.
`FOCUS`, `DEFINITION`, and the voice's adaptive role determine G_max.
---
# 19. Repeat transient softening
This is not a normal full-band compressor. Its purpose is to reduce repeated pick attacks while preserving the body of the echo.
Split the delayed repeat at a configurable or profile-defined crossover:

    z[n] = z_low[n] + z_high[n]

Track fast and slow high-band envelopes:

    e_f[n] = alpha_f * e_f[n-1] + (1 - alpha_f) * |z_high[n]|
    e_s[n] = alpha_s * e_s[n-1] + (1 - alpha_s) * |z_high[n]|

where:

    alpha = exp(-1 / (tau * F_s))

Transient excess:

    t[n] = max(0, (e_f[n] - e_s[n]) / (e_s[n] + epsilon))

Transient attenuation:

    G_t[n] = -A_t * clamp(t[n], 0, 1)
    g_t[n] = 10^(G_t[n] / 20)

Output:

    z_soft[n] = z_low[n] + g_t[n] * z_high[n]

This leaves low-frequency body substantially intact while pushing repeated pick attack behind the dry note.
---
# 20. Mild repeat character
Character should remain subtle and controllable.
A useful soft-saturation function with unity small-signal slope is:

    s_D(x) = tanh(D*x) / D

because:

    d/dx s_D(x) at x=0 equals 1

Blend amount c:

    y = (1 - c)*x + c*s_D(x)

This provides soft compression without increasing small-signal loop gain.
## Aliasing decision
Saturation is not the product's primary effect. It should remain mild and occur after repeat low-pass filtering.
Two implementation choices should be prototyped:
1. base-rate processing with conservative drive;
2. optional 2× oversampling in Studio quality.
JUCE oversampling supports 2× and higher factors, FIR or IIR stages, and latency measurement. Oversampling reduces nonlinear aliasing but increases processing cost and introduces latency that must be accounted for. (https://docs.juce.com/master/classjuce_1_1dsp_1_1Oversampling.html)
Do not commit to oversampling until profiling shows that the audible benefit justifies feedback-loop and delay-time compensation complexity.
---
# 21. Modulation and drift
Modulation is for polished rack movement, not for replacing a chorus.
## Periodic modulation
For voice j, channel c:

    m_j,c[n] = A_j * sin(2*pi*f_j*n/F_s + phi_j,c)

Delay in samples:

    D_j,c[n] = F_s * (T_j + delta_j,c + m_j,c[n])

The right-channel phase can be offset:

    phi_R = phi_L + Delta_phi

## Random drift
Generate white noise xi[n] in [-1,1] and low-pass it:

    r[n] = alpha_r * r[n-1] + (1 - alpha_r) * xi[n]

Then:

    m[n] = m_LFO[n] + A_r * r[n]

The drift bandwidth should remain very low so it feels like slow rack instability rather than audible random pitch modulation.
---
# 22. Restricted diffusion
Diffusion should soften individual repeats, not create a full reverb tail.
Use a short cascade of first-order all-pass delay stages:

    H(z) = (-a + z^-M) / (1 - a*z^-M)

Difference equation:

    y[n] = -a*x[n] + x[n-M] + a*y[n-M]

For |a| < 1, the ideal magnitude response is unity while phase and temporal distribution change.
Use:
* two or three stages;
* very short, unequal delays;
* conservative feedback coefficients;
* wet amount capped around 40%.
Diffused repeat:

    z_diff = cos(pi*d/2)*z + sin(pi*d/2)*A{z}

where A{.} is the all-pass cascade.
No early-reflection system, late reverb network, room-size model, or damping matrix belongs here.
---
# 23. Stereo processing
## Constant-power pan
For pan p in [-1,1]:

    g_L = cos(pi*(p+1)/4)
    g_R = sin(pi*(p+1)/4)

## Mid/side width
Convert:

    M = (L + R) / sqrt(2)
    S = (L - R) / sqrt(2)

Apply width w:

    L' = (M + w*S) / sqrt(2)
    R' = (M - w*S) / sqrt(2)

Interpretation:
* w = 0: mono;
* w = 1: original width;
* w > 1: expanded width.
The plugin should cap and meter extreme negative correlation.
## Low-frequency mono
Split wet output at f_m:

    L = L_low + L_high
    R = R_low + R_high

Mono low component:

    M_low = (L_low + R_low) / 2

Output:

    L' = M_low + L_high
    R' = M_low + R_high

This preserves width in the useful lead frequencies while preventing unnecessarily unstable low-frequency stereo tails.
---
# 24. Performance analyzer
The analyzer should use continuous measurements rather than claiming perfect classification of alternate picking, legato, bends, or vibrato.
It estimates:
```text
Activity
Transient strength
Spectral novelty
Onset events
Event density
Sustain confidence
Phrase gaps
```
Onset detection research shows why a single envelope is insufficient: onsets must be distinguished from offsets, vibrato, amplitude modulation, and noise. Magnitude-based spectral flux, phase-based methods, and complex-domain methods are established approaches. (https://www.dafx.de/paper-archive/2006/papers/p_133.pdf)
For this plugin, a fused time-domain and magnitude-flux detector is the best balance of specialization, CPU use, and real-time determinism.
## Analysis signal
For stereo input, avoid simple polarity-sensitive downmixing for envelope measurements.
Instantaneous energy:

    E[n] = sqrt((x_L^2[n] + x_R^2[n]) / 2 + epsilon)

Use a band-limited analysis path, approximately covering the musically useful guitar range, so low-frequency rumble and high-frequency hiss do not dominate detection.
---
# 25. Fast and slow envelopes

    e_f[n] = alpha_f * e_f[n-1] + (1 - alpha_f) * E[n]
    e_s[n] = alpha_s * e_s[n-1] + (1 - alpha_s) * E[n]

with:

    alpha_f = exp(-1 / (tau_f * F_s))
    alpha_s = exp(-1 / (tau_s * F_s))

Typical starting regions:
```text
Fast envelope: 1–5 ms
Slow envelope: 30–100 ms
```
Time-domain novelty:

    N_t[n] = max(0, 20*log10((e_f[n] + epsilon) / (e_s[n] + epsilon)))

---
# 26. Spectral-flux onset feature
Use a causal STFT:

    X_m[k] = sum_{n=0}^{N-1} x_a[mH + n] * w[n] * e^(-j*2*pi*k*n/N)

Hann window:

    w[n] = (1/2) * (1 - cos(2*pi*n/(N-1)))

Recommended starting values:
```text
FFT size: 512
Hop: 256 samples
Analysis band: approximately 150 Hz–8 kHz
```
Positive log-magnitude spectral flux:

    SF[m] = (1/K) * sum_{k=k0}^{k1} w_k * max(0, log(|X_m[k]| + epsilon) - log(|X_{m-1}[k]| + epsilon))

Positive-only changes are used because rising spectral energy is more relevant to note beginnings than falling energy.
This control-rate FFT does not delay the dry signal. Its few milliseconds of detection latency are acceptable because it is controlling echoes that generally occur much later.
---
# 27. Adaptive feature normalization
Each detector feature v_i should adapt to the player's input level and tone.
Running mean:

    mu_i[m] = alpha_mu * mu_i[m-1] + (1 - alpha_mu) * v_i[m]

Running absolute deviation:

    q_i[m] = alpha_q * q_i[m-1] + (1 - alpha_q) * |v_i[m] - mu_i[m]|

Normalized positive deviation:

    z_i[m] = max(0, (v_i[m] - mu_i[m]) / (q_i[m] + epsilon))

Combined onset score:

    O[m] = sigma(w_t*z_t[m] + w_f*z_f[m] + w_e*z_e[m] - theta)

where:

    sigma(x) = 1 / (1 + e^(-x))

Use:
* threshold crossing;
* local-maximum selection;
* a short refractory interval;
* noise-gate qualification.
This reduces duplicate detections from one pick attack.
---
# 28. Note-density estimate
Keep onset times in a fixed-size real-time-safe ring buffer.
For analysis window W_d:

    r(t) = N_onsets(t - W_d, t) / W_d

where r is events per second.
Normalize:

    u = clamp((r - r_low) / (r_high - r_low), 0, 1)
    d = 3u^2 - 2u^3

Suggested starting calibration:
```text
Low density: 2–3 detected events/second
High density: 10–14 detected events/second
Window: 300–800 ms
```
These are tuning values, not universal musical truths.
---
# 29. Sustain confidence
A held note generally has:
* input activity;
* limited recent onset density;
* limited transient novelty;
* sufficient time since the last strong event.
Activity gate:

    a = smoothstep((L - theta_g) / W_g)

Time-since-event ramp:

    s_t = smoothstep((t - t_last - T_0) / (T_1 - T_0))

Sustain estimate:

    s = a * s_t * (1 - d) * (1 - O_tilde)

where O_tilde is a smoothed onset activity measure.
This should be described internally as **sustain confidence**, not a guarantee that the system understands the player's exact technique.
---
# 30. Phrase-gap detector
A phrase gap occurs when:
* the signal was recently active;
* the envelope drops below a gate;
* no qualifying onset occurs for the selected gap interval.
Let g_raw trigger when those conditions are met.
Use an attack/hold/release envelope:

    g[n] = attack toward 1 when triggered
           1 during hold
           release toward 0 otherwise

This does not generate new sound. It simply releases contraction and allows existing delay energy to become more audible.
---
# 31. Contraction and bloom control signals
## Active-phrase contraction

    C_target = clamp(w_d*d + w_t*O_tilde, 0, 1)

Apply separate attack and release smoothing.
## Bloom

    B_target = clamp(w_s*s + w_g*g, 0, 1) * (1 - C)

The (1 - C) term prevents maximum bloom and maximum contraction from fighting at the same time.
`DEFINITION` scales C.
`BLOOM` scales B.
---
# 32. Adaptive destination mapping
For voice j, let rho_j in [0,1] be Adaptive Role.
## Wet level

    L_j,eff = L_j,base - rho_j*C*A_j,wet + rho_j*B*B_j,wet

All level terms are in dB.
Voice B should normally have larger adaptive amounts.
## Tail
A useful multiplicative mapping is:

    T60_j,eff = T60_j,base * 2^(-rho_j*C*A_j,tail + rho_j*B*B_j,tail)

Interpretation:
* A_tail = 1: full contraction halves the decay time;
* B_tail = 1: full bloom doubles it.
Then calculate feedback:

    g_j = 10^(-3*T_j / T60_j,eff)

## Low-pass movement

    f_LP,eff = f_LP,base * 2^(-rho_j*C*A_j,tone + rho_j*B*B_j,tone)

Dense playing darkens the repeats; bloom may reopen them.
## Width

    W_j,eff = clamp(W_j,base - rho_j*C*A_j,width + rho_j*B*B_j,width, 0, 2)

## Transient softening

    S_j,eff = clamp(S_j,base + rho_j*C*A_j,soft, 0, 1)

These mappings produce coordinated behavior without hard-switching between supposed playing styles.
---
# 33. Intelligence settings
## Detector
| Setting        | Proposed range |
| -------------- | -------------: |
| Sensitivity    |         0–100% |
| Input Gate     |  −80 to −20 dB |
| Density Window |    200–1000 ms |
| Density Start  |   1–8 events/s |
| Density Full   |  6–20 events/s |
| Sustain Start  |      50–800 ms |
| Sustain Full   |    150–2000 ms |
| Phrase Gap     |      20–500 ms |
## Definition response
| Setting                   | Proposed range |
| ------------------------- | -------------: |
| Contraction Attack        |       1–100 ms |
| Contraction Release       |     20–1000 ms |
| Wet Reduction             |        0–18 dB |
| Tail Reduction            |    0–2 octaves |
| Tone Darkening            |    0–2 octaves |
| Width Reduction           |         0–100% |
| Attack Softening Increase |         0–100% |
## Bloom response
| Setting            | Proposed range |
| ------------------ | -------------: |
| Bloom Rise         |     20–1500 ms |
| Bloom Hold         |      0–2000 ms |
| Bloom Release      |     50–3000 ms |
| Wet Lift           |        0–12 dB |
| Tail Expansion     |    0–2 octaves |
| Tone Opening       |     0–1 octave |
| Width Expansion    |         0–100% |
| Diffusion Increase |          0–30% |
Profiles provide good defaults. Users do not need to edit these to use the plugin successfully.
---
# 34. Global dry/wet mixing
## Insert mode
Use equal-power mixing:

    g_d = cos(pi*m/2)
    g_w = sin(pi*m/2)
    y = g_d*x_dry + g_w*w_wet

where m in [0,1].
## Send mode

    y = w_wet

The dry path is omitted.
JUCE's `DryWetMixer` supports wet-path latency compensation and configurable mix ramps if needed, although a custom mixer may be preferable because this product has trails and special bypass behavior. (https://docs.juce.com/master/classjuce_1_1dsp_1_1DryWetMixer.html)
---
# 35. Bypass and trails
## Hard bypass
Fade the wet path out and return dry input.
## Trails bypass
Stop feeding new input into the delay:

    u_j[n] = (1 - b[n]) * x[n]

while allowing existing feedback to decay.
Dry output remains active.
Here b[n] is a smoothed bypass state.
A click-free bypass needs a short ramp rather than an instantaneous switch.
---
# 36. Hold
Hold should preserve the current tail without allowing uncontrolled escalation.
When active:

    u_j[n] = 0

Set a very long effective decay:

    T60_hold >> T60_normal

or:

    g_hold ~ 1

but always below the stability ceiling.
The feedback safety processor remains active.
The Hold state should not blindly set feedback above unity.
---
# 37. Feedback safety
The safety system should not function as an audible everyday limiter.
Track feedback-path RMS:

    e_f[n] = sqrt(alpha*e_f^2[n-1] + (1-alpha)*z^2[n] + epsilon)

When it exceeds a safety threshold:

    r[n] = min(1, E_safe / (e_f[n] + epsilon))

Apply smoothed reduction:

    g_safe[n] = g_requested * r[n]

Also:
* reject NaN and infinity;
* flush denormals;
* cap maximum feedback;
* reset corrupted state safely;
* expose a visible safety indicator if intervention occurs.
---
# 38. Profiles
Profiles are complete system configurations, not static artist simulations.
Recommended initial factory profiles:
| Profile             | Purpose                                               |
| ------------------- | ----------------------------------------------------- |
| Precision Rack      | Clear, polished, restrained dual delay                |
| Rapid Definition    | Maximum separation for dense alternate picking        |
| Neoclassical Flight | Singing lead with controlled sustained-note expansion |
| Wide Arpeggio       | Separation across sweeps and arpeggios                |
| Singing Lead        | Longer melodic tail with moderate definition          |
| Dry Center Arena    | Strong center image with wide tails                   |
| Legato Space        | Smooth repeats for lower-transient phrases            |
| Dark High-Gain      | Strong upper-mid and pick-repeat control              |
| Ballad Bloom        | Large sustained-note growth                           |
| Controlled Cascade  | Serial delay movement without uncontrolled wash       |
| Mono Rig Polish     | Rack depth from a mono source without excessive width |
| Buffer Bend         | Intentional delay-time pitch movement and safe hold   |
## Development references
Internal listening targets exist in private development notes only. Public names must describe behavior rather than imply affiliation, endorsement, or exact reproduction. No artist or song name may appear in any shipped file or resource.
---
# 39. Performance scenes
Each profile can contain four macro scenes.
Suggested defaults:
```text
A — Tight
B — Open
C — Bloom
D — Special
```
Scenes store:
* eight performance macros;
* mix;
* hold behavior;
* possibly routing selection.
They should not duplicate every advanced parameter. The underlying profile remains the same.
## Scene morphing
For a continuous parameter:

    p[n] = (1 - lambda[n]) * p_A + lambda[n] * p_B

For frequency and time parameters, interpolate logarithmically:

    p[n] = p_A * (p_B / p_A)^lambda[n]

Delay-time changes still use the selected Smooth or Bend mechanism.
Discrete routing changes require a short wet-path crossfade.
---
# 40. Parameter smoothing
Every continuously automatable gain, frequency, decay, width, and modulation amount needs smoothing.
One-pole smoothing:

    y[n] = alpha*y[n-1] + (1 - alpha)*x[n]
    alpha = exp(-1 / (tau * F_s))

JUCE's `SmoothedValue` supports linear and multiplicative smoothing. (https://docs.juce.com/master/classjuce_1_1SmoothedValue.html)

Smoothing domain rules (binding):
* Linear smoothing for values that can be zero, negative, or signed: dB values, mix, pan, width, signed offsets, macro amounts, cross-feedback, and normalized 0-1 controls. Smooth in dB where the value is a level, then convert: g[n] = 10^(G_dB[n]/20).
* Multiplicative/logarithmic smoothing only for strictly positive quantities: filter frequency, T60, LFO rate, and delay time when not handled by the read-head crossfade.
* Dedicated transition mechanisms, not SmoothedValue, for: large delay-time changes (Smooth/Bend engine), routing changes (wet-path crossfade), profile changes (morph engine), and discrete mode switches (short topology crossfade).

Recommended behavior:
```text
Gain and mix: 10–30 ms
Filter frequency: 20–100 ms
Feedback / tail: 20–100 ms
Width: 20–100 ms
Adaptive controls: separate musical attack and release
```
---
# 41. State and parameter architecture
All automatable parameters should receive stable IDs before implementation spreads across phases, and IDs are frozen only after the parameter contract document is complete and reviewed.
JUCE's `AudioProcessorValueTreeState` provides parameter ownership and a `ValueTree` representing processor state, with each parameter identified by a string ID. (https://docs.juce.com/master/classjuce_1_1AudioProcessorValueTreeState.html)
Suggested parameter groups:
```text
GLOBAL_*
PERF_*
TIME_*
VOICE_A_*
VOICE_B_*
INTEL_*
STEREO_*
SCENE_*
```
Public host-automatable parameters, deliberately selected expert parameters, and internal/profile state are three distinct surfaces. Not every internal setting appears as a DAW parameter. Detector normalization history, profile metadata, macro destination tables, scene names, migration metadata, tags, safety intervention state, and runtime analysis values are internal state, never APVTS parameters.

Macro/scene/adaptive layering model (binding): moving a macro, scene morph, or adaptive engine never rewrites base APVTS parameter values. They contribute deltas combined into an internal effective value:

    linear domain:  p_eff = clamp(p_base + dp_macro + dp_adaptive + dp_scene, p_min, p_max)
    log domain:     p_eff = clamp(p_base * 2^(d_macro + d_adaptive + d_scene), p_min, p_max)
    gain domain:    G_eff_dB = clamp(G_base_dB + dG_macro_dB + dG_adaptive_dB, G_min, G_max)

Profile files should contain:
```text
schema_version
product_version
profile_name
author
description
parameter_values
macro_mapping
scene_values
tags
```
Preset migration must be designed before the first public release.
---
# 42. Proposed source architecture
```text
Source/
├── PluginProcessor.*
├── PluginEditor.*
├── Parameters/
│   ├── ParameterIDs.h
│   ├── ParameterDescriptors.h
│   ├── Descriptors*.cpp
│   ├── ParameterLayout.*
│   ├── ParameterSnapshot.*
│   └── MacroEngine.*
├── DSP/
│   ├── DelayVoice.*
│   ├── FractionalDelay.*
│   ├── DelayTimeController.*
│   ├── FeedbackController.*
│   ├── RepeatToneProcessor.*
│   ├── PresenceGuard.*
│   ├── RepeatTransientSoftener.*
│   ├── ModulationEngine.*
│   ├── SoftSaturator.*
│   ├── ShortDiffuser.*
│   ├── StereoProcessor.*
│   ├── RoutingEngine.*
│   └── SafetyProcessor.*
├── Analysis/
│   ├── AnalysisResampler.*
│   ├── PerformanceAnalyzer.*
│   ├── OnsetDetector.*
│   ├── DensityEstimator.*
│   ├── SustainEstimator.*
│   ├── AdaptiveController.*
│   └── AdaptiveSnapshot.h
├── Telemetry/
│   └── TelemetryState.*
├── Profiles/
│   ├── ProfileManager.*
│   ├── ProfileSchema.*
│   ├── ProfileSnapshot.*
│   └── SceneManager.*
└── GUI/
    ├── CustomLookAndFeel.*
    ├── PerformancePage.*
    ├── VoicesPage.*
    ├── IntelligencePage.*
    ├── ToneMotionPage.*
    ├── DetectorDisplay.*
    └── MeterComponent.*
```
The DSP, profile system, parameter system, and GUI must remain independent. That allows later visual redesign without rewriting the processor.

Host timing acquisition is owned by the processor, which reads the playhead once per block into a small POD snapshot (bpm, bpmValid, isPlaying, samplePosition) and passes it to DelayTimeController. DelayVoice never queries the host.
---
# 43. Real-time requirements
Inside `processBlock()`:
* no heap allocation;
* no buffer resizing;
* no locks;
* no file operations;
* no profile parsing;
* no GUI calls;
* no unbounded loops;
* no string construction;
* no coefficient object allocation;
* no delay-buffer reallocation.
Prepare in advance:
* maximum delay buffers;
* FFT storage;
* analysis windows;
* ring buffers;
* allpass buffers;
* scratch buffers;
* filter states;
* profile transition storage.
Profile loading and disk access happen off the audio thread. Parameter snapshots are transferred to the processor via fixed-size POD double buffers with atomic index/generation publication (acquire/release), never via atomic shared_ptr exchange, because releasing the final reference could deallocate on the audio thread. Strings, tags, and filenames never enter audio-thread snapshots.

Telemetry rules: scalar latest-value telemetry (peaks, RMS, activity, density, sustain, contraction, bloom, safety flag, correlation) travels through atomics. Time-series history for visualization (detector history traces) travels through juce::AbstractFifo. FIFO overflow is acceptable for history, never for authoritative meter state.

Safety reset rules: on invalid-sample detection, mute the affected wet path for the block, reset lightweight filter/envelope states immediately, clear large delay buffers only at a bounded block boundary or incrementally, and publish the safety intervention flag atomically. Never clear megabytes of buffer unpredictably mid-block.
---
# 44. Expected CPU profile
The focused design should remain substantially lighter than a general delay workstation.
The principal costs are:
* four fractional delay reads for two stereo voices;
* feedback filtering;
* dynamic EQ;
* transient envelopes;
* two short all-pass cascades;
* low-rate modulation;
* one 512-point analysis FFT every 256 samples at the fixed internal analysis rate;
* parameter smoothing;
* meters.
Explicitly excluded expensive systems:
* convolution;
* full reverb;
* neural inference;
* granular processing;
* 16 independent taps;
* full pitch shifting;
* arbitrary modulation matrix;
* intelligent harmonization.
## Delay memory
For two voices, two channels, maximum delay T_max, floating-point samples:

    M = 2 * 2 * F_s * T_max * 4 bytes

At 192 kHz and 2 seconds:

    M = 4 * 192000 * 2 * 4 = 6,144,000 bytes

Approximately 6.1 MB, excluding guard samples and overhead. That is acceptable.
---
# 45. Quality strategy
One excellent primary processing path rather than many quality levels that subtly change the sound.
Initial quality choices:
* third-order Lagrange delay interpolation;
* double-precision coefficient calculations;
* floating-point audio processing;
* control-rate filter updates;
* fixed-rate analysis engine (see below);
* no lookahead;
* optional mild saturation;
* zero reported latency unless oversampling requires otherwise.
A Live/Studio switch should only be added if measured CPU or aliasing results demonstrate a real benefit.

## Fixed-rate analysis (binding)
The analyzer runs at a fixed internal analysis rate of 48 kHz regardless of host sample rate: input passes through an inexpensive anti-alias low-pass and integer/rational downsampling into the fixed-rate analysis engine. At 48 kHz the 512-sample window is 10.67 ms, the 256-sample hop is 5.33 ms, and bin spacing is 93.75 Hz. This makes spectral resolution, temporal context, transient behavior, and density detection consistent at 44.1, 48, 88.2, 96, and 192 kHz. Do not scale FFT/hop with host sample rate.
---
# 46. Testing specification
## Deterministic DSP tests
### Delay timing
Feed an impulse and verify first-repeat location:

    n_expected = round(F_s * T)

Test:
```text
44.1 kHz
48 kHz
88.2 kHz
96 kHz
192 kHz
```
### Fractional delay
Test noninteger delay positions against calculated interpolation results.
### Decay
Measure repeat amplitudes and verify:

    20*log10(A(T60)/A(0)) ~ -60 dB

within tolerance.
### Filters
Measure frequency response against expected HPF, LPF, and peaking curves.
### Cross-feedback
Verify channel movement and matrix stability at:
```text
q = 0
q = 0.5
q = 1
```
### Modulation
Verify:
* no discontinuities;
* bounded delay values;
* correct LFO rate;
* correct stereo phase.
### Diffusion
Verify bounded output and near-unity all-pass magnitude response.
### Safety
Feed:
* impulses;
* full-scale tones;
* silence;
* denormally small values;
* NaN injection in unit tests;
* maximum feedback.
Confirm no runaway, hang, or invalid output.
### Adaptive end-to-end
Identical input audio must produce measurably different output when:
```text
Definition = 0 versus 100%
Bloom = 0 versus 100%
Adaptive Mode = Off versus Articulation
dense impulse train versus sustained signal
```
### State
Set parameter values, serialize, construct/reset the processor, restore, and verify exact parameter and profile state round-trip.
## Analyzer tests
Use synthetic signals:
* isolated impulses;
* 2, 4, 8, 12, 16, and 20 events/second;
* sustained sine waves;
* vibrato;
* amplitude tremolo;
* legato-like frequency changes;
* noise bursts;
* distorted guitar-like test signals.
Verify:
* density increases monotonically with event rate;
* held tones develop sustain confidence;
* silence triggers phrase gap only after prior activity;
* vibrato does not continuously retrigger onsets;
* noise below the gate does not trigger bloom;
* analyzer behavior is consistent across host sample rates.
## Real guitar test corpus
Include clean DI and post-amp signals for:
* alternate picking;
* economy picking;
* sweeps;
* legato;
* tapped runs;
* bends;
* wide vibrato;
* chords;
* palm-muted rhythms;
* sustained feedback;
* natural string noise;
* noise-gated silence.
The plugin must be evaluated principally after high-gain amp or NAM processing, because that is where transient compression and upper-mid accumulation are most difficult.
## Host testing
Test:
```text
REAPER
Studio One
Bitwig
Standalone
```
Across:
```text
VST3
LV2
Windows (later, as with Aural Maximizer Pro)
Linux (initial target)
```
And block sizes:
```text
32
64
128
256
512
1024
2048
```
---
# 47. Major concerns and potential snags
## 1. Distortion flattens transients
High-gain processing compresses note attacks. A detector based only on amplitude may miss rapid notes.
**Response:** fuse envelope novelty with spectral flux and adaptive thresholds.
## 2. Vibrato and bends can resemble new events
Pitch movement changes the spectrum and can create false onsets.
**Response:** use event refractory periods, require fused evidence, and treat the system as continuous confidence rather than hard note classification.
## 3. Legato is difficult to distinguish from sustain
Smooth legato may contain weak transients but substantial spectral movement.
**Response:** spectral novelty must contribute to activity and density even when time-domain transient strength is low.
## 4. Input-level dependence
Players, pickups, amp models, and gain settings vary dramatically.
**Response:** adaptive normalization, input gate, sensitivity, useful meters, and profiles calibrated over diverse sources.
## 5. Overreaction will feel like pumping
Too much wet, tail, tone, and width movement can make the effect conspicuously automated.
**Response:** conservative defaults, different attack/release times for each destination, profile tuning, and a global Adaptation mode.
## 6. Users need predictability
An "intelligent" effect that behaves inconsistently will be abandoned.
**Response:** adaptation can be disabled; the underlying static dual delay must sound excellent on its own; the GUI must show density, contraction, and bloom activity.
## 7. Delay-time automation can click or bend pitch
Naive smoothing can produce either zipper noise or unwanted pitch movement.
**Response:** separate Smooth dual-read-head transitions from intentional Bend behavior.
## 8. Feedback stability is more complicated after processing
Filtering, diffusion, cross-feedback, and nonlinear processing all affect loop behavior.
**Response:** derive feedback from decay time, cap loop gain, keep saturation unity-slope near zero, monitor feedback energy, and test the full loop rather than individual modules alone.
## 9. Dynamic filter updates may zipper
Rapidly changing peaking and cutoff filters can create artifacts.
**Response:** smooth control values, update at a bounded control rate, and avoid direct arbitrary coefficient interpolation.
## 10. Stereo widening can create mono cancellation
Inter-channel offsets and modulation may sound large in stereo but weak in mono.
**Response:** correlation metering, low-frequency mono, sensible width limits, mono test suite, and profiles tuned for compatibility.
## 11. Cascade routing can produce excessive density
Feeding one delay into another may rapidly create too many repeats.
**Response:** normalize cascade sends, retain independent feedback loops, and avoid arbitrary bidirectional voice feedback.
## 12. Diffusion can turn into bad reverb
Too much all-pass smearing will destroy rhythmic precision.
**Response:** cap amount, use only short networks, and keep it subordinate to the delay voices.
## 13. Saturation may not justify oversampling cost
Oversampling the feedback path complicates latency and timing.
**Response:** prototype audibly and measure. Remove or constrain saturation rather than adding expensive technology for marketing purposes.
## 14. Hold and regeneration can run away
Feedback near unity can accumulate DC, noise, and resonant frequencies.
**Response:** input kill during Hold, DC blocking, loop safety, conservative feedback ceiling, and visible intervention.
## 15. Scene and profile changes can damage tails
Changing time, filters, or routing while buffers contain audio may click or create pitch jumps.
**Response:** parameter morphing, read-head crossfades, wet-path fades around discrete topology changes, and explicit spillover rules.
## 16. Host tempo can jump
DAW loops, tempo automation, or missing host information can change synchronized delay abruptly.
**Response:** validate host BPM, retain the last valid tempo, and apply Smooth or Bend transition logic.
## 17. Mono-to-stereo host support varies
Some hosts manage bus layouts differently.
**Response:** explicitly test mono-mono, mono-stereo, and stereo-stereo configurations in every target host. Rule: dry mono input remains center-equivalent in stereo; Voice A/B create width only through wet routing; unused output channels are initialized deterministically.
## 18. Profiles are the product as much as the DSP
A technically excellent adaptive engine can still disappoint if factory profiles are poorly tuned.
**Response:** build the reference test corpus early and tune profiles while developing the engine—not after implementation is "finished." Shipped profiles are labeled development profiles until the real-guitar tuning phase completes.
## 19. Too many advanced controls can obscure the central idea
The Intelligence page could become intimidating.
**Response:** the eight macros must remain sufficient for daily use, and advanced settings must be grouped by musical result rather than DSP terminology alone.
## 20. Parameter IDs must be stable
Changing IDs after users create projects breaks automation and recall.
**Response:** finalize parameter taxonomy in the parameter contract before broad implementation, add schema versioning, and test preset migration.
## 21. Public artist references create unnecessary risk
Even accurate inspiration can be interpreted as endorsement or emulation claims.
**Response:** keep song and artist names in private development notes; public profiles describe musical behavior.
## 22. Scope creep remains the largest strategic risk
Features such as full reverb, scale harmony, granular delay, 16 taps, tape modeling, and an arbitrary modulation matrix are individually tempting.
**Response:** require every feature to improve articulate high-gain lead guitar directly. Otherwise it becomes another product.
---
# 48. Recommended frozen 1.0 boundary
The complete initial product should contain:
* two stereo fractional-delay voices;
* milliseconds, sync, and tap timing;
* Smooth and Bend time changes;
* decay-time-based feedback;
* parallel and one-way cascade routing;
* stereo cross-feedback and ping-pong;
* guitar-focused HPF and LPF;
* dynamic upper-mid Presence Guard;
* repeat transient softening;
* restrained modulation and drift;
* restrained diffusion;
* mild repeat saturation if justified by testing;
* activity, onset, density, sustain, and phrase-gap analysis at a fixed internal analysis rate;
* adaptive wet, tail, tone, width, and attack behavior integrated into the audio path;
* eight performance macros layered as effective-value deltas;
* detailed Voices, Intelligence, and Tone/Motion pages;
* complete development profiles plus a real-guitar tuning pass before the factory label;
* four macro scenes;
* trails bypass;
* Hold;
* feedback safety;
* mono-mono, mono-stereo, and stereo-stereo operation;
* Standalone, VST3, and LV2 builds (Linux first; Windows later as with Aural Maximizer Pro);
* native JUCE GUI;
* meters and adaptation visualization;
* deterministic DSP, analyzer, adaptive end-to-end, and state tests.
Explicitly outside 1.0:
* full reverb;
* convolution;
* tape-machine emulation;
* scale-aware harmonization;
* full pitch shifting;
* granular processing;
* arbitrary modulation matrix;
* multi-tap rhythm sequencer;
* chorus/flanger/phaser replacements;
* artist-branded modes.
This is large enough to be a serious professional instrument, focused enough to remain unique and understandable, and technically realistic enough to implement without creating a single delay instance that consumes an unreasonable portion of the system.
