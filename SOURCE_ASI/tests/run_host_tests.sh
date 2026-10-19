#!/usr/bin/env bash
# Linux HOST logic/ABI-shape checks, NOT a Windows build or real SDK test.
set -euo pipefail
pkg="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
out="${1:-/tmp/mw05_v0711_host_qa}"
mkdir -p "$out"
for test in resolver_regression_test recorded_drag_replay_test recorded_explosion_replay_test recorded_transition_replay_test moza_core_test drive_probe_test direction_core_test session_gate_test button_core_test menu_core_test telemetry_core_test hud_telemetry_core_test udp_telemetry_test host_mock_test; do
  g++ -std=c++17 -Wall -Wextra -Werror -pedantic -pthread \
    -I"$pkg/tests/host_stubs" -I"$pkg/examples/mw05_mirror_probe" "$pkg/tests/$test.cpp" -o "$out/$test"
  "$out/$test"
done
g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -I"$pkg/tests/button_stubs" "$pkg/tests/di_button_reader_test.cpp" -o "$out/di_button_reader_test"
"$out/di_button_reader_test"
gcc -std=c11 -Wall -Wextra -Werror -pedantic \
  "$pkg/tests/c_api_layout_test.c" -o "$out/c_api_layout_test"
"$out/c_api_layout_test"
echo "PASS: plain C header/layout check"
bash -n "$pkg/install_and_build.sh"
echo "HOST QA OK. This does not produce an ASI or MSVC bridge DLL."
