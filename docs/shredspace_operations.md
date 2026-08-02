# ShredSpace — Operations Manual

## Plugin Overview
ShredSpace is a performance-responsive dual-voice stereo delay designed specifically for articulate high-gain lead guitar. Using a real-time spectral onset and performance analyzer, it continuously adapts delay parameters based on note density and sustain. During fast, dense runs, it ducks repeat levels, darkens tone, and contracts the stereo image to prevent muddiness. During sustained notes and phrase pauses, it expands the tail, lifts output levels, opens high frequencies, and widens the stereo image to create a rich sense of space.

## Intended Use
ShredSpace is designed for high-gain lead guitar tracks to dynamically adapt delay tails to performance speed and style, ensuring fast runs remain clear while sustained notes are rich and spacious.

## Signal Flow
1. **Input Gain (`GLOBAL_INPUT_GAIN`)**: Scales incoming mono or stereo PCM audio.
2. **Dry Path Buffer**: Audio is copied to the dry buffer. Dry mono input is kept centered.
3. **Performance Analysis**: Audio is concurrently analyzed by the intelligence engine:
   - *Onset and Density*: Identifies spectral transitions and estimates note events per second.
   - *Sustain & Phrase Gap*: Tracks note duration and detects pauses.
   - *Adaptive Controller*: Calculates dynamic contraction (ducking) and bloom (expansion) coefficients.
4. **Delay Routing Matrix (`PERF_ROUTING`)**: Audio enters the routing engine for Voice A (Definition) and Voice B (Flight):
   - *Parallel*: Input split into Voice A and Voice B independently.
   - *A to B*: Voice A feeds into Voice B.
   - *B to A*: Voice B feeds into Voice A.
   - *Routing Cascade (`PERF_ROUTING_CASCADE`)*: Controls coupling intensity.
5. **Voice Feedback Processing (per voice)**:
   - *Fractional Delay Read*: Reads from circular buffers with Lagrange fractional interpolation.
   - *High-Pass Filter (HPF)* & *Low-Pass Filter (LPF)*: Shapes frequency response.
   - *Presence Guard*: Suppresses dynamic accumulator peaks in the 1.5–6 kHz range.
   - *Attack Soften*: Attenuates sharp pick attacks on delay repeats.
   - *Saturation*: Adds tube/tape-style harmonic richness.
   - *Restricted Diffusion*: Smooths repeats.
   - *Feedback Tap*: Feeds signal back into the delay line. Feedback is T60-derived ($g = 10^{-3 \cdot T / T_{60}}$) and capped at 0.985.
6. **Adaptive Voice Mix**: Combines outputs of Voice A and Voice B with dynamic damping/bloom.
7. **Stereo Crossover (`STEREO_LF_MONO`)**: Centers frequencies below the low crossover cutoff to preserve punch.
8. **Global Mix (`GLOBAL_MIX`)**: Blends dry and wet paths based on Insert or Send modes (`GLOBAL_MIX_MODE`).
9. **Output Stage (`GLOBAL_OUTPUT_GAIN`)**: Applies master output level and safety limiting.

## Controls and Parameters
Grouped by interface tabs:

### Global Controls
- **Bypass**: bool, Off (0.0) or On (1.0), default Off. Bypasses delay processing.
- **Bypass Mode**: choice, Hard (0) or Trails (1), default Trails. Hard silences the delay output immediately; Trails allows echoes to decay naturally.
- **Input Gain**: float, -24.0 to +24.0 dB, default 0.0 dB. Sets the pre-processing input level.
- **Output Gain**: float, -24.0 to +24.0 dB, default 0.0 dB. Sets the post-processing output level.
- **Mix**: float, 0.0% to 100.0%, default 22.0%. Sets the dry/wet balance.
- **Mix Mode**: choice, Insert (0) or Send (1), default Insert. Selects between crossfaded wet/dry (Insert) or wet-only output (Send).
- **Spillover**: bool, Off (0.0) or On (1.0), default On. Retains delay tails during scene switching.
- **Safety Limiter**: bool, Off (0.0) or On (1.0), default On. Active feedback threshold limiter preventing runaway escalation.

### Performance Tab
- **Macro Time**: float, 20.0 to 2000.0 ms, default 380.0 ms. Sets fundamental delay period (time basis).
- **Macro Space**: float, 0.0% to 100.0%, default 40.0%. Adjusts overall wet level and diffusion.
- **Macro Definition**: float, 0.0% to 100.0%, default 65.0%. Enhances early repeat clarity.
- **Macro Bloom**: float, 0.0% to 100.0%, default 45.0%. Scales the responsiveness of delay expansion on sustained notes.
- **Macro Tail**: float, 0.15 to 12.0 s, default 2.8 s. Sets the primary decay T60 time.
- **Macro Focus**: float, 0.0% to 100.0%, default 55.0%. Focuses delay image toward mono center.
- **Macro Width**: float, 0.0% to 100.0%, default 65.0%. Expands delay stereo image width.
- **Macro Character**: float, 0.0% to 100.0%, default 30.0%. Adds saturation and texture to repeats.
- **Tap Trigger**: bool, Off/On, default Off. Tap-tempo trigger.
- **Hold Trigger**: bool, Off/On, default Off. Freezes the feedback loop.
- **Hold Mode**: choice, Momentary (0) or Latch (1), default Momentary.
- **Adaptive Mode**: choice, Off (0), Level (1), Articulation (2), default Articulation. Selects static, standard envelope ducking, or full articulation tracking.
- **Routing**: choice, Parallel (0), A to B (1), B to A (2), default Parallel.
- **Routing Cascade**: float, 0.0% to 100.0%, default 50.0%. Scales cascade connection routing gain.
- **Scene Select**: choice, Scene 1 to 4, default Scene 1. Selects the starting scene for recall and morphing.
- **Scene Morph**: float, 0.0% to 100.0%, default 0.0%. Morphs from the selected scene toward the next scene. Scene 4 morphs to itself.

