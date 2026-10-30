// RedaotZipShell.dll - comenzi pentru meniul de click dreapta din Windows 11
// (IExplorerCommand, inregistrat prin pachetul MSIX "sparse" -> apare direct, fara "Show more options").
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <new>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>
#include "version.h"
#include "i18n.h"

// {1D0E488B-5D6A-4EBF-8072-5B131E14919E}
static const CLSID CLSID_RzArchive = { 0x1d0e488b, 0x5d6a, 0x4ebf, { 0x80, 0x72, 0x5b, 0x13, 0x1e, 0x14, 0x91, 0x9e } };
// {12BA1B38-1C92-42C9-AE82-6FF2BFE6C1E3}
static const CLSID CLSID_RzExtract = { 0x12ba1b38, 0x1c92, 0x42c9, { 0xae, 0x82, 0x6f, 0xf2, 0xbf, 0xe6, 0xc1, 0xe3 } };

static HMODULE g_module = nullptr;
static LONG g_refs = 0;

// Jurnal de diagnostic: %LOCALAPPDATA%\RedaotZip\shell.log (max ~100 KB)
static void ShellLog(const wchar_t* fmt, ...) {
    wchar_t dir[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", dir, MAX_PATH);
    if (!n || n >= MAX_PATH - 32) return;
    std::wstring d = std::wstring(dir) + L"\\RedaotZip";
    CreateDirectoryW(d.c_str(), nullptr);
    std::wstring f = d + L"\\shell.log";
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (GetFileAttributesExW(f.c_str(), GetFileExInfoStandard, &fa) && fa.nFileSizeLow > 100 * 1024) DeleteFileW(f.c_str());
    wchar_t msg[1024];
    va_list ap; va_start(ap, fmt); _vsnwprintf(msg, 1000, fmt, ap); va_end(ap); msg[1000] = 0;
    SYSTEMTIME t; GetLocalTime(&t);
    wchar_t line[1200];
    int len = _snwprintf(line, 1199, L"%04d-%02d-%02d %02d:%02d:%02d pid=%lu  %s\r\n", t.wYear, t.wMonth, t.wDay,
                         t.wHour, t.wMinute, t.wSecond, GetCurrentProcessId(), msg);
    if (len <= 0) return;
    int u8 = WideCharToMultiByte(CP_UTF8, 0, line, len, nullptr, 0, nullptr, nullptr);
    std::string out(u8, 0); WideCharToMultiByte(CP_UTF8, 0, line, len, &out[0], u8, nullptr, nullptr);
    HANDLE h = CreateFileW(f.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD wr; WriteFile(h, out.data(), (DWORD)out.size(), &wr, nullptr); CloseHandle(h);
}

static std::wstring ModuleDir() {
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(g_module, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    std::wstring s(buf, n);
    size_t p = s.find_last_of(L"\\/");
    return p == std::wstring::npos ? s : s.substr(0, p);
}
static std::wstring ExePath() {
    std::wstring dir = ModuleDir();
    std::wstring parent = dir.substr(0, dir.find_last_of(L"\\/"));
    std::wstring up = parent + L"\\RedaotZip.exe";
    // pachetul de dezarhivare are DLL-ul in subfolderul "meniu-zip"; programul principal e un nivel mai sus
    if (GetFileAttributesW(up.c_str()) != INVALID_FILE_ATTRIBUTES &&
        _wcsicmp(dir.c_str() + dir.find_last_of(L"\\/") + 1, L"meniu-zip") == 0) return up;
    return dir + L"\\RedaotZip.exe";
}

static bool EndsWithZip(const std::wstring& p) {
    if (p.size() < 4) return false;
    return _wcsicmp(p.c_str() + p.size() - 4, L".zip") == 0;
}

static std::vector<std::wstring> GetPaths(IShellItemArray* items) {
    std::vector<std::wstring> out;
    if (!items) return out;
    DWORD count = 0;
    if (FAILED(items->GetCount(&count))) return out;
    for (DWORD i = 0; i < count; ++i) {
        IShellItem* it = nullptr;
        if (SUCCEEDED(items->GetItemAt(i, &it))) {
            PWSTR name = nullptr;
            if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &name))) { out.push_back(name); CoTaskMemFree(name); }
            it->Release();
        }
    }
    return out;
}

