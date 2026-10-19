#!/usr/bin/env bash
# Run in the user's EXISTING MSYS2 MINGW32 environment. Does not deploy to the game.
set -euo pipefail
export PATH="/mingw32/bin:$PATH"
payload="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
sdk="${1:-/c/MW05_Local_Assistant/workspace/nfsmw-2005-sdk}"
sdk="$(cd -- "$sdk" && pwd)"
cmake_file="$sdk/examples/CMakeLists.txt"
source_dir="$sdk/examples/mw05_mirror_probe"

[[ -f "$sdk/CMakeLists.txt" && -f "$cmake_file" && -f "$source_dir/main.cpp" ]] || {
  echo "STOP: not the expected existing SDK tree: $sdk" >&2; exit 1;
}
grep -Eq 'nfsmw_add_plugin[[:space:]]*\([[:space:]]*mw05_mirror_probe[[:space:]]+SOURCES[[:space:]]+mw05_mirror_probe/main\.cpp[[:space:]]*\)' "$cmake_file" || {
  echo "STOP: the existing target does not match examples/mw05_mirror_probe/main.cpp" >&2; exit 1;
}
[[ -f "$sdk/build/CMakeCache.txt" ]] || {
  echo "STOP: existing build/CMakeCache.txt not found. Do not create another toolchain/build." >&2; exit 1;
}
command -v cmake >/dev/null
command -v g++ >/dev/null
target="$(g++ -dumpmachine)"
[[ "$target" == i686-w64-mingw32 ]] || {
  echo "STOP: use MSYS2 MINGW32. Current compiler target: $target" >&2; exit 1;
}

stamp="$(date +%Y%m%d_%H%M%S)"
mkdir -p "$sdk/backups"
backup="$(mktemp -d "$sdk/backups/mw05_getcontrols_${stamp}_XXXXXX")"
cp -p "$cmake_file" "$backup/examples_CMakeLists.txt"
cp -p "$source_dir/main.cpp" "$backup/main.cpp"
for header in probe_core.h moza_core.h moza_bridge_api.h drive_probe.h direction_core.h session_gate.h button_core.h di_button_reader.h menu_core.h telemetry_core.h hud_telemetry_core.h udp_telemetry_core.h udp_transport.h; do
  if [[ -f "$source_dir/$header" ]]; then
    cp -p "$source_dir/$header" "$backup/$header"
  fi
done
if [[ -f "$sdk/build/examples/mw05_mirror_probe.asi" ]]; then
  cp -p "$sdk/build/examples/mw05_mirror_probe.asi" "$backup/mw05_mirror_probe.asi"
fi
cp "$payload/examples/mw05_mirror_probe/main.cpp" "$source_dir/main.cpp"
for header in probe_core.h moza_core.h moza_bridge_api.h drive_probe.h direction_core.h session_gate.h button_core.h di_button_reader.h menu_core.h telemetry_core.h hud_telemetry_core.h udp_telemetry_core.h udp_transport.h; do
  cp "$payload/examples/mw05_mirror_probe/$header" "$source_dir/$header"
done
link_line='target_link_libraries(mw05_mirror_probe PRIVATE user32 ws2_32)'
if ! grep -Fqx "$link_line" "$cmake_file"; then
  printf '\n# MW05 GetControls Test: foreground-window check and F8.\n%s\n' "$link_line" >> "$cmake_file"
fi

echo "Source backup: $backup"
echo "Building ONLY the existing mw05_mirror_probe target."
cd "$sdk"
cmake -S . -B build 2>&1 | tee "$backup/configure.log"
cmake --build build --target mw05_mirror_probe -j4 2>&1 | tee "$backup/build.log"
asi="$sdk/build/examples/mw05_mirror_probe.asi"
[[ -s "$asi" ]] || { echo "STOP: ASI output missing: $asi" >&2; exit 1; }
grep -aFq 'MW05 MOZA DirectDrive v0.7.16 DRAG TELEMETRY TEST 1' "$asi" || {
  echo "STOP: output ASI does not contain the v0.7.16 DRAG TELEMETRY TEST 1 runtime banner." >&2; exit 1;
}
echo
ls -l "$asi"
sha256sum "$asi" | tee "$backup/new_asi.sha256"
if command -v objdump >/dev/null; then
  imports="$(objdump -p "$asi" | sed -n 's/.*DLL Name: //p')"
  printf 'DLL imports:\n%s\n' "$imports"
  if printf '%s\n' "$imports" | grep -Eiq 'libgcc|libstdc\+\+|libwinpthread|libssp'; then
    echo "STOP: non-system runtime DLL dependency. Review static-link settings before deploying." >&2
    exit 1
  fi
fi
echo
echo "ASI BUILD OK. NOTHING has been copied to the game."
echo "v0.7.16 DRAG TELEMETRY TEST 1 ASI-only update: keep the WORKING bridge, SDK DLL and INI."
echo "Do NOT rebuild the MSVC bridge for this update. Follow README_RU.md."