### Voices Tab
- **Time Mode**: choice, Milliseconds (0), Host Sync (1), Tap (2), default Milliseconds.
- **Sync Division**: choice, 15 divisions from 1/1 to 1/16T, default 1/4 (index 6).
- **Time Glide**: float, 5.0 to 1000.0 ms, default 120.0 ms. Sets delay time interpolation speed.
- **Voice [A/B] Enable**: bool, default On. Enables/disables individual voice channels.
- **Voice [A/B] Time Ratio**: float, 0.25 to 4.0, default 0.5 (A) / 1.0 (B). Scales delay time relative to macro time.
- **Voice [A/B] Fine Offset**: float, -50.0 to +50.0 ms, default 0.0 ms. Applies micro delay offsets.
- **Voice [A/B] Level**: float, -60.0 to 0.0 dB, default -12.0 dB (A) / -10.0 dB (B). Base output level.
- **Voice [A/B] Tail Scale**: float, 0.25 to 4.0, default 0.65 (A) / 1.2 (B). Scales decay T60 time.
- **Voice [A/B] Adaptive Role**: float, 0.0% to 100.0%, default 40.0% (A) / 85.0% (B). Determines susceptibility to adaptive analyzer damping.
- **Voice [A/B] Pan**: float, -100.0 to +100.0, default -20.0 (A) / +20.0 (B). Position in stereo field.
- **Voice [A/B] Width**: float, 0.0% to 200.0%, default 60.0% (A) / 130.0% (B). Local voice stereo width.
- **Voice [A/B] Stereo Offset**: float, 0.0 to 30.0 ms, default 3.0 ms (A) / 11.0 ms (B). Introduces L/R channel delay.
- **Voice [A/B] Cross Feedback**: float, 0.0% to 100.0%, default 15.0% (A) / 35.0% (B). Mixes feedback between stereo channels.

### Tone & Motion Tab
- **LF Mono**: bool, default On. Keeps low frequencies centered.
- **LF Mono Freq**: float, 60.0 to 500.0 Hz, default 180.0 Hz. Low-crossover safety filter cutoff.
- **Voice [A/B] Mod Phase**: float, 0.0 to 180.0 deg, default 90.0 (A) / 180.0 (B). Phase offset between channels.
- **Voice [A/B] HPF**: float, 40.0 to 1500.0 Hz, default 180.0 Hz (A) / 220.0 Hz (B). High-pass filter.
- **Voice [A/B] LPF**: float, 2.0 to 20.0 kHz, default 6.5 kHz (A) / 5.5 kHz (B). Low-pass filter.
- **Voice [A/B] Presence Freq**: float, 1.5 to 6.0 kHz, default 3.2 kHz. Sets center frequency of Presence Guard.
- **Voice [A/B] Presence Guard**: float, 0.0 to 12.0 dB, default 4.0 dB (A) / 5.0 dB (B). Active presence-frequency limiter.
- **Voice [A/B] Attack Soften**: float, 0.0% to 100.0%, default 30.0% (A) / 40.0% (B). Softens transient pick spikes.
- **Voice [A/B] Mod Rate**: float, 0.05 to 5.0 Hz, default 0.35 Hz (A) / 0.23 Hz (B). LFO speed.
- **Voice [A/B] Mod Depth**: float, 0.0 to 8.0 ms, default 0.4 ms (A) / 0.7 ms (B). LFO pitch modulation depth.
- **Voice [A/B] Drift**: float, 0.0% to 100.0%, default 10.0% (A) / 15.0% (B). Slow random timing offset.
- **Voice [A/B] Diffusion**: float, 0.0% to 40.0%, default 8.0% (A) / 12.0% (B). Repeat smoothing factor.
- **Voice [A/B] Saturation**: float, 0.0% to 100.0%, default 8.0% (A) / 10.0% (B). Clip saturation intensity.

