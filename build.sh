#!/usr/bin/env bash
# Compileaza RedaotZip pentru Windows x64 (MinGW-w64), construieste pachetul MSIX si il semneaza.
set -euo pipefail
cd "$(dirname "$0")"
APP_VERSION="1.0.7"
OUT=build/win
mkdir -p "$OUT"
CXX=x86_64-w64-mingw32-g++
WINDRES=x86_64-w64-mingw32-windres
CXXFLAGS="-std=c++17 -O2 -Wall -Wno-misleading-indentation -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00"
LDSTATIC="-static -static-libgcc -static-libstdc++ -s"

echo "[1/5] resurse"
$WINDRES -c 65001 -I src -i src/app.rc -o "$OUT/app.res.o"

echo "[2/5] RedaotZip.exe"
$CXX $CXXFLAGS -municode -mwindows src/main.cpp src/i18n.cpp src/zipcore.cpp src/crypto_ctr.cpp src/crypto_win.cpp "$OUT/app.res.o" \
  -o "$OUT/RedaotZip.exe" $LDSTATIC /usr/x86_64-w64-mingw32/lib/libz.a \
  -lbcrypt -lcomctl32 -lcomdlg32 -lole32 -lshell32 -lshlwapi -luuid -ladvapi32

echo "[3/5] RedaotZipShell.dll"
$CXX $CXXFLAGS -shared src/shellext.cpp src/i18n.cpp src/shell.def -o "$OUT/RedaotZipShell.dll" $LDSTATIC \
  -lole32 -lshell32 -lshlwapi -luuid -ladvapi32

echo "[4/5] test CLI (pentru verificari sub Wine)"
$CXX $CXXFLAGS -municode tests/cli_win.cpp src/i18n.cpp src/zipcore.cpp src/crypto_ctr.cpp src/crypto_win.cpp \
  -o build/cli_win.exe $LDSTATIC /usr/x86_64-w64-mingw32/lib/libz.a -lbcrypt -ladvapi32

echo "[5/5] pachet MSIX + semnare"
python3 packaging/make_msix.py "$APP_VERSION" "$OUT"
echo "OK"

echo "[doc] manuale PDF din docs/*.md"
if python3 -c "import markdown, playwright" 2>/dev/null; then
  python3 tools/md2pdf.py docs/MANUAL.ro.md "packaging/Manual RedaotZip.pdf" >/dev/null
  python3 tools/md2pdf.py docs/MANUAL.en.md "packaging/RedaotZip Manual (EN).pdf" >/dev/null
fi

echo "[dist] pachete de livrare"
DIST=build/dist; NAME="RedaotZip-$APP_VERSION"
rm -rf "$DIST"; mkdir -p "$DIST/$NAME/scripts"
cp "$OUT"/RedaotZip.exe "$OUT"/RedaotZipShell.dll "$OUT"/RedaotZip.msix "$OUT"/RedaotZip-Dezarhivare.msix "$OUT"/REDAOT.cer "$DIST/$NAME/"
printf '%s\r\n' "$APP_VERSION" > "$DIST/$NAME/version.txt"
cp packaging/Instaleaza.cmd packaging/Dezinstaleaza.cmd packaging/ISTORIC.txt "packaging/Manual RedaotZip.pdf" "packaging/RedaotZip Manual (EN).pdf" packaging/Diagnostic.cmd "$DIST/$NAME/"
sed "s/@VERSION@/v$APP_VERSION/; s/\$/\r/" packaging/CITESTE-MA.txt > "$DIST/$NAME/CITESTE-MA.txt"
for f in packaging/scripts/*.ps1; do sed 's/$/\r/' "$f" > "$DIST/$NAME/scripts/$(basename "$f")"; done
# installer Setup.exe (NSIS), semnat cu certificatul REDAOT
SETUP="build/RedaotZip-$APP_VERSION-Setup.exe"
(cd packaging && makensis -V2 -DVERSION="$APP_VERSION" -DSRCDIR="../$DIST/$NAME" -DOUTFILE="../$SETUP.unsigned" setup.nsi)
osslsigncode sign -pkcs12 packaging/cert/REDAOT.pfx -pass "$(cat packaging/cert/pfx-password.txt)" -h sha256 \
  -n "RedaotZip Setup" -i "https://redaot.local" -in "$SETUP.unsigned" -out "$SETUP" >/dev/null
rm -f "$SETUP.unsigned"
cp "$SETUP" "$DIST/$NAME/"
(cd "$DIST" && rm -f "../$NAME-Windows.zip" && 7z a -tzip -mx=9 "../$NAME-Windows.zip" "$NAME" >/dev/null)
# cod sursa (fara cheia privata a certificatului)
rm -f "build/$NAME-cod-sursa.zip"
7z a -tzip -mx=9 "build/$NAME-cod-sursa.zip" src tests docs tools assets/app.ico assets/icon512.png packaging build.sh README.md BUILDING.md CHANGELOG.md LICENSE \
   -xr'!cert' >/dev/null
# certificatul de semnare (PRIVAT - pastreaza-l doar tu)
rm -f "build/$NAME-certificat-semnare-PRIVAT.zip"
7z a -tzip "build/$NAME-certificat-semnare-PRIVAT.zip" packaging/cert/REDAOT.pfx packaging/cert/pfx-password.txt packaging/cert/REDAOT.cer packaging/cert/cert.pem >/dev/null
ls -la build/*.zip build/*.exe
