// Motor ZIP: scriere/citire, WinZip AES (AE-2), ZIP64, Deflate, ZipCrypto (doar citire).
#include "zipcore.h"
#include "crypto.h"
#include "version.h"
#include "i18n.h"

#include <zlib.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <set>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <utime.h>
#endif

namespace rz {

// Numele fisierelor puse in arhiva urmeaza limba programului; la citire le recunoastem pe toate.
static const char* kWarnRo  = "!!! CITESTE-MA - ARHIVA CRIPTATA.txt";
static const char* kWarnEn  = "!!! README - ENCRYPTED ARCHIVE.txt";
static const char* kInnerRo = "continut.zip";
static const char* kInnerEn = "content.zip";
const char* WarningFileName() { return TR(kWarnRo, kWarnEn); }
const char* NestedInnerName() { return TR(kInnerRo, kInnerEn); }
static bool IsWarningName(const std::string& n) { return n == kWarnRo || n == kWarnEn; }
static bool IsInnerName(const std::string& n) { return n == kInnerRo || n == kInnerEn; }
static const char* kNestedMarker = "[REDAOT-NESTED-1]";

static const size_t kBuf = 1 << 20; // 1 MB

// ---------------------------------------------------------------- utilitare cale
std::string PathToUtf8(const fs::path& p) { return p.u8string(); }
fs::path Utf8ToPath(const std::string& s) { return fs::u8path(s); }

#ifdef _WIN32
static std::wstring LongPathW(const fs::path& p) {
    std::wstring s = p.wstring();
    for (auto& c : s) if (c == L'/') c = L'\\';
    if (s.size() < 240 || s.rfind(L"\\\\?\\", 0) == 0) return s;
    if (s.rfind(L"\\\\", 0) == 0) return L"\\\\?\\UNC\\" + s.substr(2);
    if (s.size() > 2 && s[1] == L':') return L"\\\\?\\" + s;
    return s;
}
static fs::path LP(const fs::path& p) { return fs::path(LongPathW(p)); }
#else
static fs::path LP(const fs::path& p) { return p; }
#endif

// ---------------------------------------------------------------- fisier
class File {
public:
    File() = default;
    File(const File&) = delete;
    ~File() { Close(); }
    void Open(const fs::path& p, bool write) {
        path = p;
#ifdef _WIN32
        f = _wfopen(LongPathW(p).c_str(), write ? L"wb" : L"rb");
#else
        f = fopen(p.c_str(), write ? "wb" : "rb");
#endif
        if (!f) throw ZipError((write ? TR("Nu pot crea fișierul: ", "Cannot create the file: ") : TR("Nu pot deschide fișierul: ", "Cannot open the file: ")) + PathToUtf8(p));
        setvbuf(f, nullptr, _IOFBF, 1 << 18);
    }
    void Close() { if (f) { fclose(f); f = nullptr; } }
    size_t ReadSome(void* b, size_t n) { return fread(b, 1, n, f); }
    void Read(void* b, size_t n) {
        if (n && fread(b, 1, n, f) != n) throw ZipError(TR("Arhivă incompletă sau deteriorată.", "Incomplete or damaged archive."));
    }
    void Write(const void* b, size_t n) {
        if (n && fwrite(b, 1, n, f) != n) throw ZipError(TR("Eroare la scriere (disc plin?): ", "Write error (disk full?): ") + PathToUtf8(path));
    }
    void Seek(uint64_t pos) {
#ifdef _WIN32
        if (_fseeki64(f, (__int64)pos, SEEK_SET) != 0)
#else
        if (fseeko(f, (off_t)pos, SEEK_SET) != 0)
#endif
            throw ZipError(TR("Eroare de poziționare în fișier.", "File seek error."));
    }
    uint64_t Tell() {
#ifdef _WIN32
        return (uint64_t)_ftelli64(f);
#else
        return (uint64_t)ftello(f);
#endif
    }
    uint64_t Size() {
        uint64_t cur = Tell();
#ifdef _WIN32
        _fseeki64(f, 0, SEEK_END);
#else
        fseeko(f, 0, SEEK_END);
#endif
        uint64_t s = Tell(); Seek(cur); return s;
    }
    void Flush() { if (fflush(f) != 0) throw ZipError(TR("Eroare la scriere (disc plin?).", "Write error (disk full?).")); }
private:
    FILE* f = nullptr;
    fs::path path;
};

// ---------------------------------------------------------------- timp / atribute
struct FileMeta {
    uint64_t size = 0;
    bool isDir = false;
    uint32_t attrs = 0;        // atribute Windows
    uint64_t mtimeFt = 0;      // FILETIME (100ns din 1601, UTC)
    uint64_t ctimeFt = 0, atimeFt = 0;
};

static const uint64_t kFtEpoch = 116444736000000000ULL; // 1970-01-01 in FILETIME

static uint64_t UnixToFt(int64_t t) { return (uint64_t)t * 10000000ULL + kFtEpoch; }
static int64_t FtToUnix(uint64_t ft) { return (int64_t)((ft - kFtEpoch) / 10000000ULL); }

static bool GetMeta(const fs::path& p, FileMeta& m) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExW(LongPathW(p).c_str(), GetFileExInfoStandard, &d)) return false;
    m.isDir = (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    m.attrs = d.dwFileAttributes;
    m.size = ((uint64_t)d.nFileSizeHigh << 32) | d.nFileSizeLow;
    auto ft = [](FILETIME f) { return ((uint64_t)f.dwHighDateTime << 32) | f.dwLowDateTime; };
    m.mtimeFt = ft(d.ftLastWriteTime); m.ctimeFt = ft(d.ftCreationTime); m.atimeFt = ft(d.ftLastAccessTime);
    return true;
#else
    struct stat st;
    if (stat(p.c_str(), &st) != 0) return false;
    m.isDir = S_ISDIR(st.st_mode);
    m.attrs = m.isDir ? 0x10 : 0x20;
    m.size = m.isDir ? 0 : (uint64_t)st.st_size;
    m.mtimeFt = UnixToFt(st.st_mtime); m.atimeFt = UnixToFt(st.st_atime); m.ctimeFt = m.mtimeFt;
    return true;
#endif
}

// FILETIME UTC -> data/ora DOS (ora locala)
static uint32_t FtToDos(uint64_t ft) {
    int y, mo, d, h, mi, s;
#ifdef _WIN32
    FILETIME u, l; SYSTEMTIME st;
    u.dwLowDateTime = (DWORD)ft; u.dwHighDateTime = (DWORD)(ft >> 32);
    FileTimeToLocalFileTime(&u, &l); FileTimeToSystemTime(&l, &st);
    y = st.wYear; mo = st.wMonth; d = st.wDay; h = st.wHour; mi = st.wMinute; s = st.wSecond;
#else
    time_t t = (time_t)FtToUnix(ft); struct tm tmv; localtime_r(&t, &tmv);
    y = tmv.tm_year + 1900; mo = tmv.tm_mon + 1; d = tmv.tm_mday; h = tmv.tm_hour; mi = tmv.tm_min; s = tmv.tm_sec;
#endif
    if (y < 1980) { y = 1980; mo = 1; d = 1; h = mi = s = 0; }
    if (y > 2107) y = 2107;
    uint16_t date = (uint16_t)(((y - 1980) << 9) | (mo << 5) | d);
    uint16_t time = (uint16_t)((h << 11) | (mi << 5) | (s / 2));
    return ((uint32_t)date << 16) | time;
}