### Intelligence Tab
- **Sensitivity**: float, 0.0% to 100.0%, default 55.0%. Global analyzer input sensitivity.
- **Input Gate**: float, -80.0 to -20.0 dB, default -55.0 dB. Sidechain detector gate threshold.
- **Density Window**: float, 200.0 to 1000.0 ms, default 500.0 ms. History window for event density.
- **Density Start**: float, 1.0 to 8.0 ev/s, default 3.0 ev/s. Density threshold where contraction begins.
- **Density Full**: float, 6.0 to 20.0 ev/s, default 12.0 ev/s. Threshold for maximum contraction.
- **Sustain Start**: float, 50.0 to 800.0 ms, default 180.0 ms. Held-note duration where bloom begins.
- **Sustain Full**: float, 150.0 to 2000.0 ms, default 650.0 ms. Held-note duration for maximum bloom.
- **Phrase Gap**: float, 20.0 to 500.0 ms, default 120.0 ms. Silence threshold to reset analyzer state.
- **Contraction Attack**: float, 1.0 to 100.0 ms, default 8.0 ms. Speed of contraction onset.
- **Contraction Release**: float, 20.0 to 1000.0 ms, default 220.0 ms. Delay recovery speed.
- **Wet Reduction**: float, 0.0 to 18.0 dB, default 8.0 dB. Maximum dynamic gain ducking during runs.
- **Tail Reduction**: float, 0.0 to 2.0 oct, default 1.0 oct. Maximum decay scale reduction.
- **Tone Darkening**: float, 0.0 to 2.0 oct, default 0.75 oct. Maximum dynamic LPF frequency reduction.
- **Width Reduction**: float, 0.0% to 100.0%, default 35.0%. Dynamic stereo width compression.
- **Attack Soften Increase**: float, 0.0% to 100.0%, default 45.0%. Additional pick attack softening during runs.
- **Bloom Rise**: float, 20.0 to 1500.0 ms, default 220.0 ms. Attack time of delay expansion.
- **Bloom Hold**: float, 0.0 to 2000.0 ms, default 350.0 ms. Sustain duration of bloom before decay.
- **Bloom Release**: float, 50.0 to 3000.0 ms, default 800.0 ms. Decay time of bloom.
- **Wet Lift**: float, 0.0 to 12.0 dB, default 4.0 dB. Dynamic boost applied during bloom.
- **Tail Expansion**: float, 0.0 to 2.0 oct, default 0.75 oct. Dynamic decay tail extension.
- **Tone Opening**: float, 0.0 to 1.0 oct, default 0.35 oct. Dynamic LPF cutoff extension.
- **Width Expansion**: float, 0.0% to 100.0%, default 30.0%. Dynamic stereo widening.
- **Diffusion Increase**: float, 0.0% to 30.0%, default 12.0%. Added repeat diffusion.
- **Transient Crossover**: float, 1000.0 to 6000.0 Hz, default 2500.0 Hz. Filter cutoff frequency separating transient analysis bands.

## Stereo and Mono Behavior
Low frequency audio is centered below a crossover point (`STEREO_LF_MONO`, range 60 to 500 Hz) to maintain bass punch and prevent phase cancellation. The rest of the delay voices are panned across the stereo field according to the Voice Pan settings.

## Latency and Lookahead
- The DSP chain operates with zero reported latency.
- Internal performance analysis runs at a fixed 48 kHz rate via a sample-rate converter, which does not delay the audio dry signal path.

## Bypass Behavior
Supports true bypass with two modes: Hard (silences output immediately) and Trails (allows tails to decay naturally).

## Presets and State Recall
ShredSpace relies on a dedicated profile catalog rather than traditional presets. The binary bundles 12 factory profile JSON documents:
- `BalladBloom`, `BufferBend`, `ControlledCascade`, `DarkHighGain`, `DryCenterArena`, `LegatoSpace`, `MonoRigPolish`, `NeoclassicalFlight`, `PrecisionRack`, `RapidDefinition`, `SingingLead`, and `WideArpeggio`.

Recalling a profile updates the APVTS base parameters on the message thread, while macro mappings are evaluated in real time without altering the base values. Standard DAW session saving and loading are fully supported.

### Scene Recall, Morphing, and Automation
- Each profile supplies four scene targets for the eight performance macros, Mix, Routing, and Hold behavior. Selecting a profile installs those scene targets along with the profile's base parameters and macro mappings.
- Scene Morph interpolates continuously from the selected scene to the following scene. Time and Tail use logarithmic interpolation; the other macro targets and Mix use linear interpolation.
- Discrete Routing and Hold behavior follow the nearer scene and switch at the midpoint of the morph. Spillover, when enabled, preserves existing delay tails across scene changes.
- Scene Select, Scene Morph, and profile recall are intended for deliberate performance changes. Use moderate host automation rates for scene and profile changes; dense point-by-point automation provides no practical benefit and can add avoidable message-thread work.

## Metering and Visual Feedback
- **Stereo Level Meters (`MeterComponent`)**: Peak and RMS output bars on a -50 dB to +6 dB scale.
- **Telemetry Indicators**: Real-time scalar indicators for Onset Activity, Note Density, Sustain Level, Contraction, and Bloom.
- **Detector Display (`DetectorDisplay`)**: A scrolling timeline rendering onset event triggers (green), note density tracking (yellow), sustain estimation (blue), and phrase gap boundaries over a 512-frame window.

## Supported Formats
- VST3, LV2, Standalone.
