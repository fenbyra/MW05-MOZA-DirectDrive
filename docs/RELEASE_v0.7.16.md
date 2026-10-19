# MW05 MOZA DirectDrive v0.7.16 — Drag Race Fix

Bug-fix update for the existing v0.7.12 public installation.

## Fixed
- Fixed loss of MOZA analog input when entering Drag races.
- Added support for the Drag player-input interface and specialized transmission.
- Fixed RPM LED telemetry in Drag races by reading the dedicated Drag HUD tachometer.
- Preserved normal driving input and the normal HUD RPM source after returning from Drag.

## Download / install
**Existing users:** download `MW05_MOZA_DirectDrive_v0.7.16_Update.zip`.
Close the game and back up the old files outside the game folder.
Extract the ZIP, then copy the **contents** of `COPY_TO_GAME` into the game root
(next to `speed.exe`), accepting replacement of the ASI and installation-check manifest.
Do not copy the outer release folder into `scripts`. Keep only one DirectDrive ASI.
Your INI, bridge DLL, launcher scripts and external dependencies stay unchanged.

**New users:** install the v0.7.12 public package and its required official x86
dependencies first, then apply this update. This is not a standalone installation.

`MW05_MOZA_DirectDrive_v0.7.16_Source.zip` is optional ASI source/tests.
GitHub's automatic “Source code” downloads are not ready-to-install packages.

## Compatibility / notes
- Game-tested by the project owner on the existing NFS MW 2005 x86 v1.3 / MOZA setup.
- New game log confirms active Drag HUD samples and normal HUD samples after Drag;
  no UDP send errors were reported in that session. The owner confirmed working controls/LEDs.
- Other hardware, executable builds and Pit House versions remain unverified.
- The tested binary retains the banner `v0.7.16 DRAG TELEMETRY TEST 1` intentionally.
- No new force-feedback, clutch, H-pattern or full vehicle-telemetry features.
- No game EXE, MOZA SDK, ASI loader or personal logs are included.
- Original project code: BSD-3-Clause; third-party licenses apply to their components.

If a regression occurs, close the game and restore both backed-up files.
For reports enable Diagnostics=1 and review logs for personal paths before sharing.

---

## По-русски
Исправлены отключение аналогового ввода MOZA и отсутствие RPM LED в дрэг-гонках.
Добавлена поддержка отдельного input-интерфейса, трансмиссии и тахометра дрэга.
Работу подтвердил автор проекта в игре; другие конфигурации пока не проверены.

Это **обновление установленной v0.7.12**, не полный установщик.
Закройте игру, сохраните старые файлы вне игры и перенесите содержимое COPY_TO_GAME
в корень игры с заменой ASI и ExpectedFiles.json. INI, DLL и скрипты launcher не менять.
Для чистой установки сначала установите v0.7.12 и официальные x86-зависимости.
Надпись DRAG TELEMETRY TEST 1 в логе сохранена, чтобы не менять проверенную сборку.