static uint64_t DosToFt(uint32_t dos) {
    uint16_t date = (uint16_t)(dos >> 16), time = (uint16_t)dos;
    int y = ((date >> 9) & 0x7f) + 1980, mo = (date >> 5) & 0xf, d = date & 0x1f;
    int h = (time >> 11) & 0x1f, mi = (time >> 5) & 0x3f, s = (time & 0x1f) * 2;
    if (mo < 1) mo = 1; if (d < 1) d = 1;
#ifdef _WIN32
    SYSTEMTIME st{}; st.wYear = (WORD)y; st.wMonth = (WORD)mo; st.wDay = (WORD)d;
    st.wHour = (WORD)h; st.wMinute = (WORD)mi; st.wSecond = (WORD)s;
    FILETIME l, u;
    if (!SystemTimeToFileTime(&st, &l) || !LocalFileTimeToFileTime(&l, &u)) return 0;
    return ((uint64_t)u.dwHighDateTime << 32) | u.dwLowDateTime;
#else
    struct tm tmv{}; tmv.tm_year = y - 1900; tmv.tm_mon = mo - 1; tmv.tm_mday = d;
    tmv.tm_hour = h; tmv.tm_min = mi; tmv.tm_sec = s; tmv.tm_isdst = -1;
    return UnixToFt((int64_t)mktime(&tmv));
#endif
}

