#!/usr/bin/env python3
"""Construieste pachetul MSIX "sparse" (AppxManifest + blockmap + content types), il semneaza
cu certificatul REDAOT si semneaza si RedaotZip.exe / RedaotZipShell.dll.
Utilizare: make_msix.py <versiune> <folder_build>"""
import base64, hashlib, os, secrets, subprocess, sys, zipfile
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
ver, out = sys.argv[1], sys.argv[2]
out = os.path.abspath(out)
certdir = os.path.join(HERE, "cert")
os.makedirs(certdir, exist_ok=True)
pfx = os.path.join(certdir, "REDAOT.pfx")
pwfile = os.path.join(certdir, "pfx-password.txt")
cer = os.path.join(certdir, "REDAOT.cer")

def run(*a):
    subprocess.run(a, check=True)

# 1. certificat de semnare (se genereaza o singura data, apoi se refoloseste la versiunile noi)
if not os.path.exists(pfx):
    key = os.path.join(certdir, "key.pem"); crt = os.path.join(certdir, "cert.pem")
    cnf = os.path.join(certdir, "openssl.cnf")
    open(cnf, "w").write("""[req]
distinguished_name=dn
x509_extensions=ext
prompt=no
[dn]
CN=REDAOT
[ext]
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature
extendedKeyUsage=codeSigning
subjectKeyIdentifier=hash
""")
    run("openssl", "req", "-x509", "-newkey", "rsa:3072", "-sha256", "-nodes", "-days", "3650",
        "-keyout", key, "-out", crt, "-config", cnf)
    pw = secrets.token_urlsafe(18)
    open(pwfile, "w").write(pw)
    run("openssl", "pkcs12", "-export", "-inkey", key, "-in", crt, "-out", pfx, "-passout", "pass:" + pw,
        "-keypbe", "AES-256-CBC", "-certpbe", "AES-256-CBC", "-macalg", "sha256")
    run("openssl", "x509", "-in", crt, "-outform", "DER", "-out", cer)
    os.remove(key); os.remove(cnf)
pw = open(pwfile).read().strip()

# 2. continutul pachetului
icon = Image.open(os.path.join(ROOT, "assets", "icon512.png")).convert("RGBA")
def png(size):
    import io
    b = io.BytesIO(); icon.resize((size, size), Image.LANCZOS).save(b, "PNG"); return b.getvalue()
def write_appx_zip(path, entries):
    """Scrie ZIP-ul exact in structura folosita de MakeAppx (Microsoft msix-packaging, ZipObjectWriter):
    LFH v2.0 fara flaguri/extra, director central "made by" 4.5, apoi INTOTDEAUNA
    Zip64 End Of Central Directory + Zip64 Locator + EOCD cu valori 0xFFFF/0xFFFFFFFF."""
    import struct, zlib
    dos_time, dos_date = 0, ((2026 - 1980) << 9) | (1 << 5) | 1
    buf = bytearray(); cds = []
    for name, data in entries:
        n = name.encode("utf-8"); crc = zlib.crc32(data) & 0xFFFFFFFF; off = len(buf)
        buf += struct.pack("<IHHHHHIIIHH", 0x04034b50, 20, 0, 0, dos_time, dos_date, crc, len(data), len(data), len(n), 0)
        buf += n + data
        cds.append(struct.pack("<IHHHHHHIIIHHHHHII", 0x02014b50, 45, 20, 0, 0, dos_time, dos_date, crc,
                               len(data), len(data), len(n), 0, 0, 0, 0, 0, off) + n)
    cd_start = len(buf); cd = b"".join(cds); buf += cd
    z64 = len(buf)
    buf += struct.pack("<IQHHIIQQQQ", 0x06064b50, 44, 45, 45, 0, 0, len(entries), len(entries), len(cd), cd_start)
    buf += struct.pack("<IIQI", 0x07064b50, 0, z64, 1)
    buf += struct.pack("<IHHHHIIH", 0x06054b50, 0, 0, 0xFFFF, 0xFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0)
    open(path, "wb").write(bytes(buf))