// Lista de fisiere se trimite printr-un fisier temporar (UTF-16), ca sa nu depasim lungimea liniei de comanda.
static void Launch(const wchar_t* mode, const std::vector<std::wstring>& paths) {
    if (paths.empty()) return;
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
    std::wstring dir = std::wstring(tmp) + L"RedaotZip";
    CreateDirectoryW(dir.c_str(), nullptr);
    wchar_t name[64];
    wsprintfW(name, L"\\list-%u-%u.txt", GetCurrentProcessId(), GetTickCount());
    std::wstring list = dir + name;
    HANDLE h = CreateFileW(list.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    std::wstring body = L"\xFEFF";
    for (auto& p : paths) { body += p; body += L"\r\n"; }
    DWORD wr = 0;
    WriteFile(h, body.data(), (DWORD)(body.size() * sizeof(wchar_t)), &wr, nullptr);
    CloseHandle(h);

    std::wstring cmd = L"\"" + ExePath() + L"\" " + mode + L" \"@" + list + L"\"";
    STARTUPINFOW si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cl(cmd.begin(), cmd.end()); cl.push_back(0);
    if (CreateProcessW(nullptr, cl.data(), nullptr, nullptr, FALSE, 0, nullptr, ModuleDir().c_str(), &si, &pi)) {
        AllowSetForegroundWindow(pi.dwProcessId);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    } else {
        DeleteFileW(list.c_str());
    }
}

// Doua comenzi simple, fiecare in pachetul ei MSIX (Windows 11 grupeaza intr-un submeniu
// comenzile multiple ale aceluiasi pachet; cu un pachet per comanda, ambele apar direct):
//   Archive - "Arhivează cu REDAOT (AES-256)" pe orice fișier sau folder
//   Extract - "Dezarhivează cu REDAOT" doar pe arhive .zip
enum class Mode { Archive, Extract };

static bool AllZip(const std::vector<std::wstring>& paths) {
    if (paths.empty()) return false;
    for (auto& p : paths) if (!EndsWithZip(p)) return false;
    return true;
}

class RzCommand : public IExplorerCommand, public IObjectWithSite {
public:
    explicit RzCommand(Mode m) : mode_(m) { InterlockedIncrement(&g_refs); }
    virtual ~RzCommand() { if (site_) site_->Release(); InterlockedDecrement(&g_refs); }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IExplorerCommand)) *ppv = static_cast<IExplorerCommand*>(this);
        else if (IsEqualIID(riid, IID_IObjectWithSite)) *ppv = static_cast<IObjectWithSite*>(this);
        else { *ppv = nullptr; return E_NOINTERFACE; }
        AddRef(); return S_OK;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs_); }
    IFACEMETHODIMP_(ULONG) Release() override { LONG r = InterlockedDecrement(&refs_); if (!r) delete this; return r; }

    // IExplorerCommand
    IFACEMETHODIMP GetTitle(IShellItemArray*, LPWSTR* name) override {
        return SHStrDupW(mode_ == Mode::Extract ? TR(L"Dezarhivează cu REDAOT", L"Extract with REDAOT")
                                                : TR(L"Arhivează cu REDAOT (AES-256)", L"Archive with REDAOT (AES-256)"), name);
    }
    IFACEMETHODIMP GetIcon(IShellItemArray*, LPWSTR* icon) override {
        std::wstring s = ExePath() + L",0";
        return SHStrDupW(s.c_str(), icon);
    }
    IFACEMETHODIMP GetToolTip(IShellItemArray*, LPWSTR* tip) override { *tip = nullptr; return E_NOTIMPL; }
    IFACEMETHODIMP GetCanonicalName(GUID* g) override {
        *g = (mode_ == Mode::Archive) ? CLSID_RzArchive : CLSID_RzExtract;
        return S_OK;
    }
    IFACEMETHODIMP GetState(IShellItemArray* items, BOOL, EXPCMDSTATE* state) override {
        *state = ECS_ENABLED;
        if (mode_ == Mode::Extract && !AllZip(GetPaths(items))) *state = ECS_HIDDEN;
        ShellLog(L"GetState mode=%d -> %s", (int)mode_, *state == ECS_HIDDEN ? L"ascuns" : L"vizibil");
        return S_OK;
    }
    IFACEMETHODIMP Invoke(IShellItemArray* items, IBindCtx*) override {
        ShellLog(L"Invoke mode=%d", (int)mode_);
        Launch(mode_ == Mode::Extract ? L"/x" : L"/a", GetPaths(items));
        return S_OK;
    }
    IFACEMETHODIMP GetFlags(EXPCMDFLAGS* flags) override { *flags = ECF_DEFAULT; return S_OK; }
    IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** e) override { *e = nullptr; return E_NOTIMPL; }

    // IObjectWithSite
    IFACEMETHODIMP SetSite(IUnknown* site) override {
        if (site_) site_->Release();
        site_ = site; if (site_) site_->AddRef();
        return S_OK;
    }
    IFACEMETHODIMP GetSite(REFIID riid, void** ppv) override {
        if (!site_) { *ppv = nullptr; return E_FAIL; }
        return site_->QueryInterface(riid, ppv);
    }