static void SetMTime(const fs::path& p, uint64_t ft) {
    if (!ft) return;
#ifdef _WIN32
    HANDLE h = CreateFileW(LongPathW(p).c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    FILETIME f; f.dwLowDateTime = (DWORD)ft; f.dwHighDateTime = (DWORD)(ft >> 32);
    SetFileTime(h, nullptr, nullptr, &f);
    CloseHandle(h);
#else
    struct utimbuf ub; ub.actime = ub.modtime = (time_t)FtToUnix(ft); utime(p.c_str(), &ub);
#endif
}

// ---------------------------------------------------------------- little endian
static void P16(std::vector<uint8_t>& v, uint16_t x) { v.push_back((uint8_t)x); v.push_back((uint8_t)(x >> 8)); }
static void P32(std::vector<uint8_t>& v, uint32_t x) { for (int i = 0; i < 4; ++i) v.push_back((uint8_t)(x >> (8 * i))); }
static void P64(std::vector<uint8_t>& v, uint64_t x) { for (int i = 0; i < 8; ++i) v.push_back((uint8_t)(x >> (8 * i))); }
static uint16_t G16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t G32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint64_t G64(const uint8_t* p) { return (uint64_t)G32(p) | ((uint64_t)G32(p + 4) << 32); }

// ---------------------------------------------------------------- parola
int PasswordStrength(const std::string& pw) {
    if (pw.empty()) return 0;
    bool lo = false, up = false, dig = false, sym = false;
    size_t chars = 0;
    for (size_t i = 0; i < pw.size(); ++i) {
        unsigned char c = (unsigned char)pw[i];
        if ((c & 0xC0) != 0x80) ++chars;
        if (c >= 'a' && c <= 'z') lo = true; else if (c >= 'A' && c <= 'Z') up = true;
        else if (c >= '0' && c <= '9') dig = true; else sym = true;
    }
    int classes = lo + up + dig + sym;
    if (chars < 8) return 1;
    if (chars < 12) return classes >= 3 ? 2 : 1;
    if (chars < 16) return classes >= 3 ? 3 : 2;
    return classes >= 2 ? 4 : 3;
}

// WinZip AES: lungimi in functie de "strength" (1=128, 2=192, 3=256 biti)
static size_t AesKeyLen(int strength) { return strength == 1 ? 16 : strength == 2 ? 24 : 32; }
static size_t AesSaltLen(int strength) { return strength == 1 ? 8 : strength == 2 ? 12 : 16; }

struct AesKeys { std::vector<uint8_t> enc, mac; uint8_t pv[2]; };
static AesKeys DeriveAes(const std::string& pw, const uint8_t* salt, int strength) {
    size_t kl = AesKeyLen(strength);
    std::vector<uint8_t> dk(kl * 2 + 2);
    if (!Pbkdf2Sha1(pw, salt, AesSaltLen(strength), 1000, dk.data(), dk.size()))
        throw ZipError(TR("Derivarea cheii a eșuat.", "Key derivation failed."));
    AesKeys k;
    k.enc.assign(dk.begin(), dk.begin() + kl);
    k.mac.assign(dk.begin() + kl, dk.begin() + 2 * kl);
    k.pv[0] = dk[2 * kl]; k.pv[1] = dk[2 * kl + 1];
    return k;
}

static std::string LowerExt(const fs::path& p) {
    std::string e = p.extension().u8string();
    for (auto& c : e) c = (char)tolower((unsigned char)c);
    return e;
}

static bool IsIncompressible(const fs::path& p) {
    static const std::set<std::string> ext = {
        ".zip", ".7z", ".rar", ".gz", ".tgz", ".bz2", ".xz", ".zst", ".cab", ".jpg", ".jpeg", ".png",
        ".gif", ".webp", ".heic", ".avif", ".mp3", ".mp4", ".m4v", ".mkv", ".avi", ".mov", ".webm",
        ".m4a", ".aac", ".ogg", ".opus", ".flac", ".docx", ".xlsx", ".pptx", ".odt", ".ods", ".apk",
        ".jar", ".epub" };
    return ext.count(LowerExt(p)) > 0;
}

// ---------------------------------------------------------------- scanare intrari
struct InItem { fs::path src; std::string name; FileMeta meta; };

static void CollectItems(const std::vector<fs::path>& inputs, const fs::path& exclude, std::vector<InItem>& out) {
    std::error_code ec;
    fs::path exAbs = exclude.empty() ? fs::path() : fs::absolute(exclude, ec);
    for (const auto& in : inputs) {
        fs::path abs = fs::absolute(in, ec);
        FileMeta m;
        if (!GetMeta(abs, m)) throw ZipError(TR("Nu găsesc: ", "Not found: ") + PathToUtf8(in));
        fs::path base = abs.parent_path();
        auto add = [&](const fs::path& p, const FileMeta& mm) {
            if (!exAbs.empty() && fs::equivalent(p, exAbs, ec)) return;
            InItem it; it.src = p; it.meta = mm;
            it.name = p.lexically_relative(base).generic_u8string();
            if (it.name.empty() || it.name == ".") return;
            if (mm.isDir) it.name += "/";
            out.push_back(std::move(it));
        };
        add(abs, m);
        if (m.isDir) {
            auto opts = fs::directory_options::skip_permission_denied;
            for (auto it = fs::recursive_directory_iterator(LP(abs), opts, ec);
                 !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
                fs::path p = it->path();
#ifdef _WIN32
                std::wstring w = p.wstring();
                if (w.rfind(L"\\\\?\\UNC\\", 0) == 0) p = fs::path(L"\\\\" + w.substr(8));
                else if (w.rfind(L"\\\\?\\", 0) == 0) p = fs::path(w.substr(4));
#endif
                if (it->is_symlink(ec)) { it.disable_recursion_pending(); continue; }
                FileMeta mm;
                if (!GetMeta(p, mm)) continue;
#ifdef _WIN32
                if (mm.attrs & FILE_ATTRIBUTE_REPARSE_POINT) { it.disable_recursion_pending(); continue; }
#endif
                add(p, mm);
            }
        }
    }
}

ArchiveSummary ScanInputs(const std::vector<fs::path>& inputs) {
    std::vector<InItem> items;
    CollectItems(inputs, {}, items);
    ArchiveSummary s;
    for (auto& i : items) { if (i.meta.isDir) ++s.dirs; else { ++s.files; s.totalBytes += i.meta.size; } }
    return s;
}

// ---------------------------------------------------------------- scriere arhiva
struct CdEntry {
    std::string name;
    uint16_t version = 20, flags = 0, method = 0;
    uint32_t dosTime = 0, crc = 0, extAttr = 0;
    uint64_t csize = 0, usize = 0, offset = 0;
    bool z64local = false;
    std::vector<uint8_t> extra; // extra fara zip64 (AES, NTFS)
};

class ZipWriter {
public:
    ZipWriter(const fs::path& outPath, const ArchiveOptions& o, const ProgressFn& pf, uint64_t total)
        : opt(o), progress(pf), totalBytes(total) {
        out.Open(outPath, true);
        inBuf.resize(kBuf); outBuf.resize(kBuf + 1024);
    }

    void AddDir(const std::string& name, const FileMeta& m) {
        CdEntry e;
        e.name = name; e.flags = 0x0800; e.method = 0; e.version = 20;
        e.dosTime = FtToDos(m.mtimeFt); e.extAttr = 0x10;
        e.extra = NtfsExtra(m);
        e.offset = out.Tell();
        WriteLocalHeader(e);
        entries.push_back(std::move(e));
    }

    void AddMemory(const std::string& name, const std::string& data, bool encrypt) {
        FileMeta m; m.size = data.size();
        int64_t now = (int64_t)time(nullptr);
        m.mtimeFt = m.ctimeFt = m.atimeFt = UnixToFt(now);
        m.attrs = 0x20;
        size_t pos = 0;
        AddStream(name, m, encrypt, false, [&](uint8_t* b, size_t n) {
            size_t t = std::min(n, data.size() - pos); memcpy(b, data.data() + pos, t); pos += t; return t; });
    }

    void AddFile(const fs::path& src, const std::string& name, const FileMeta& m, bool encrypt) {
        File in; in.Open(src, false);
        AddStream(name, m, encrypt, IsIncompressible(src), [&](uint8_t* b, size_t n) { return in.ReadSome(b, n); });
    }

    void Finish(const std::string& comment) {
        uint64_t cdStart = out.Tell();
        for (auto& e : entries) WriteCentral(e);
        uint64_t cdEnd = out.Tell(), cdSize = cdEnd - cdStart;
        uint64_t n = entries.size();
        std::vector<uint8_t> v;
        bool z64 = n >= 0xFFFF || cdStart >= 0xFFFFFFFFULL || cdSize >= 0xFFFFFFFFULL;
        if (z64) {
            P32(v, 0x06064b50); P64(v, 44); P16(v, 45); P16(v, 45); P32(v, 0); P32(v, 0);
            P64(v, n); P64(v, n); P64(v, cdSize); P64(v, cdStart);
            P32(v, 0x07064b50); P32(v, 0); P64(v, cdEnd); P32(v, 1);
        }
        std::string c = comment.substr(0, 0xFFFF);
        P32(v, 0x06054b50); P16(v, 0); P16(v, 0);
        P16(v, z64 ? 0xFFFF : (uint16_t)n); P16(v, z64 ? 0xFFFF : (uint16_t)n);
        P32(v, cdSize >= 0xFFFFFFFFULL ? 0xFFFFFFFF : (uint32_t)cdSize);
        P32(v, cdStart >= 0xFFFFFFFFULL ? 0xFFFFFFFF : (uint32_t)cdStart);
        P16(v, (uint16_t)c.size());
        out.Write(v.data(), v.size());
        out.Write(c.data(), c.size());
        out.Flush();
        out.Close();
    }

    uint64_t doneBytes = 0;

private:
    File out;
    ArchiveOptions opt;
    ProgressFn progress;
    uint64_t totalBytes;
    std::vector<CdEntry> entries;
    std::vector<uint8_t> inBuf, outBuf;

    static std::vector<uint8_t> NtfsExtra(const FileMeta& m) {
        std::vector<uint8_t> v;
        if (!m.mtimeFt) return v;
        P16(v, 0x000A); P16(v, 32); P32(v, 0); P16(v, 0x0001); P16(v, 24);
        P64(v, m.mtimeFt); P64(v, m.atimeFt ? m.atimeFt : m.mtimeFt); P64(v, m.ctimeFt ? m.ctimeFt : m.mtimeFt);
        return v;
    }

    void WriteLocalHeader(const CdEntry& e) {
        std::vector<uint8_t> v;
        std::vector<uint8_t> ex;
        if (e.z64local) { P16(ex, 0x0001); P16(ex, 16); P64(ex, e.usize); P64(ex, e.csize); }
        ex.insert(ex.end(), e.extra.begin(), e.extra.end());
        P32(v, 0x04034b50); P16(v, e.version); P16(v, e.flags); P16(v, e.method);
        P32(v, e.dosTime); P32(v, e.crc);
        P32(v, e.z64local ? 0xFFFFFFFF : (uint32_t)e.csize);
        P32(v, e.z64local ? 0xFFFFFFFF : (uint32_t)e.usize);
        P16(v, (uint16_t)e.name.size()); P16(v, (uint16_t)ex.size());
        out.Write(v.data(), v.size());
        out.Write(e.name.data(), e.name.size());
        out.Write(ex.data(), ex.size());
    }

    void WriteCentral(const CdEntry& e) {
        bool bigU = e.z64local || e.usize >= 0xFFFFFFFFULL;
        bool bigC = e.z64local || e.csize >= 0xFFFFFFFFULL;
        bool bigO = e.offset >= 0xFFFFFFFFULL;
        std::vector<uint8_t> ex;
        if (bigU || bigC || bigO) {
            std::vector<uint8_t> z;
            if (bigU) P64(z, e.usize);
            if (bigC) P64(z, e.csize);
            if (bigO) P64(z, e.offset);
            P16(ex, 0x0001); P16(ex, (uint16_t)z.size()); ex.insert(ex.end(), z.begin(), z.end());
        }
        ex.insert(ex.end(), e.extra.begin(), e.extra.end());
        uint16_t ver = e.version;
        if ((bigU || bigC || bigO) && ver < 45) ver = 45;
        std::vector<uint8_t> v;
        P32(v, 0x02014b50); P16(v, (uint16_t)(0x0000 | 63)); P16(v, ver); P16(v, e.flags); P16(v, e.method);
        P32(v, e.dosTime); P32(v, e.crc);
        P32(v, bigC ? 0xFFFFFFFF : (uint32_t)e.csize);
        P32(v, bigU ? 0xFFFFFFFF : (uint32_t)e.usize);
        P16(v, (uint16_t)e.name.size()); P16(v, (uint16_t)ex.size()); P16(v, 0);
        P16(v, 0); P16(v, 0); P32(v, e.extAttr);
        P32(v, bigO ? 0xFFFFFFFF : (uint32_t)e.offset);
        out.Write(v.data(), v.size());
        out.Write(e.name.data(), e.name.size());
        out.Write(ex.data(), ex.size());
    }

    template <class ReadFn>
    void AddStream(const std::string& name, const FileMeta& m, bool encrypt, bool incompressible, ReadFn readFn) {
        if (name.size() > 0xFFFF) throw ZipError(TR("Nume de fișier prea lung: ", "File name too long: ") + name);
        CdEntry e;
        e.name = name;
        e.dosTime = FtToDos(m.mtimeFt);
        e.extAttr = m.attrs & 0xFFFF & ~0x10u;
        if (!e.extAttr) e.extAttr = 0x20;
        e.z64local = m.size >= 0xF0000000ULL;
        uint16_t actual = (opt.level == 0 || incompressible || m.size == 0) ? 0 : 8;
        e.flags = 0x0800;
        if (actual == 8) {
            if (opt.level >= 8) e.flags |= 0x02; else if (opt.level <= 2) e.flags |= 0x04;
        }
        if (encrypt) {
            e.flags |= 0x01; e.method = 99; e.version = 51;
            std::vector<uint8_t> a; P16(a, 0x9901); P16(a, 7); P16(a, 2); a.push_back('A'); a.push_back('E');
            a.push_back(3); P16(a, actual);
            e.extra = a;
        } else {
            e.method = actual; e.version = e.z64local ? 45 : 20;
        }
        auto nt = NtfsExtra(m); e.extra.insert(e.extra.end(), nt.begin(), nt.end());
        e.offset = out.Tell();
        WriteLocalHeader(e);
        uint64_t dataStart = out.Tell();

        std::unique_ptr<AesCtrWz> ctr;
        std::unique_ptr<HmacSha1> mac;
        if (encrypt) {
            uint8_t salt[16];
            if (!RandomBytes(salt, sizeof(salt))) throw ZipError(TR("Generatorul de numere aleatoare a eșuat.", "The random number generator failed."));
            AesKeys k = DeriveAes(opt.password, salt, 3);
            ctr.reset(new AesCtrWz(k.enc.data(), k.enc.size()));
            mac.reset(new HmacSha1(k.mac.data(), k.mac.size()));
            out.Write(salt, sizeof(salt));
            out.Write(k.pv, 2);
        }

        auto emit = [&](uint8_t* b, size_t n) {
            if (!n) return;
            if (ctr) { ctr->Process(b, n); mac->Update(b, n); }
            out.Write(b, n);
        };

        uint32_t crc = crc32(0L, Z_NULL, 0);
        uint64_t usize = 0;
        z_stream zs{};
        if (actual == 8) {
            if (deflateInit2(&zs, opt.level, Z_DEFLATED, -15, 9, Z_DEFAULT_STRATEGY) != Z_OK)
                throw ZipError(TR("Inițializare compresie eșuată.", "Compression initialization failed."));
        }
        try {
            for (;;) {
                size_t n = readFn(inBuf.data(), kBuf);
                if (n) {
                    crc = crc32(crc, inBuf.data(), (uInt)n);
                    usize += n;
                }
                bool last = n == 0;
                if (actual == 8) {
                    zs.next_in = inBuf.data(); zs.avail_in = (uInt)n;
                    do {
                        zs.next_out = outBuf.data(); zs.avail_out = (uInt)outBuf.size();
                        int r = deflate(&zs, last ? Z_FINISH : Z_NO_FLUSH);
                        if (r == Z_STREAM_ERROR) throw ZipError(TR("Eroare de compresie.", "Compression error."));
                        emit(outBuf.data(), outBuf.size() - zs.avail_out);
                    } while (zs.avail_out == 0);
                } else {
                    emit(inBuf.data(), n);
                }
                doneBytes += n;
                if (progress && !progress(doneBytes, totalBytes, name)) throw Cancelled();
                if (last) break;
            }
        } catch (...) {
            if (actual == 8) deflateEnd(&zs);
            throw;
        }
        if (actual == 8) deflateEnd(&zs);

        if (encrypt) {
            uint8_t h[20]; mac->Final(h);
            out.Write(h, 10);
        }
        uint64_t end = out.Tell();
        e.csize = end - dataStart;
        e.usize = usize;
        e.crc = encrypt ? 0 : crc;          // AE-2: CRC nu se stocheaza (integritatea e data de HMAC)
        if (!e.z64local && (e.usize >= 0xFFFFFFFFULL || e.csize >= 0xFFFFFFFFULL))
            throw ZipError(TR("Fișierul și-a schimbat dimensiunea în timpul arhivării: ", "The file changed size while being archived: ") + name);
        // completam antetul local
        out.Seek(e.offset + 14);
        std::vector<uint8_t> v;
        P32(v, e.crc);
        P32(v, e.z64local ? 0xFFFFFFFF : (uint32_t)e.csize);
        P32(v, e.z64local ? 0xFFFFFFFF : (uint32_t)e.usize);
        out.Write(v.data(), v.size());
        if (e.z64local) {
            out.Seek(e.offset + 30 + e.name.size() + 4);
            v.clear(); P64(v, e.usize); P64(v, e.csize);
            out.Write(v.data(), v.size());
        }
        out.Seek(end);
        entries.push_back(std::move(e));
    }
};

static std::string WarningText() {
    const std::string inner = NestedInnerName();
    std::string ro =
        "ATENȚIE: ACEASTĂ ARHIVĂ ESTE CRIPTATĂ CU AES-256\r\n"
        "================================================\r\n\r\n"
        "Fișierele din această arhivă sunt protejate cu parolă și criptate cu AES-256.\r\n\r\n"
        "Utilitarul ZIP inclus în Windows NU poate deschide arhive criptate AES-256:\r\n"
        "afișează o eroare (de exemplu 0x80004005) și nu cere parola.\r\n\r\n"
        "Pentru a dezarhiva, instalează unul dintre următoarele programe:\r\n\r\n"
        "   - RedaotZip  (gratuit)   " RZ_HOMEPAGE "\r\n"
        "   - 7-Zip      (gratuit)   https://www.7-zip.org\r\n"
        "   - PeaZip     (gratuit)   https://peazip.github.io\r\n"
        "   - WinRAR                 https://www.win-rar.com\r\n"
        "   - WinZip                 https://www.winzip.com\r\n\r\n"
        "Apoi click dreapta pe arhivă, dezarhiveaz-o și introdu parola\r\n"
        "primită de la expeditor.\r\n\r\n"
        "Dacă în arhivă vezi doar fișierul \"" + inner + "\", dezarhivează-l și pe el:\r\n"
        "acela conține fișierele propriu-zise (numele lor au fost ascunse).\r\n";
    std::string en =
        "WARNING: THIS ARCHIVE IS ENCRYPTED WITH AES-256\r\n"
        "===============================================\r\n\r\n"
        "The files in this archive are password-protected and encrypted with AES-256.\r\n\r\n"
        "The ZIP tool built into Windows CANNOT open AES-256 encrypted archives:\r\n"
        "it shows an error (for example 0x80004005) and never asks for the password.\r\n\r\n"
        "To extract it, install one of these programs:\r\n\r\n"
        "   - RedaotZip  (free)   " RZ_HOMEPAGE "\r\n"
        "   - 7-Zip      (free)   https://www.7-zip.org\r\n"
        "   - PeaZip     (free)   https://peazip.github.io\r\n"
        "   - WinRAR              https://www.win-rar.com\r\n"
        "   - WinZip              https://www.winzip.com\r\n\r\n"
        "Then right-click the archive, extract it and enter the password\r\n"
        "you received from the sender.\r\n\r\n"
        "If you only see the file \"" + inner + "\", extract it too:\r\n"
        "it holds the actual files (their names were hidden).\r\n";
    const std::string sep = "\r\n------------------------------------------------------------------\r\n\r\n";
    std::string t = "\xEF\xBB\xBF";
    t += IsRo() ? ro + sep + en : en + sep + ro;
    t += sep;
    t += TR("Creată cu ", "Created with ");
    t += RZ_APP_NAME " v" RZ_VERSION_STR " - made by REDAOT\r\n";
    return t;
}

static std::string ArchiveComment(bool nested) {
    const std::string ro = "Arhiva criptata AES-256. Deschide-o cu RedaotZip (" RZ_HOMEPAGE "), 7-Zip, PeaZip, WinRAR sau WinZip.";
    const std::string en = "AES-256 encrypted archive. Open it with RedaotZip (" RZ_HOMEPAGE "), 7-Zip, PeaZip, WinRAR or WinZip.";
    std::string c = (IsRo() ? ro + " / " + en : en + " / " + ro) + " - " RZ_APP_NAME " v" RZ_VERSION_STR " - made by REDAOT";
    if (nested) c += std::string(" ") + kNestedMarker;
    return c;
}

static fs::path TempSibling(const fs::path& target, const char* tag) {
    uint8_t r[6]; RandomBytes(r, sizeof(r));
    char hex[16]; snprintf(hex, sizeof(hex), "%02x%02x%02x%02x%02x%02x", r[0], r[1], r[2], r[3], r[4], r[5]);
    return target.parent_path() / Utf8ToPath(std::string(".redaot-") + tag + "-" + hex + ".tmp");
}

static void RemoveQuiet(const fs::path& p) { std::error_code ec; fs::remove(LP(p), ec); }

void CreateArchive(const std::vector<fs::path>& inputs, const fs::path& output,
                   const ArchiveOptions& opt, const ProgressFn& progress) {
    if (inputs.empty()) throw ZipError(TR("Nu ai selectat nimic de arhivat.", "Nothing was selected to archive."));
    std::vector<InItem> items;
    CollectItems(inputs, output, items);
    uint64_t total = 0;
    for (auto& i : items) if (!i.meta.isDir) total += i.meta.size;
    bool encrypt = !opt.password.empty();

    // scriem intai intr-un fisier temporar, apoi redenumim (nu lasam arhive pe jumatate)
    fs::path tmp = TempSibling(output, "zip");
    fs::path innerTmp;
    try {
        if (opt.hideNames && encrypt) {
            innerTmp = TempSibling(output, "inner");
            {
                ArchiveOptions io = opt; io.password.clear(); io.addWarningFile = false;
                ZipWriter w(innerTmp, io, progress, total * 2);
                for (auto& i : items) {
                    if (i.meta.isDir) w.AddDir(i.name, i.meta);
                    else w.AddFile(i.src, i.name, i.meta, false);
                }
                w.Finish(RZ_APP_NAME " v" RZ_VERSION_STR " - made by REDAOT");
            }
            FileMeta im; GetMeta(innerTmp, im);
            ArchiveOptions oo = opt; oo.level = 0; // continutul e deja comprimat
            ZipWriter w(tmp, oo, [&](uint64_t d, uint64_t, const std::string&) {
                return progress ? progress(total + (im.size ? d * total / im.size : 0), total * 2, NestedInnerName()) : true;
            }, im.size);
            if (opt.addWarningFile) w.AddMemory(WarningFileName(), WarningText(), false);
            w.AddFile(innerTmp, NestedInnerName(), im, true);
            w.Finish(ArchiveComment(true));
            RemoveQuiet(innerTmp);
        } else {
            ZipWriter w(tmp, opt, progress, total);
            if (encrypt && opt.addWarningFile) w.AddMemory(WarningFileName(), WarningText(), false);
            for (auto& i : items) {
                if (i.meta.isDir) w.AddDir(i.name, i.meta);
                else w.AddFile(i.src, i.name, i.meta, encrypt);
            }
            w.Finish(encrypt ? ArchiveComment(false) : std::string(RZ_APP_NAME " v" RZ_VERSION_STR " - made by REDAOT"));
        }
        std::error_code ec;
        fs::remove(LP(output), ec);
        fs::rename(LP(tmp), LP(output), ec);
        if (ec) throw ZipError(TR("Nu pot salva arhiva: ", "Cannot save the archive: ") + PathToUtf8(output));
    } catch (...) {
        if (!innerTmp.empty()) RemoveQuiet(innerTmp);
        RemoveQuiet(tmp);
        throw;
    }
}

// ---------------------------------------------------------------- citire arhiva
struct ZEntry {
    std::string name;
    uint16_t flags = 0, method = 0;
    uint32_t dosTime = 0, crc = 0, extAttr = 0;
    uint64_t csize = 0, usize = 0, offset = 0, mtimeFt = 0;
    bool aes = false; int aesStrength = 0; uint16_t aesVersion = 0, aesActual = 0;
    bool isDir() const { return !name.empty() && (name.back() == '/' || name.back() == '\\'); }
    bool encrypted() const { return flags & 1; }
    uint16_t realMethod() const { return aes ? aesActual : method; }
};

static std::string Cp437ToUtf8(const std::string& s) {
#ifdef _WIN32
    int n = MultiByteToWideChar(437, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0); MultiByteToWideChar(437, 0, s.data(), (int)s.size(), &w[0], n);
    int m = WideCharToMultiByte(CP_UTF8, 0, w.data(), n, nullptr, 0, nullptr, nullptr);
    std::string o(m, 0); WideCharToMultiByte(CP_UTF8, 0, w.data(), n, &o[0], m, nullptr, nullptr);
    return o;
#else
    std::string o;
    for (unsigned char c : s) { if (c < 0x80) o += (char)c; else { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); } }
    return o;
#endif
}

