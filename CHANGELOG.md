# Changelog

## 1.0.7 — 2026-10-30
- New: English interface (program, menu, installer, manual); the language is chosen during setup.
- New: `Setup.exe` installer with language selection.
- New: RedaotZip (github.com/redaot/RedaotZip) is listed first in the instructions file added to archives.
- Fixed: on .zip files the options appeared in a nested submenu on Windows 11. *Extract* and *Archive* now appear directly, one under the other (two menu packages).

## 1.0.6 — 2026-10-30
- Fixed: the "RedaotZip" submenu on .zip files had no icon.

## 1.0.5 — 2026-10-30
- Fixed: options appeared twice under "Show more options" on Windows 11.

## 1.0.4 — 2026-10-29
- Classic menu kept as a fallback; menu extension writes a log (`%LOCALAPPDATA%\RedaotZip\shell.log`).
- New: `Diagnostic.cmd` creates `RedaotZip-diagnostic.txt` on the Desktop.

## 1.0.3 — 2026-10-29
- Fixed: Windows 11 menu package rejected with error 0x8007000B (now ZIP64, MakeAppx layout).
- Setup detects when Smart App Control or an antivirus blocks the program.
- PDF manual included.

## 1.0.2 — 2026-10-29
- Setup restarts Explorer automatically.

## 1.0.1 — 2026-10-29
- Fixed: corrupted Windows 11 menu package (error 0x8007000D).
- Windows 10 uses the classic menu, which shows directly there.
- "Hide file names" is on by default.

## 1.0.0 — 2026-10-29
- First version.
