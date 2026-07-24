#!/usr/bin/env bash
# Paste-and-run build script for the ShredSpace plugin (VST3, LV2, Standalone).
#
# Runs the linux-release CMake preset, builds the project, and verifies that all
# three Linux artifacts exist on disk.
#
# This script deliberately does NOT enable exit-on-error (errexit) mode. A failed
# configure or build must not prevent the artifact checks from running and
# reporting, so that the operator always sees the full picture of what
# succeeded and what failed.

# Resolve the repository root from this script's own location so the script runs
# correctly from any working directory. No hard-coded absolute paths, no ~ use.
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT" || {
    echo "FAIL: could not change directory to repository root: $REPO_ROOT" >&2
    exit 1
}

# Artifact locations produced by the linux-release preset. The preset binaryDir
# resolves to build/linux-release, and JUCE places each format's output under
# build/linux-release/ShredSpace_artefacts/Release/VST3, .../LV2, and
# .../Standalone, named after the product ShredSpace.
ARTEFACTS_DIR="$REPO_ROOT/build/linux-release/ShredSpace_artefacts/Release"
VST3_PATH="$ARTEFACTS_DIR/VST3/ShredSpace.vst3"
LV2_PATH="$ARTEFACTS_DIR/LV2/ShredSpace.lv2"
STANDALONE_PATH="$ARTEFACTS_DIR/Standalone/ShredSpace"

# ---------------------------------------------------------------------------
# Step 1: Configure.
# ---------------------------------------------------------------------------
echo "==> cmake --preset linux-release"
cmake --preset linux-release
CONFIGURE_STATUS=$?
echo "CONFIGURE: cmake --preset linux-release exited with status $CONFIGURE_STATUS"

# ---------------------------------------------------------------------------
# Step 2: Build.
# ---------------------------------------------------------------------------
echo "==> cmake --build build/linux-release"
cmake --build build/linux-release
BUILD_STATUS=$?
echo "BUILD: cmake --build build/linux-release exited with status $BUILD_STATUS"

# ---------------------------------------------------------------------------
# Step 3: Artifact checks. All three always run, regardless of earlier results.
# ---------------------------------------------------------------------------
check_artifact() {
    local label="$1"
    local path="$2"
    if [ -e "$path" ]; then
        echo "PASS: $label $path"
        return 0
    fi
    echo "FAIL: $label $path"
    return 1
}

VST3_RESULT=1
LV2_RESULT=1
STANDALONE_RESULT=1

check_artifact VST3 "$VST3_PATH"           && VST3_RESULT=0
check_artifact LV2 "$LV2_PATH"             && LV2_RESULT=0
check_artifact Standalone "$STANDALONE_PATH" && STANDALONE_RESULT=0

# ---------------------------------------------------------------------------
# Step 4: Summary. Exit 0 only when configure and build both succeeded AND all
# three artifacts exist; otherwise exit 1.
# ---------------------------------------------------------------------------
if [ "$CONFIGURE_STATUS" -eq 0 ] \
    && [ "$BUILD_STATUS" -eq 0 ] \
    && [ "$VST3_RESULT" -eq 0 ] \
    && [ "$LV2_RESULT" -eq 0 ] \
    && [ "$STANDALONE_RESULT" -eq 0 ]; then
    echo "SUMMARY: configure and build succeeded; VST3, LV2, and Standalone artifacts present"
    exit 0
fi

echo "SUMMARY: build incomplete (configure=$CONFIGURE_STATUS build=$BUILD_STATUS vst3=$VST3_RESULT lv2=$LV2_RESULT standalone=$STANDALONE_RESULT)"
exit 1