class ZipReader {
public:
    std::vector<ZEntry> entries;
    std::string comment;
    File f;
    uint64_t fileSize = 0;

    explicit ZipReader(const fs::path& zpath) {
        f.Open(zpath, false);
        fileSize = f.Size();
        if (fileSize < 22) throw ZipError(TR("Fișierul nu este o arhivă ZIP validă.", "The file is not a valid ZIP archive."));
        size_t tail = (size_t)std::min<uint64_t>(fileSize, 65557);
        std::vector<uint8_t> b(tail);
        f.Seek(fileSize - tail); f.Read(b.data(), tail);
        int64_t pos = -1;
        for (int64_t i = (int64_t)tail - 22; i >= 0; --i) {
            if (G32(&b[i]) == 0x06054b50 && (size_t)i + 22 + G16(&b[i + 20]) <= tail) { pos = i; break; }
        }
        if (pos < 0) throw ZipError(TR("Fișierul nu este o arhivă ZIP validă (sau este deteriorat).", "The file is not a valid ZIP archive (or it is damaged)."));
        const uint8_t* e = &b[pos];
        uint64_t count = G16(e + 10), cdSize = G32(e + 12), cdOff = G32(e + 16);
        comment.assign((const char*)e + 22, G16(e + 20));
        uint64_t eocdAbs = fileSize - tail + pos;
        if (eocdAbs >= 20) {
            uint8_t loc[20]; f.Seek(eocdAbs - 20); f.Read(loc, 20);
            if (G32(loc) == 0x07064b50) {
                uint64_t z64off = G64(loc + 8);
                uint8_t z[56]; f.Seek(z64off); f.Read(z, 56);
                if (G32(z) == 0x06064b50) { count = G64(z + 32); cdSize = G64(z + 40); cdOff = G64(z + 48); }
            }
        }
        if (cdOff + cdSize > fileSize) throw ZipError(TR("Arhivă deteriorată (directorul central lipsește).", "Damaged archive (central directory missing)."));
        if (cdSize > (1ULL << 31)) throw ZipError(TR("Director central prea mare.", "Central directory too large."));
        std::vector<uint8_t> cd((size_t)cdSize);
        f.Seek(cdOff); f.Read(cd.data(), cd.size());
        size_t p = 0;
        for (uint64_t i = 0; i < count; ++i) {
            if (p + 46 > cd.size() || G32(&cd[p]) != 0x02014b50) throw ZipError(TR("Arhivă deteriorată (intrare invalidă).", "Damaged archive (invalid entry)."));
            const uint8_t* c = &cd[p];
            ZEntry z;
            z.flags = G16(c + 8); z.method = G16(c + 10); z.dosTime = G32(c + 12); z.crc = G32(c + 16);
            z.csize = G32(c + 20); z.usize = G32(c + 24);
            uint16_t nl = G16(c + 28), xl = G16(c + 30), cl = G16(c + 32);
            z.extAttr = G32(c + 38); z.offset = G32(c + 42);
            if (p + 46 + nl + xl + cl > cd.size()) throw ZipError(TR("Arhivă deteriorată.", "Damaged archive."));
            std::string rawName((const char*)c + 46, nl);
            const uint8_t* x = c + 46 + nl; const uint8_t* xe = x + xl;
            std::string uniName;
            while (x + 4 <= xe) {
                uint16_t id = G16(x), sz = G16(x + 2);
                const uint8_t* d = x + 4;
                if (d + sz > xe) break;
                if (id == 0x0001) {
                    const uint8_t* q = d;
                    if (z.usize == 0xFFFFFFFF && q + 8 <= d + sz) { z.usize = G64(q); q += 8; }
                    if (z.csize == 0xFFFFFFFF && q + 8 <= d + sz) { z.csize = G64(q); q += 8; }
                    if (z.offset == 0xFFFFFFFF && q + 8 <= d + sz) { z.offset = G64(q); q += 8; }
                } else if (id == 0x9901 && sz >= 7) {
                    z.aes = true; z.aesVersion = G16(d); z.aesStrength = d[4]; z.aesActual = G16(d + 5);
                } else if (id == 0x000A && sz >= 32) {
                    const uint8_t* q = d + 4;
                    while (q + 4 <= d + sz) {
                        uint16_t t = G16(q), ts = G16(q + 2);
                        if (t == 1 && ts >= 8 && q + 12 <= d + sz) z.mtimeFt = G64(q + 4);
                        q += 4 + ts;
                    }
                } else if (id == 0x7075 && sz > 5 && d[0] == 1) {
                    uint32_t ncrc = G32(d + 1);
                    if (ncrc == crc32(0, (const Bytef*)rawName.data(), (uInt)rawName.size()))
                        uniName.assign((const char*)d + 5, sz - 5);
                }
                x = d + sz;
            }
            if (z.flags & 0x0800) z.name = rawName;
            else if (!uniName.empty()) z.name = uniName;
            else {
                bool ascii = std::all_of(rawName.begin(), rawName.end(), [](char ch) { return (unsigned char)ch < 0x80; });
                z.name = ascii ? rawName : Cp437ToUtf8(rawName);
            }
            entries.push_back(std::move(z));
            p += 46 + nl + xl + cl;
        }
    }

