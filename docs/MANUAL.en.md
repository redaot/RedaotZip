# RedaotZip Manual

**RedaotZip v1.0.7 — made by REDAOT**

## What RedaotZip is

RedaotZip creates and opens password-protected ZIP archives encrypted with AES-256. It works from the Windows 11 and Windows 10 right-click menu, directly, without "Show more options". The interface is in English or Romanian.

Archives use the standard WinZip AES format, so they also open in **7-Zip, PeaZip, WinRAR or WinZip**. RedaotZip also opens regular ZIP archives, AES archives made by other programs and old ZipCrypto archives.

**Important limitation:** the ZIP tool built into Windows cannot open AES-256 encrypted archives. It shows an error (usually 0x80004005) and never asks for the password. This is not a problem with the archive: Windows simply lacks the feature. Whoever receives the archive needs one of the programs above.

| Feature | Details |
| --- | --- |
| Encryption | AES-256 on the content of every file, HMAC-SHA1 authenticity check |
| Compression | Deflate, 4 levels; already-compressed files (jpg, mp4, zip…) are stored as-is |
| Size | No practical limit (ZIP64: files over 4 GB, more than 65,535 files) |
| File names | Unicode, any language |
| Language | English or Romanian (chosen during setup) |
| System | Windows 11 or Windows 10 version 2004+, 64-bit |

## Installation

Installation takes under a minute and asks for administrator rights once.

1. Download **`RedaotZip-1.0.7-Setup.exe`**.
2. Double-click it. If "Windows protected your PC" appears, click **More info → Run anyway** (it appears because the program is not signed by a paid certificate authority).
3. Answer **Yes** to the administrator prompt.
4. Choose the language: **English** or **Română**.
5. Click **Next**. At the end Explorer restarts automatically (open folder windows close) and the menu appears right away.

Alternatively, without Setup.exe: extract `RedaotZip-1.0.7-Windows.zip` and double-click **Instaleaza.cmd** (the language follows the Windows language).

What setup does:

- Copies the program and the manual to `C:\Program Files\RedaotZip`.
- On Windows 11, adds the REDAOT certificate to "Trusted People". Windows 11 requires signed packages for top-level menu items; the certificate is used only for the RedaotZip packages.
- Registers the right-click menu: on Windows 11 two small packages (one for "Archive", one for "Extract") so both options appear directly, without a submenu; on Windows 10 the classic menu, which shows directly there.
- Adds a **startup check**: at every Windows start, `RedaotZip.exe /repair` silently checks (under a second) that the menu is registered and restores it if a Windows update removed it.
- Creates the **RedaotZip** and **RedaotZip Manual** Start Menu shortcuts and the **Settings → Apps** entry.

If the Windows 11 menu cannot be registered, the classic menu is used ("Show more options") and the new menu is retried at every start.

## Archiving

Select one or more files or folders, right-click → **Archive with REDAOT (AES-256)**. The archive window opens, showing "made by REDAOT • v1.0.7" at the bottom left.

1. **Archive to create:** by default next to the files, named after the file or folder (e.g. `Documents.zip`). The "..." button picks another place or name.
2. **Password** and **Confirm password:** type the same password twice. The meter shows its strength: weak, fair, good, very good. "Show password" makes it visible.
3. **Compression:** Normal fits almost every case. Maximum is slightly smaller and slower. No compression is the fastest, good for photos and videos.
4. Click **Archive**. The bar shows progress. **Stop** cancels without leaving a half-written archive.
5. At the end, **Show archive** opens it selected in Explorer.

| Option | Default | What it does |
| --- | --- | --- |
| Instructions file for Windows users | on | Adds an unencrypted `!!! CITESTE-MA - ARHIVA CRIPTATA.txt` (in Romanian and English) explaining which program to use. RedaotZip ignores it when extracting. |
| Also hide file names | on | Files go into an inner archive `continut.zip`, encrypted. Without the password even the file names are hidden. |

If the password is short or contains accented characters, the program warns you first. Accented characters work in RedaotZip and 7-Zip, but other programs may interpret them differently.

Without right-click: open RedaotZip from the Start Menu and choose **Archive files...** or **Archive a folder...**.

## Extracting

Right-click a `.zip` archive → **Extract with REDAOT**. On archives both options appear, one under the other: **Extract** and **Archive**.

1. The window shows the encryption type (AES-256, ZipCrypto or none) and how many files the archive contains.
2. **Extract to folder:** by default a new folder named after the archive, next to it. If the archive already contains a single folder, it is extracted right next to the archive, without an extra folder.
3. **Password:** type the password. For an unencrypted archive the field is disabled.
4. Click **Extract**. With a wrong password you get "Wrong password. Try again." and nothing is written to disk.
5. At the end the folder with the files opens (if "Open the folder after extracting" is checked).

