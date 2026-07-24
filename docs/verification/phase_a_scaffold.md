# Phase A Scaffold Verification

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

## Commands Executed

```bash
cmake --preset linux-release
```
Exit status: 1

```bash
cmake --build build/linux-release
```
Exit status: 1

```bash
bash scripts/build_all.sh
```
Exit status: 1

## Artifact Results

- VST3: /home/mike/dev/mike/music/plugins/shredspace/build/linux-release/ShredSpace_artefacts/Release/VST3/ShredSpace.vst3 FAIL
- LV2: /home/mike/dev/mike/music/plugins/shredspace/build/linux-release/ShredSpace_artefacts/Release/LV2/ShredSpace.lv2 FAIL
- Standalone: /home/mike/dev/mike/music/plugins/shredspace/build/linux-release/ShredSpace_artefacts/Release/Standalone/ShredSpace FAIL

## Bus Layout Contract

The only supported bus layouts are mono-in/mono-out, mono-in/stereo-out, and stereo-in/stereo-out. The `processBlock` method clears every output channel beyond the input channel count before any processing occurs.

## Notes

The windows-msvc preset is configuration only and was not built in this plan. JUCE is a submodule pinned to 7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2 and was not modified.