    uint64_t DataOffset(const ZEntry& z) {
        uint8_t h[30]; f.Seek(z.offset); f.Read(h, 30);
        if (G32(h) != 0x04034b50) throw ZipError(TR("Arhivă deteriorată (antet local invalid): ", "Damaged archive (invalid local header): ") + z.name);
        return z.offset + 30 + G16(h + 26) + G16(h + 28);
    }
};

// ---------------------------------------------------------------- ZipCrypto (doar citire, pentru arhive vechi)
class ZipCryptoDec {
public:
    explicit ZipCryptoDec(const std::string& pw) {
        k0 = 0x12345678; k1 = 0x23456789; k2 = 0x34567890;
        for (unsigned char c : pw) Update(c);
    }
    uint8_t Dec(uint8_t c) {
        uint16_t t = (uint16_t)(k2 | 2);
        uint8_t p = c ^ (uint8_t)((t * (t ^ 1)) >> 8);
        Update(p); return p;
    }
    void Buf(uint8_t* b, size_t n) { for (size_t i = 0; i < n; ++i) b[i] = Dec(b[i]); }
private:
    uint32_t k0, k1, k2;
    static uint32_t Crc1(uint32_t crc, uint8_t b) {
        static uint32_t table[256]; static bool init = false;
        if (!init) { for (uint32_t i = 0; i < 256; ++i) { uint32_t c = i; for (int j = 0; j < 8; ++j) c = c & 1 ? 0xEDB88320 ^ (c >> 1) : c >> 1; table[i] = c; } init = true; }
        return table[(crc ^ b) & 0xff] ^ (crc >> 8);
    }
    void Update(uint8_t c) {
        k0 = Crc1(k0, c); k1 = (k1 + (k0 & 0xff)) * 134775813 + 1; k2 = Crc1(k2, (uint8_t)(k1 >> 24));
    }
};