def build_package(manifest_name, msix_name):
    manifest = open(os.path.join(HERE, manifest_name), encoding="utf-8").read().replace("@VERSION@", ver + ".0")
    files = [
        ("Assets/Square44x44Logo.png", png(44)),
        ("Assets/Square150x150Logo.png", png(150)),
        ("Assets/StoreLogo.png", png(50)),
        ("AppxManifest.xml", manifest.encode("utf-8")),   # ca la MakeAppx: manifestul dupa continut
    ]

    # 3. AppxBlockMap.xml (fisiere stocate necomprimat, blocuri de 64 KB cu SHA-256)
    bm = ['<?xml version="1.0" encoding="UTF-8" standalone="no"?>',
          '<BlockMap xmlns="http://schemas.microsoft.com/appx/2010/blockmap" HashMethod="http://www.w3.org/2001/04/xmlenc#sha256">']
    for name, data in files:
        lfh = 30 + len(name.encode("utf-8"))
        bm.append('<File Name="%s" Size="%d" LfhSize="%d">' % (name.replace("/", "\\"), len(data), lfh))
        for i in range(0, len(data), 65536):
            bm.append('<Block Hash="%s"/>' % base64.b64encode(hashlib.sha256(data[i:i + 65536]).digest()).decode())
        bm.append('</File>')
    bm.append('</BlockMap>')
    blockmap = "".join(bm).encode("utf-8")

    ctypes = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
              '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
              '<Default Extension="png" ContentType="image/png"/>'
              '<Override PartName="/AppxManifest.xml" ContentType="application/vnd.ms-appx.manifest+xml"/>'
              '<Override PartName="/AppxBlockMap.xml" ContentType="application/vnd.ms-appx.blockmap+xml"/>'
              '<Override PartName="/AppxSignature.p7x" ContentType="application/vnd.ms-appx.signature"/>'
              '</Types>').encode("utf-8")

    unsigned = os.path.join(out, msix_name + ".unsigned")
    write_appx_zip(unsigned, files + [("AppxBlockMap.xml", blockmap), ("[Content_Types].xml", ctypes)])

    # 4. semnare
    msix = os.path.join(out, msix_name)
    for f in (msix,):
        if os.path.exists(f): os.remove(f)
    run("osslsigncode", "sign", "-pkcs12", pfx, "-pass", pw, "-h", "sha256", "-in", unsigned, "-out", msix)
    os.remove(unsigned)
    with zipfile.ZipFile(msix) as z:
        bad = z.testzip()
        if bad: raise SystemExit("MSIX corupt dupa semnare: " + bad)
        names = z.namelist()
        for name, data in files:
            if z.read(name) != data: raise SystemExit("continut modificat: " + name)
        for info in z.infolist():
            if info.filename != "AppxSignature.p7x" and info.compress_type != zipfile.ZIP_STORED:
                raise SystemExit("intrare comprimata neasteptat: " + info.filename)
        if z.read("[Content_Types].xml") != ctypes: raise SystemExit("[Content_Types].xml modificat la semnare")
    print("MSIX verificat:", msix_name, names)

build_package("AppxManifest-arhivare.xml", "RedaotZip.msix")
build_package("AppxManifest-dezarhivare.xml", "RedaotZip-Dezarhivare.msix")
for name in ("RedaotZip.exe", "RedaotZipShell.dll"):
    p = os.path.join(out, name); tmp = p + ".signed"
    run("osslsigncode", "sign", "-pkcs12", pfx, "-pass", pw, "-h", "sha256", "-n", "RedaotZip",
        "-i", "https://redaot.local", "-in", p, "-out", tmp)
    os.replace(tmp, p)
import shutil
shutil.copy(cer, os.path.join(out, "REDAOT.cer"))
print("MSIX: gata")
