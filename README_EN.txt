CURRENT UPDATE: v0.7.16 fixes Drag input and RPM LEDs. Install v0.7.12 first, then the v0.7.16 Update ZIP. See docs/RELEASE_v0.7.16.md. The following v0.7.12 base-install instructions remain applicable.

MW05 MOZA DirectDrive v0.7.12 — public distribution package

Ready-built plugin and bridge for Need for Speed: Most Wanted (2005) x86 v1.3.
Tested setup: MOZA R3 + ES + SR-P Lite, Pit House 1.4.1.13. Other hardware,
executables and Pit House versions are unverified. The tested ASI is unchanged;
its embedded banner remains "v0.7.12 RACE DIALOG TEST".

REQUIRED EXTERNAL DOWNLOADS (NOT INCLUDED)
1. MOZA Racing SDK x86/MSVC2022-32 MOZA_SDK.dll:
   https://mozaracing.com/pages/sdk
   Redistribution permission is pending. Obtain this DLL yourself officially.
2. Ultimate ASI Loader Win32/x86 dinput8.dll:
   https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases
   The old user-tested loader is not redistributed; a newer official version
   needs local compatibility testing.
3. MOZA Pit House/drivers: https://mozaracing.com/pages/downloads
No game files, game EXE, patches or Pit House installer are included.

CLEAN INSTALL
- Make sure the original game starts. Close it and back up your game/profile.
- Copy the CONTENTS of COPY_TO_GAME next to speed.exe, not the outer folder.
- Put both external x86 DLLs next to speed.exe as well. Never in System32.
- Run MW05_Launcher/Check_Installation.cmd. Fix FAIL results. Different external
  DLL/game hashes are warnings of unverified versions, not proof of corruption.
- In Pit House disable automatic preset loading BEFORE using the launcher:
  the compatibility marker may otherwise trigger an unwanted Forza profile.
- Set 720 degrees in Pit House to match WheelRangeDegrees=720. For 900, change
  both places. Set the ES D-pad to button mode, not POV/D-pad mode.
- Set LEDs to engine RPM indication with visible brightness.
- In game, clear BOTH primary and secondary MOZA wheel/pedal bindings for gas,
  brake/reverse, steer left and steer right. Keep keyboard bindings.
- Assign normal wheel buttons and paddles using the game's controls menu.
  Reserve S2/button37 for this mod and buttons5-8 for menu navigation.
- Choose automatic or manual gearbox in the game. Paddles13/14 may be bound
  to shift down/up. Manual paddle selection of R is not added.
- Start using MW05_Launcher/Play_MW05.cmd for automatic LED receiver activation.
  Keep its adjacent PS1/CS files. Start Pit House manually first if detection
  fails, or set its full EXE path in PitHousePath.txt.
- Release pedals, centre wheel for 0.5-1 seconds, press S2 or F8 once. You may
  queue activation in the main menu. Press again to turn axes off.
- Axes default OFF at each game start. Previously active input resumes after
  valid pause/transition recovery without another neutral dwell; current pedal
  and steering positions can apply immediately. Manual OFF cancels resume.

MENU: on the tested ES, B confirms, X goes back, A is an extra action.
Directions use buttons5-8. The race-entry accept/decline dialog was tested.
The new UI allowance is not a unique dialog recognizer; unknown screens may
need further work. Axes remain suspended in blocked contexts.

TELEMETRY TRANSPARENCY
The ASI sends HUD RPM/reference data over localhost UDP 127.0.0.1:20066.
Speed/gear fields are not implemented as a full telemetry output.
The launcher locally compiles included TelemetryMarker.cs into a temporary
forzahorizon6.exe name ONLY to activate a compatible Pit House receiver.
This is our own transparent helper, not an actual Forza binary. Do not run
actual Forza at the same time. Keep the minimized PowerShell supervisor open;
helper exits on game closure or about15 seconds after lost heartbeat.
No registry/firewall/startup tasks or automatic downloads. No motor FFB calls.
The CMD's execution-policy option is process-scoped. Do not bypass security
or organizational restrictions. Direct speed.exe launch can still use the
axes with working SDK, but automatic LED receiver activation is not guaranteed.

SETTINGS: edit INI with game closed. Diagnostics=0 is release logging; use1
for troubleshooting. ToggleButton=0 disables wheel toggle but keeps F8.
MenuNavigation=0 disables added menu navigation; Telemetry Enabled=0 disables
UDP only. WheelRangeDegrees must match Pit House. No universal shift RPM
threshold exists for every car/tune; calibrate your own Pit House LED profile.

UPGRADE/REMOVE
Back up outside the game folder. Never leave old .asi copies in loader scan
folders. Replace plugin/bridge/launcher; retain your preferred angle in INI.
To remove, close game, remove only this plugin, bridge, INI and launcher.
Do not delete external DLLs shared with other mods. Restore native axis binds.

SUPPORT
Check installation report and root mw05_directdrive.log. Enable Diagnostics=1
and preserve log before restarting. Launcher log is in MW05_Launcher. Logs can
contain personal local paths; review before sharing. No personal logs included.
No self-contained all-DLL bundle is provided pending MOZA permission.
Detailed checks, hashes, license notices and Russian instructions are included.
This is an unofficial project, not endorsed by EA or MOZA.