// ---------------------------------------------------------------- cale sigura
static bool SafeRelative(const std::string& name, std::string& out) {
    std::vector<std::string> parts;
    std::string cur;
    auto flush = [&]() {
        if (cur.empty() || cur == ".") { cur.clear(); return true; }
        if (cur == "..") return false;
        parts.push_back(cur); cur.clear(); return true;
    };
    for (char c : name) {
        if (c == '/' || c == '\\') { if (!flush()) return false; }
        else cur += c;
    }
    if (!flush()) return false;
    if (!parts.empty() && parts[0].size() == 2 && parts[0][1] == ':') parts.erase(parts.begin()); // C:
    static const char* reserved[] = { "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6",
                                      "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7",
                                      "LPT8", "LPT9" };
    out.clear();
    for (auto& p : parts) {
        for (auto& c : p) {
            unsigned char u = (unsigned char)c;
            if (u < 32 || c == '<' || c == '>' || c == ':' || c == '"' || c == '|' || c == '?' || c == '*') c = '_';
        }
        while (!p.empty() && (p.back() == ' ' || p.back() == '.')) p.pop_back();
        if (p.empty()) p = "_";
        std::string base = p.substr(0, p.find('.'));
        for (auto& c : base) c = (char)toupper((unsigned char)c);
        for (auto r : reserved) if (base == r) { p = "_" + p; break; }
        if (!out.empty()) out += '/';
        out += p;
    }
    return !out.empty();
}