Existing files are not overwritten: they are saved as `file (2).txt`. Check **Overwrite existing files** to replace them.

**Several archives at once:** select them all, right-click → Extract with REDAOT. They open one after another, and the password typed for the first is filled in for the next ones.

**Archives with hidden names** (`continut.zip`): RedaotZip extracts the inner files directly. With 7-Zip or WinRAR you first extract `continut.zip`, which then opens without a password.

**Protection:** entries with dangerous paths (e.g. `..\..\Windows\file`) are skipped, so an archive can never write outside the chosen folder. The program reports how many were skipped.

## Sending an archive to someone

The recipient needs a program that supports AES-256. The Windows tool is not enough.

| Program | Price | How to open the archive |
| --- | --- | --- |
| [RedaotZip](https://github.com/redaot/RedaotZip) | free | Right-click → Extract with REDAOT |
| [7-Zip](https://www.7-zip.org) | free | Right-click → 7-Zip → Extract Here, then the password |
| [PeaZip](https://peazip.github.io) | free | Right-click → PeaZip → Extract here |
| [WinRAR](https://www.win-rar.com) | paid, trial | Right-click → Extract Here |
| [WinZip](https://www.winzip.com) | paid | Double-click the archive |

If the recipient still tries Windows, they get an error, but they see the file **!!! CITESTE-MA - ARHIVA CRIPTATA.txt** in the archive. It is not encrypted and explains, in Romanian and English, which programs to install, with links. The same message is in the archive comment (shown by WinRAR).

**Send the password through a different channel than the archive** (e.g. archive by email, password by text message or phone).

## Security

AES-256 cannot be broken directly. The only realistic attack is guessing the password, so **the archive is only as safe as its password**.

| Protected | NOT protected |
| --- | --- |
| The content of every file (AES-256) | File names (only if you uncheck "Also hide file names") |
| Tampering: any change is detected when extracting (HMAC) | The approximate size of the archive |
| Every file has its own key (16-byte random salt) | The password, if it is short or easy to guess |

The WinZip AES format derives the key from the password with a fixed number of rounds (PBKDF2, 1,000 iterations), set by the standard so archives open in 7-Zip, WinRAR and the others. That is why a long password matters.

- At least **12 characters**, ideally 16 or more.
- Upper and lower case letters, digits and symbols, or a phrase of 4–5 unrelated words (e.g. `Lamp-Train-Cloud7-Boot`).
- No personal data and no accented characters.
- A different password for every important archive.

**If you forget the password, nobody can open the archive.** There is no recovery key.

The old **ZipCrypto** encryption is weak; RedaotZip never uses it for new archives, it only opens old ones.

## Troubleshooting

| Problem | Solution |
| --- | --- |
| The menu does not appear after setup | Restart the computer. If it still does not appear, run `C:\Program Files\RedaotZip\Diagnostic.cmd`: it creates `RedaotZip-diagnostic.txt` on the Desktop. Send that file to REDAOT. |
| "Windows blocked RedaotZip.exe" during setup | **Smart App Control** or the antivirus is blocking the program. Smart App Control: Windows Security → App & browser control → Smart App Control settings. Antivirus: Windows Security → Protection history → Allow. |
| The menu only appears under "Show more options" | The Windows 11 menu could not be registered; `RedaotZip-eroare-meniu.txt` on the Desktop has the details. Send it to REDAOT. |
| The menu disappeared after a Windows update | It is restored at the next start, or reinstall. |
| "Windows protected your PC" | More info → Run anyway. |
| Explorer says 0x80004005 on an archive | The archive is AES-256: open it with RedaotZip or 7-Zip. |
| "Wrong password" although it is right | Check Caps Lock, the keyboard layout and accented characters; tick "Show password". |
| "Authenticity check failed" | The archive was modified or damaged while being copied. |
| "...uses LZMA/BZip2 compression, which is not supported" | The archive was made with special 7-Zip settings. Open it with 7-Zip. |

Logs: `%LOCALAPPDATA%\RedaotZip\repair.log` (startup check) and `shell.log` (menu).

## Uninstalling and updating

**Uninstall:** **Settings → Apps → Installed apps → RedaotZip → Uninstall** (or `Dezinstaleaza.cmd`). This removes the menu, the startup check, the certificate, the shortcuts and the program folder. **Your archives are not deleted.**

**Update:** run the new version's Setup.exe over the old one. The current version is shown in every window, next to "made by REDAOT".
