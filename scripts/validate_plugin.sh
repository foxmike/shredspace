#!/usr/bin/env bash

failure_count=0

script_path=${BASH_SOURCE[0]}
script_dir=${script_path%/*}
if [[ "$script_dir" == "$script_path" ]]; then
    script_dir=.
fi

printf 'COMMAND: cd %q\n' "$script_dir/.."
if ! cd "$script_dir/.."; then
    printf 'FAIL: unable to change to the repository root from %s\n' "$script_dir"
    exit 1
fi

repo_root=$(pwd -P)
build_dir="$repo_root/build/linux-release"
artifact_root="$build_dir/ShredSpace_artefacts/Release"
vst3_path="$artifact_root/VST3/ShredSpace.vst3"
lv2_path="$artifact_root/LV2/ShredSpace.lv2"
standalone_path="$artifact_root/Standalone/ShredSpace"

configure_result=FAIL
build_result=FAIL
tests_result=FAIL
vst3_result=FAIL
lv2_result=FAIL
standalone_result=FAIL

print_command() {
    printf 'COMMAND:'
    printf ' %q' "$@"
    printf '\n'
}

printf '\n=== CONFIGURE ===\n'
print_command cmake --preset linux-release
cmake --preset linux-release
configure_status=$?
if (( configure_status == 0 )); then
    configure_result=PASS
    printf 'PASS: CONFIGURE\n'
else
    ((failure_count += 1))
    printf 'FAIL: CONFIGURE (exit status %d)\n' "$configure_status"
fi

printf '\n=== BUILD ===\n'
print_command cmake --build "$build_dir" --target \
    ShredSpace_VST3 ShredSpace_LV2 ShredSpace_Standalone ShredSpaceTests
cmake --build "$build_dir" --target \
    ShredSpace_VST3 ShredSpace_LV2 ShredSpace_Standalone ShredSpaceTests
build_status=$?
if (( build_status == 0 )); then
    build_result=PASS
    printf 'PASS: BUILD\n'
else
    ((failure_count += 1))
    printf 'FAIL: BUILD (exit status %d)\n' "$build_status"
fi

printf '\n=== TESTS ===\n'
print_command ctest --test-dir "$build_dir" --output-on-failure
ctest --test-dir "$build_dir" --output-on-failure
tests_status=$?
if (( tests_status == 0 )); then
    tests_result=PASS
    printf 'PASS: TESTS\n'
else
    ((failure_count += 1))
    printf 'FAIL: TESTS (exit status %d)\n' "$tests_status"
fi

printf '\n=== ARTIFACTS ===\n'
printf 'CHECK: VST3 bundle at %s\n' "$vst3_path"
if [[ -d "$vst3_path" ]]; then
    vst3_result=PASS
    printf 'PASS: VST3 bundle: %s\n' "$vst3_path"
else
    ((failure_count += 1))
    printf 'FAIL: VST3 bundle: %s\n' "$vst3_path"
fi

printf 'CHECK: LV2 bundle at %s\n' "$lv2_path"
if [[ -d "$lv2_path" ]]; then
    lv2_result=PASS
    printf 'PASS: LV2 bundle: %s\n' "$lv2_path"
else
    ((failure_count += 1))
    printf 'FAIL: LV2 bundle: %s\n' "$lv2_path"
fi

printf 'CHECK: Standalone executable at %s\n' "$standalone_path"
if [[ -f "$standalone_path" && -x "$standalone_path" ]]; then
    standalone_result=PASS
    printf 'PASS: Standalone executable: %s\n' "$standalone_path"
else
    ((failure_count += 1))
    printf 'FAIL: Standalone executable: %s\n' "$standalone_path"
fi

printf '\n=== SUMMARY ===\n'
printf 'configure: %s\n' "$configure_result"
printf 'build: %s\n' "$build_result"
printf 'tests: %s\n' "$tests_result"
printf 'VST3 bundle: %s\n' "$vst3_result"
printf 'LV2 bundle: %s\n' "$lv2_result"
printf 'Standalone executable: %s\n' "$standalone_result"
printf 'total failures: %d\n' "$failure_count"

if (( failure_count == 0 )); then
    exit 0
fi

exit 1
