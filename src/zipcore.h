#pragma once
// Motorul de arhivare/dezarhivare ZIP cu criptare WinZip AES-256.
// Compatibil cu 7-Zip, WinRAR, PeaZip, WinZip.
#include <cstdint>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace rz {

namespace fs = std::filesystem;

// done, total (bytes), nume fisier curent (UTF-8). Returneaza false pentru anulare.
using ProgressFn = std::function<bool(uint64_t done, uint64_t total, const std::string& name)>;

struct Cancelled : std::runtime_error { Cancelled() : std::runtime_error("cancelled") {} };
struct WrongPassword : std::runtime_error { using std::runtime_error::runtime_error; };
struct ZipError : std::runtime_error { using std::runtime_error::runtime_error; };

extern const char* kWarningFileName;   // fisierul de avertizare pentru utilitarul Windows
extern const char* kNestedInnerName;   // arhiva interna cand numele fisierelor sunt ascunse

struct ArchiveOptions {
    std::string password;      // UTF-8; gol = fara criptare
    int level = 6;             // 0 = fara compresie, 1 rapid, 6 normal, 9 maxim
    bool addWarningFile = true;
    bool hideNames = false;    // arhiva in arhiva: numele fisierelor nu se vad fara parola
};

struct ArchiveSummary {
    uint64_t files = 0, dirs = 0, totalBytes = 0;
};

// Numara ce urmeaza sa fie arhivat (pentru afisare in interfata).
ArchiveSummary ScanInputs(const std::vector<fs::path>& inputs);

void CreateArchive(const std::vector<fs::path>& inputs, const fs::path& output,
                   const ArchiveOptions& opt, const ProgressFn& progress);

enum class EncKind { None, Aes, ZipCrypto, Mixed, Unsupported };

struct ArchiveInfo {
    uint64_t files = 0, dirs = 0, totalBytes = 0;
    EncKind enc = EncKind::None;
    int aesBits = 0;
    bool nested = false;          // creata cu "ascunde numele"
    bool hasWarningFile = false;
    bool unsupportedMethod = false;
    std::string singleRoot;       // daca totul e intr-un singur folder radacina (UTF-8)
};

ArchiveInfo InspectArchive(const fs::path& zip);

struct ExtractOptions {
    std::string password;
    bool overwrite = false;       // altfel se redenumeste: "fisier (2).txt"
};

struct ExtractResult {
    uint64_t files = 0, dirs = 0, skippedUnsafe = 0;
};

ExtractResult ExtractArchive(const fs::path& zip, const fs::path& destDir,
                             const ExtractOptions& opt, const ProgressFn& progress);

// Utilitare
std::string PathToUtf8(const fs::path& p);
fs::path Utf8ToPath(const std::string& s);
int PasswordStrength(const std::string& pwUtf8); // 0..4

} // namespace rz