private:
    LONG refs_ = 1;
    Mode mode_;
    IUnknown* site_ = nullptr;
};

class RzFactory : public IClassFactory {
public:
    explicit RzFactory(bool extract) : extract_(extract) { InterlockedIncrement(&g_refs); }
    virtual ~RzFactory() { InterlockedDecrement(&g_refs); }
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) { *ppv = this; AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs_); }
    IFACEMETHODIMP_(ULONG) Release() override { LONG r = InterlockedDecrement(&refs_); if (!r) delete this; return r; }
    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        *ppv = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        RzCommand* c = new (std::nothrow) RzCommand(extract_ ? Mode::Extract : Mode::Archive);
        if (!c) return E_OUTOFMEMORY;
        HRESULT hr = c->QueryInterface(riid, ppv);
        c->Release();
        return hr;
    }
    IFACEMETHODIMP LockServer(BOOL lock) override { if (lock) InterlockedIncrement(&g_refs); else InterlockedDecrement(&g_refs); return S_OK; }
private:
    LONG refs_ = 1;
    bool extract_;
};

extern "C" {

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = h; DisableThreadLibraryCalls(h);
        wchar_t host[MAX_PATH] = L""; GetModuleFileNameW(nullptr, host, MAX_PATH);
        ShellLog(L"DLL incarcat de %s (v%s)", host, RZ_VERSION_W);
    }
    return TRUE;
}

__declspec(dllexport) HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void** ppv) {
    *ppv = nullptr;
    bool ex;
    if (IsEqualCLSID(clsid, CLSID_RzArchive)) ex = false;
    else if (IsEqualCLSID(clsid, CLSID_RzExtract)) ex = true;
    else { ShellLog(L"DllGetClassObject: CLSID necunoscut"); return CLASS_E_CLASSNOTAVAILABLE; }
    ShellLog(L"DllGetClassObject %s", ex ? L"extract" : L"archive");
    RzFactory* f = new (std::nothrow) RzFactory(ex);
    if (!f) return E_OUTOFMEMORY;
    HRESULT hr = f->QueryInterface(riid, ppv);
    f->Release();
    return hr;
}

__declspec(dllexport) HRESULT WINAPI DllCanUnloadNow() { return g_refs == 0 ? S_OK : S_FALSE; }

}
