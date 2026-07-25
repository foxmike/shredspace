# Phase K Release Verification

## Product Identity

| Property | Value |
| :--- | :--- |
| Plugin Name | ShredSpace |
| Vendor | nSpace Systems |
| Version | 1.0.0 |
| Manufacturer Code | NSPC |
| Plugin Code | ShSp |
| Bundle Identifier | com.nspacesystems.shredspace |
| LV2 URI | urn:nspacesystems:plugin:shredspace |

## Command Matrix

| Command | Result | Exit Status |
| :--- | :--- | :--- |
| `cmake --preset linux-release` | PASS | 0 |
| `cmake --build /home/mike/dev/mike/music/plugins/shredspace/build/linux-release --target ShredSpace_VST3 ShredSpace_LV2 ShredSpace_Standalone ShredSpaceTests` | PASS | 0 |
| `ctest --test-dir /home/mike/dev/mike/music/plugins/shredspace/build/linux-release --output-on-failure` | PASS | 0 |
| `[[ -d /home/mike/dev/mike/music/plugins/shredspace/build/linux-release/ShredSpace_artefacts/Release/VST3/ShredSpace.vst3 ]]` | PASS | 0 |
| `[[ -d /home/mike/dev/mike/music/plugins/shredspace/build/linux-release/ShredSpace_artefacts/Release/LV2/ShredSpace.lv2 ]]` | PASS | 0 |
| `[[ -f /home/mike/dev/mike/music/plugins/shredspace/build/linux-release/ShredSpace_artefacts/Release/Standalone/ShredSpace && -x /home/mike/dev/mike/music/plugins/shredspace/build/linux-release/ShredSpace_artefacts/Release/Standalone/ShredSpace ]]` | PASS | 0 |

## Test Results

**CTest Summary:**
- Tests run: 1
- Passed: 1
- Failed: 0

**State Round-Trip Test:**
- `ShredSpaceTests`: PASSED

## Remaining operator work before release

- Real-guitar evaluation after high-gain amp or NAM processing, covering alternate picking, economy picking, sweep arpeggios, legato, tapping, bends, wide vibrato, held notes, phrase endings, chords, palm-muted rhythms, sustained feedback, string noise, and gated silence; judged for dry attack clarity, repeated-note clutter, upper-mid buildup, pumping, width, mono compatibility, bloom timing, sustained-note size, and profile usefulness; at 44.1 kHz, 48 kHz, and 96 kHz, in REAPER, Studio One, Bitwig, and Standalone, across VST3 and LV2.
- Profile tuning updates to the bundled resources based on that evaluation; profiles are NOT release-ready merely because the JSON parses.
- The later Windows MSVC build performed on Windows tooling via the Aural Maximizer Pro workflow; the `windows-msvc` CMake preset is configuration only and was not built in this phase.

## Final Status

Linux release gate: PASSED