static fs::path UniqueName(const fs::path& p) {
    std::error_code ec;
    if (!fs::exists(LP(p), ec)) return p;
    fs::path stem = p.stem(), ext = p.extension(), dir = p.parent_path();
    for (int i = 2; i < 100000; ++i) {
        fs::path c = dir / Utf8ToPath(stem.u8string() + " (" + std::to_string(i) + ")" + ext.u8string());
        if (!fs::exists(LP(c), ec)) return c;
    }
    return p;
}

static const char* MethodName(uint16_t m) {
    switch (m) { case 9: return "Deflate64"; case 12: return "BZip2"; case 14: return "LZMA"; case 93: return "Zstandard";
                 case 95: return "XZ"; case 98: return "PPMd"; default: return TR("necunoscută", "unknown"); }
}

static void CheckSupported(const ZEntry& z) {
    if (z.encrypted() && (z.flags & 0x40))
        throw ZipError(TR("Arhiva folosește criptarea PKWARE „Strong Encryption”, care nu este suportată. Încearcă cu 7-Zip sau WinZip.", "The archive uses PKWARE \"Strong Encryption\", which is not supported. Try 7-Zip or WinZip."));
    if (z.aes && z.aesStrength >= 1 && z.aesStrength <= 3) {}
    else if (z.aes) throw ZipError(TR("Variantă AES necunoscută în: ", "Unknown AES variant in: ") + z.name);
    uint16_t m = z.realMethod();
    if (m != 0 && m != 8)
        throw ZipError(std::string(TR("Fișierul „", "The file \"")) + z.name + TR("” folosește compresia ", "\" uses ") + MethodName(m) +
                       TR(", care nu este suportată. Dezarhivează-l cu 7-Zip.", " compression, which is not supported. Extract it with 7-Zip."));
}

// verifica parola pe prima intrare criptata, inainte sa scriem ceva pe disc
static void VerifyPassword(ZipReader& r, const ZEntry& z, const std::string& pw) {
    if (pw.empty()) throw WrongPassword(TR("Arhiva este criptată. Introdu parola.", "The archive is encrypted. Enter the password."));
    uint64_t off = r.DataOffset(z);
    r.f.Seek(off);
    if (z.aes) {
        size_t sl = AesSaltLen(z.aesStrength);
        uint8_t salt[16], pv[2];
        r.f.Read(salt, sl); r.f.Read(pv, 2);
        AesKeys k = DeriveAes(pw, salt, z.aesStrength);
        if (k.pv[0] != pv[0] || k.pv[1] != pv[1]) throw WrongPassword(TR("Parolă greșită.", "Wrong password."));
    } else {
        uint8_t h[12]; r.f.Read(h, 12);
        ZipCryptoDec d(pw); d.Buf(h, 12);
        uint8_t check = (z.flags & 0x08) ? (uint8_t)((z.dosTime >> 8) & 0xff) : (uint8_t)(z.crc >> 24);
        if (h[11] != check) throw WrongPassword(TR("Parolă greșită.", "Wrong password."));
    }
}

static void ExtractEntry(ZipReader& r, const ZEntry& z, const fs::path& target, const std::string& pw,
                         std::vector<uint8_t>& inBuf, std::vector<uint8_t>& outBuf,
                         uint64_t& done, uint64_t total, const ProgressFn& progress) {
    uint64_t off = r.DataOffset(z);
    r.f.Seek(off);
    uint64_t remaining = z.csize;
    std::unique_ptr<AesCtrWz> ctr; std::unique_ptr<HmacSha1> mac; std::unique_ptr<ZipCryptoDec> zc;
    if (z.encrypted()) {
        if (z.aes) {
            size_t sl = AesSaltLen(z.aesStrength);
            if (remaining < sl + 2 + 10) throw ZipError(TR("Arhivă deteriorată: ", "Damaged archive: ") + z.name);
            uint8_t salt[16], pv[2];
            r.f.Read(salt, sl); r.f.Read(pv, 2);
            AesKeys k = DeriveAes(pw, salt, z.aesStrength);
            if (k.pv[0] != pv[0] || k.pv[1] != pv[1]) throw WrongPassword(TR("Parolă greșită.", "Wrong password."));
            ctr.reset(new AesCtrWz(k.enc.data(), k.enc.size()));
            mac.reset(new HmacSha1(k.mac.data(), k.mac.size()));
            remaining -= sl + 2 + 10;
        } else {
            if (remaining < 12) throw ZipError(TR("Arhivă deteriorată: ", "Damaged archive: ") + z.name);
            uint8_t h[12]; r.f.Read(h, 12);
            zc.reset(new ZipCryptoDec(pw)); zc->Buf(h, 12);
            uint8_t check = (z.flags & 0x08) ? (uint8_t)((z.dosTime >> 8) & 0xff) : (uint8_t)(z.crc >> 24);
            if (h[11] != check) throw WrongPassword(TR("Parolă greșită.", "Wrong password."));
            remaining -= 12;
        }
    }
    uint16_t method = z.realMethod();
    File out; out.Open(target, true);
    uint32_t crc = crc32(0L, Z_NULL, 0);
    uint64_t written = 0;
    z_stream zs{};
    bool zinit = false;
    try {
        if (method == 8) { if (inflateInit2(&zs, -15) != Z_OK) throw ZipError(TR("Inițializare decompresie eșuată.", "Decompression initialization failed.")); zinit = true; }
        bool streamEnd = false;
        while (remaining > 0 || (method == 8 && !streamEnd)) {
            size_t n = (size_t)std::min<uint64_t>(remaining, inBuf.size());
            if (n) {
                r.f.Read(inBuf.data(), n);
                remaining -= n;
                if (mac) { mac->Update(inBuf.data(), n); ctr->Process(inBuf.data(), n); }
                if (zc) zc->Buf(inBuf.data(), n);
            }
            if (method == 0) {
                crc = crc32(crc, inBuf.data(), (uInt)n); out.Write(inBuf.data(), n); written += n;
                done += n;
            } else {
                if (n == 0 && streamEnd) break;
                zs.next_in = inBuf.data(); zs.avail_in = (uInt)n;
                do {
                    zs.next_out = outBuf.data(); zs.avail_out = (uInt)outBuf.size();
                    int rr = inflate(&zs, Z_NO_FLUSH);
                    if (rr == Z_STREAM_END) streamEnd = true;
                    else if (rr == Z_BUF_ERROR && n == 0) throw ZipError(TR("Date comprimate incomplete: ", "Incomplete compressed data: ") + z.name);
                    else if (rr != Z_OK && rr != Z_BUF_ERROR)
                        throw ZipError(z.encrypted() ? TR("Parolă greșită sau arhivă deteriorată: ", "Wrong password or damaged archive: ") + z.name
                                                     : TR("Date deteriorate în: ", "Damaged data in: ") + z.name);
                    size_t got = outBuf.size() - zs.avail_out;
                    crc = crc32(crc, outBuf.data(), (uInt)got); out.Write(outBuf.data(), got); written += got;
                } while (zs.avail_out == 0 && !streamEnd);
                if (streamEnd && remaining == 0) { done += n; break; }
                done += n;
                if (n == 0 && !streamEnd) throw ZipError(TR("Date comprimate incomplete: ", "Incomplete compressed data: ") + z.name);
            }
            if (progress && !progress(done, total, z.name)) throw Cancelled();
        }
        if (zinit) { inflateEnd(&zs); zinit = false; }
        if (remaining) { r.f.Seek(r.f.Tell() + remaining); }
        if (mac) {
            uint8_t h[20], stored[10]; mac->Final(h); r.f.Read(stored, 10);
            if (memcmp(h, stored, 10) != 0)
                throw ZipError(TR("Verificarea autenticității a eșuat pentru „", "Authenticity check failed for \"") + z.name + TR("”: parolă greșită sau arhivă modificată/deteriorată.", "\": wrong password or modified/damaged archive."));
        }
        bool checkCrc = !(z.aes && z.aesVersion == 2);
        if (checkCrc && crc != z.crc) throw ZipError(TR("CRC greșit (fișier deteriorat): ", "Bad CRC (damaged file): ") + z.name);
        if (written != z.usize && !(z.aes && z.aesVersion == 2 && z.usize == 0))
            throw ZipError(TR("Dimensiune greșită (fișier deteriorat): ", "Wrong size (damaged file): ") + z.name);
        out.Close();
    } catch (...) {
        if (zinit) inflateEnd(&zs);
        out.Close();
        RemoveQuiet(target);
        throw;
    }
    SetMTime(target, z.mtimeFt ? z.mtimeFt : DosToFt(z.dosTime));
}

