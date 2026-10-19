MW05 MOZA DirectDrive v0.7.16 ASI source and host tests.
Contains drag input and drag HUD RPM fixes over public v0.7.12.
Game-tested by project owner as v0.7.16 DRAG TELEMETRY TEST 1; banner retained.
No standalone SDK/toolchain or proprietary MOZA files included.
Build in existing configured MSYS2 MINGW32 nfsmw-2005-sdk workspace:
  bash install_and_build.sh /c/MW05_Local_Assistant/workspace/nfsmw-2005-sdk
Script backs up SDK sources/ASI and builds only mw05_mirror_probe.
It does not copy files to the game. Do not overwrite a working binary without backup.
Linux host tests: bash tests/run_host_tests.sh
Third-party licenses are in ../third_party in the repository/release.
