# MW05 MOZA DirectDrive

Direct analog MOZA wheel and pedal input for
Need for Speed: Most Wanted (2005).

## Download

Players should download the ready-to-install archive from
[Releases](../../releases).

GitHub's automatic “Source code” archives are not installation packages.

## Features

- Analog steering, throttle and brake input.
- Native game braking and reverse behavior.
- Automatic/manual transmission selected in the game.
- Wheel-button assignments through the game's controls menu.
- D-pad menu navigation.
- S2 / F8 input toggle.
- Input recovery after pauses and supported game transitions.
- RPM LED telemetry through a local MOZA Pit House receiver.

## Tested setup

- Need for Speed: Most Wanted (2005), x86 v1.3.
- MOZA R3 + ES + SR-P Lite.
- MOZA Pit House 1.4.1.13.
- Steering ranges: 720° and 900°.

Other hardware, executable builds and Pit House versions are unverified.

## Installation

- [Russian instructions](README_RU.txt)
- [English instructions](README_EN.txt)

MOZA_SDK.dll and Ultimate ASI Loader are not bundled.
Obtain the required x86 dependencies from their official sources.

## Source availability

This repository contains the ASI plugin sources, host tests,
launcher scripts and telemetry helper source.

The current release also uses MW05_MozaBridge.dll.
Its source and complete build instructions are not included yet.

The ASI build script currently targets an existing configured
nfsmw-2005-sdk workspace; it is not a standalone build system.

## Limitations

- No custom force feedback implementation.
- No H-pattern shifter or clutch support.
- No paddle-based manual reverse selection.
- Telemetry output is focused on RPM, not a full vehicle-data stream.

## Telemetry helper

The launcher creates a temporary compatibility-marker process named
forzahorizon6.exe to activate a compatible Pit House receiver.
It is our own helper, not a Forza game binary.

Read the installation instructions before using telemetry.
Disable automatic preset loading in Pit House first.

## Reporting issues

Please include:
- Game executable version or SHA256.
- MOZA hardware and Pit House version.
- Mod version.
- Steps to reproduce.
- Relevant logs with personal paths reviewed or removed.

Do not upload game executables or proprietary SDK files.

## License and third-party components

Original project code is licensed under BSD-3-Clause.
Third-party components retain their own licenses;
see LICENSE and third_party.

The project license does not cover MOZA SDK or game files.

## Disclaimer

Unofficial community project.
Not affiliated with or endorsed by EA or MOZA Racing.
