# Building RedaotZip

RedaotZip is cross-compiled on Linux (tested on Ubuntu 24.04) with MinGW-w64. No Visual Studio is needed.

## Requirements

```bash
sudo apt install mingw-w64 g++-mingw-w64-x86-64 libz-mingw-w64-dev nsis osslsigncode 7zip openssl python3-pil
pip install markdown playwright   # optional: regenerates the PDF manuals from docs/*.md
```

## Build

```bash
./build.sh
```

Output in `build/`:

| File | What it is |
| --- | --- |
| `RedaotZip-x.y.z-Setup.exe` | Installer (upload this to GitHub Releases) |
| `RedaotZip-x.y.z-Windows.zip` | The same files without an installer (`Instaleaza.cmd`) |
| `RedaotZip-x.y.z-cod-sursa.zip` | Source archive |
| `RedaotZip-x.y.z-certificat-semnare-PRIVAT.zip` | Your signing key. **Never publish it.** |

## Signing certificate

The first build creates a self-signed code-signing certificate in `packaging/cert/` (ignored by git). Keep `REDAOT.pfx` and `pfx-password.txt` safe and put them back in `packaging/cert/` before building a new version: the menu packages of an update must be signed with the same certificate as the installed ones.

## New version

1. Change the version in `src/version.h` (`RZ_VER_*`) and `build.sh` (`APP_VERSION`).
2. Add an entry to `CHANGELOG.md` and `packaging/ISTORIC.txt`.
3. Run `./build.sh` and upload `RedaotZip-x.y.z-Setup.exe` to a new GitHub release.

## Project layout

| Path | Contents |
| --- | --- |
| `src/zipcore.*` | ZIP engine: WinZip AES-256 (AE-2), ZIP64, Deflate, zip-slip protection |
| `src/crypto_win.cpp` | AES / HMAC-SHA1 / PBKDF2 via Windows CNG (`bcrypt.dll`) |
| `src/crypto_ossl.cpp` | OpenSSL implementation, used only for tests on Linux |
| `src/main.cpp`, `src/app.rc` | Win32 user interface |
| `src/shellext.cpp` | Right-click menu commands (`IExplorerCommand`) |
| `src/i18n.*` | Romanian / English selection |
| `packaging/` | MSIX sparse-package manifests, installer scripts, NSIS script |
| `docs/` | Manuals (Markdown) and screenshots |
| `tests/cli.cpp` | Command-line test tool for the engine |

## Tests (Linux)

```bash
g++ -std=c++17 -O2 tests/cli.cpp src/i18n.cpp src/zipcore.cpp src/crypto_ctr.cpp src/crypto_ossl.cpp -lz -lcrypto -o build/cli
./build/cli a test.zip 'Password#123' 6 some-folder       # create
7z t -p'Password#123' test.zip                            # verify with 7-Zip
./build/cli x test.zip out 'Password#123'                 # extract
```