ArchiveInfo InspectArchive(const fs::path& zip) {
    ZipReader r(zip);
    ArchiveInfo info;
    bool anyEnc = false, anyPlain = false, anyAes = false, anyZc = false;
    std::string root; bool rootOk = true; size_t considered = 0; bool rootHasChildren = false;
    for (auto& z : r.entries) {
        if (IsWarningName(z.name) && !z.encrypted()) { info.hasWarningFile = true; continue; }
        if (z.isDir()) ++info.dirs; else { ++info.files; info.totalBytes += z.usize; }
        if (!z.isDir()) {
            if (z.encrypted()) { anyEnc = true; if (z.aes) { anyAes = true; info.aesBits = std::max(info.aesBits, (int)AesKeyLen(z.aesStrength) * 8); } else anyZc = true; }
            else anyPlain = true;
            if (z.encrypted() && (z.flags & 0x40)) info.enc = EncKind::Unsupported;
            uint16_t m = z.realMethod(); if (m != 0 && m != 8) info.unsupportedMethod = true;
        }
        std::string n = z.name; std::replace(n.begin(), n.end(), '\\', '/');
        size_t s = n.find('/');
        std::string first = s == std::string::npos ? n : n.substr(0, s);
        bool isChild = s != std::string::npos && s + 1 < n.size();
        if (s == std::string::npos) rootOk = false; // fisier direct in radacina
        if (considered == 0) root = first; else if (first != root) rootOk = false;
        if (isChild) rootHasChildren = true;
        ++considered;
    }
    if (info.enc != EncKind::Unsupported) {
        if (!anyEnc) info.enc = EncKind::None;
        else if (anyAes && !anyZc && !anyPlain) info.enc = EncKind::Aes;
        else if (anyZc && !anyAes && !anyPlain) info.enc = EncKind::ZipCrypto;
        else info.enc = EncKind::Mixed;
    }
    info.nested = r.comment.find(kNestedMarker) != std::string::npos;
    if (rootOk && considered > 0 && rootHasChildren) info.singleRoot = root;
    return info;
}

static ExtractResult ExtractImpl(const fs::path& zip, const fs::path& destDir, const ExtractOptions& opt,
                                 const ProgressFn& progress, bool allowNested) {
    ZipReader r(zip);
    ExtractResult res;
    bool nested = allowNested && r.comment.find(kNestedMarker) != std::string::npos;
    uint64_t total = 0;
    const ZEntry* firstEnc = nullptr;
    for (auto& z : r.entries) {
        if (IsWarningName(z.name) && !z.encrypted()) continue;
        if (!z.isDir()) { CheckSupported(z); total += z.csize; }
        if (z.encrypted() && !firstEnc && !z.isDir()) firstEnc = &z;
    }
    if (firstEnc) VerifyPassword(r, *firstEnc, opt.password);

    std::error_code ec;
    fs::create_directories(LP(destDir), ec);
    if (!fs::is_directory(LP(destDir), ec)) throw ZipError(TR("Nu pot crea folderul de destinație: ", "Cannot create the destination folder: ") + PathToUtf8(destDir));

    std::vector<uint8_t> inBuf(kBuf), outBuf(kBuf);
    uint64_t done = 0;
    std::vector<std::pair<fs::path, uint64_t>> dirTimes;

    if (nested) {
        const ZEntry* inner = nullptr;
        for (auto& z : r.entries) if (IsInnerName(z.name)) inner = &z;
        if (inner) {
            fs::path tmp = TempSibling(destDir / "x", "inner");
            try {
                ExtractEntry(r, *inner, tmp, opt.password, inBuf, outBuf, done, total * 2, progress);
                r.f.Close();
                ExtractOptions io = opt; io.password.clear();
                res = ExtractImpl(tmp, destDir, io, [&](uint64_t d, uint64_t t, const std::string& n) {
                    return progress ? progress(total + (t ? d * total / t : 0), total * 2, n) : true;
                }, false);
                RemoveQuiet(tmp);
                return res;
            } catch (...) { RemoveQuiet(tmp); throw; }
        }
    }

    for (auto& z : r.entries) {
        if (IsWarningName(z.name) && !z.encrypted()) continue;
        std::string rel;
        if (!SafeRelative(z.name, rel)) { ++res.skippedUnsafe; continue; }
        fs::path target = destDir / Utf8ToPath(rel);
        if (z.isDir()) {
            fs::create_directories(LP(target), ec);
            dirTimes.push_back({ target, z.mtimeFt ? z.mtimeFt : DosToFt(z.dosTime) });
            ++res.dirs;
            continue;
        }
        fs::create_directories(LP(target.parent_path()), ec);
        if (fs::exists(LP(target), ec)) {
            if (opt.overwrite && !fs::is_directory(LP(target), ec)) RemoveQuiet(target);
            else target = UniqueName(target);
        }
        ExtractEntry(r, z, target, opt.password, inBuf, outBuf, done, total, progress);
        ++res.files;
    }
    for (auto it = dirTimes.rbegin(); it != dirTimes.rend(); ++it) SetMTime(it->first, it->second);
    return res;
}

ExtractResult ExtractArchive(const fs::path& zip, const fs::path& destDir, const ExtractOptions& opt,
                             const ProgressFn& progress) {
    return ExtractImpl(zip, destDir, opt, progress, true);
}

} // namespace rz
