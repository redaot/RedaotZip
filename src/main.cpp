// RedaotZip.exe - interfata grafica (Win32) pentru arhivare / dezarhivare AES-256.
//   RedaotZip.exe                      fereastra principala
//   RedaotZip.exe /a <fisiere|@lista>  arhivare
//   RedaotZip.exe /x <zip|@lista>      dezarhivare
//   RedaotZip.exe /ac <fisier>         arhivare din meniul clasic (aduna selectia multipla)
//   RedaotZip.exe /register-classic | /unregister-classic | /repair
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "resource.h"
#include "version.h"
#include "zipcore.h"
#include "i18n.h"

using namespace rz;

static HINSTANCE g_inst;
static const UINT WM_RZ_PROGRESS = WM_APP + 1;
static const UINT WM_RZ_DONE = WM_APP + 2;

// ------------------------------------------------------------------ utilitare text
static std::wstring W(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0); MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}
static std::string U(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
static std::wstring GetText(HWND dlg, int id) {
    HWND h = GetDlgItem(dlg, id);
    int n = GetWindowTextLengthW(h);
    std::wstring s(n, 0);
    if (n) GetWindowTextW(h, &s[0], n + 1);
    return s;
}
static void SecureClear(HWND dlg, int id) { SetDlgItemTextW(dlg, id, L""); }

static std::wstring FormatSize(uint64_t b) {
    wchar_t buf[64];
    if (b < 1024) swprintf(buf, 64, L"%llu B", (unsigned long long)b);
    else if (b < 1024ull * 1024) swprintf(buf, 64, L"%.1f KB", b / 1024.0);
    else if (b < 1024ull * 1024 * 1024) swprintf(buf, 64, L"%.1f MB", b / 1048576.0);
    else swprintf(buf, 64, L"%.2f GB", b / 1073741824.0);
    return buf;
}

static void Msg(HWND owner, const std::wstring& text, UINT flags = MB_ICONINFORMATION) {
    MessageBoxW(owner, text.c_str(), L"RedaotZip", flags | MB_OK);
}

// ------------------------------------------------------------------ aspect comun
static HFONT g_titleFont = nullptr;
static HICON g_bigIcon = nullptr, g_smallIcon = nullptr;

static void SetupCommon(HWND dlg) {
    UINT dpi = 96;
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    auto pGetDpiForWindow = (UINT(WINAPI*)(HWND))GetProcAddress(u32, "GetDpiForWindow");
    if (pGetDpiForWindow) dpi = pGetDpiForWindow(dlg);
    int big = MulDiv(48, dpi, 96), small = MulDiv(16, dpi, 96);
    if (g_bigIcon) DestroyIcon(g_bigIcon);
    if (g_smallIcon) DestroyIcon(g_smallIcon);
    g_bigIcon = (HICON)LoadImageW(g_inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, big, big, 0);
    g_smallIcon = (HICON)LoadImageW(g_inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, small, small, 0);
    SendMessageW(dlg, WM_SETICON, ICON_BIG, (LPARAM)g_bigIcon);
    SendMessageW(dlg, WM_SETICON, ICON_SMALL, (LPARAM)g_smallIcon);
    SendDlgItemMessageW(dlg, IDC_LOGO, STM_SETICON, (WPARAM)g_bigIcon, 0);
    if (g_titleFont) DeleteObject(g_titleFont);
    g_titleFont = CreateFontW(-MulDiv(15, dpi, 72), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    SendDlgItemMessageW(dlg, IDC_TITLE, WM_SETFONT, (WPARAM)g_titleFont, TRUE);
    std::wstring brand = std::wstring(RZ_BRAND_W) + L"  •  v" + RZ_VERSION_W;
    SetDlgItemTextW(dlg, IDC_BRAND, brand.c_str());
}

static INT_PTR BrandColor(HWND dlg, WPARAM wp, LPARAM lp) {
    HWND ctl = (HWND)lp; HDC dc = (HDC)wp;
    int id = GetDlgCtrlID(ctl);
    if (id == IDC_BRAND || id == IDC_SUBTITLE) {
        SetTextColor(dc, RGB(110, 110, 110));
        SetBkMode(dc, TRANSPARENT);
        return (INT_PTR)GetSysColorBrush(COLOR_BTNFACE);
    }
    (void)dlg;
    return 0;
}

static void TogglePassword(HWND dlg, bool show, std::initializer_list<int> ids) {
    for (int id : ids) {
        HWND h = GetDlgItem(dlg, id);
        SendMessageW(h, EM_SETPASSWORDCHAR, show ? 0 : (WPARAM)0x25CF, 0);
        InvalidateRect(h, nullptr, TRUE);
    }
}

static void OpenFolder(const std::wstring& path, bool selectFile) {
    if (selectFile) {
        std::wstring args = L"/select,\"" + path + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    } else {
        ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

static bool BrowseFolder(HWND owner, std::wstring& inout, const wchar_t* title) {
    IFileOpenDialog* d = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) return false;
    DWORD opts = 0; d->GetOptions(&opts);
    d->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    d->SetTitle(title);
    if (!inout.empty()) {
        IShellItem* si = nullptr;
        std::wstring start = inout;
        DWORD a = GetFileAttributesW(start.c_str());
        if (a == INVALID_FILE_ATTRIBUTES) start = fs::path(start).parent_path().wstring();
        if (SUCCEEDED(SHCreateItemFromParsingName(start.c_str(), nullptr, IID_PPV_ARGS(&si)))) { d->SetFolder(si); si->Release(); }
    }
    bool ok = false;
    if (SUCCEEDED(d->Show(owner))) {
        IShellItem* r = nullptr;
        if (SUCCEEDED(d->GetResult(&r))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(r->GetDisplayName(SIGDN_FILESYSPATH, &p))) { inout = p; CoTaskMemFree(p); ok = true; }
            r->Release();
        }
    }
    d->Release();
    return ok;
}


// Ferestrele sunt definite in romana in app.rc; in engleza textele se inlocuiesc la deschidere.
struct UiText { int id; const wchar_t* en; };
static void LocalizeDialog(HWND dlg, const wchar_t* caption, std::initializer_list<UiText> items) {
    if (rz::IsRo()) return;
    SetWindowTextW(dlg, caption);
    for (auto& t : items) SetDlgItemTextW(dlg, t.id, t.en);
}

static void LocalizeArchive(HWND dlg) {
    LocalizeDialog(dlg, L"RedaotZip - Encrypted archive", {
        { IDC_TITLE, L"AES-256 encrypted archive" }, { IDC_L_OUT, L"Archive to create:" },
        { IDC_PWLABEL, L"Password:" }, { IDC_PW2LABEL, L"Confirm password:" }, { IDC_SHOWPW, L"Show password" },
        { IDC_L_LEVEL, L"Compression:" },
        { IDC_WARNFILE, L"Add an instructions file for people who open the archive with Windows" },
        { IDC_HIDENAMES, L"Also hide file names (archive inside archive)" },
        { IDOK, L"Archive" }, { IDCANCEL, L"Cancel" } });
}
static void LocalizeExtract(HWND dlg) {
    LocalizeDialog(dlg, L"RedaotZip - Extract", {
        { IDC_TITLE, L"Extract" }, { IDC_L_DEST, L"Extract to folder:" }, { IDC_PWLABEL, L"Password:" },
        { IDC_SHOWPW, L"Show password" },
        { IDC_OVERWRITE, L"Overwrite existing files (otherwise they are renamed)" },
        { IDC_OPENFOLDER, L"Open the folder after extracting" },
        { IDOK, L"Extract" }, { IDCANCEL, L"Cancel" } });
}
static void LocalizeMain(HWND dlg) {
    LocalizeDialog(dlg, L"RedaotZip", {
        { IDC_SUBTITLE, L"ZIP archiver with AES-256 encryption" },
        { IDC_M_FILES, L"Archive files..." }, { IDC_M_FOLDER, L"Archive a folder..." },
        { IDC_M_EXTRACT, L"Extract a ZIP archive..." },
        { IDC_L_TIP, L"Tip: you can also find these in the Explorer right-click menu." },
        { IDCANCEL, L"Close" } });
}

// ------------------------------------------------------------------ lucru in fundal
struct Job {
    std::atomic<bool> cancel{ false };
    std::atomic<bool> running{ false };
    std::mutex m;
    std::wstring current;
    bool ok = false, wrongPassword = false, cancelled = false;
    std::wstring error;
    ExtractResult xres;
};

static ProgressFn MakeProgress(HWND dlg, Job* job) {
    auto last = std::make_shared<ULONGLONG>(0);
    return [dlg, job, last](uint64_t done, uint64_t total, const std::string& name) {
        ULONGLONG now = GetTickCount64();
        if (now - *last >= 80 || done == total) {
            *last = now;
            int pct = total ? (int)(done * 1000 / total) : 1000;
            { std::lock_guard<std::mutex> g(job->m); job->current = W(name); }
            PostMessageW(dlg, WM_RZ_PROGRESS, (WPARAM)pct, 0);
        }
        return !job->cancel.load();
    };
}

template <class F>
static void RunJob(HWND dlg, Job* job, F work) {
    job->running = true; job->cancel = false; job->ok = job->wrongPassword = job->cancelled = false; job->error.clear();
    std::thread([dlg, job, work]() {
        try { work(); job->ok = true; }
        catch (Cancelled&) { job->cancelled = true; }
        catch (WrongPassword& e) { job->wrongPassword = true; job->error = W(e.what()); }
        catch (std::exception& e) { job->error = W(e.what()); }
        catch (...) { job->error = TR(L"Eroare necunoscută.", L"Unknown error."); }
        job->running = false;
        PostMessageW(dlg, WM_RZ_DONE, 0, 0);
    }).detach();
}

static void SetBusy(HWND dlg, bool busy, std::initializer_list<int> ids) {
    for (int id : ids) EnableWindow(GetDlgItem(dlg, id), !busy);
    EnableWindow(GetDlgItem(dlg, IDOK), !busy);
    SetDlgItemTextW(dlg, IDCANCEL, busy ? TR(L"Oprește", L"Stop") : TR(L"Închide", L"Close"));
}

static void ShowProgress(HWND dlg, Job* job, WPARAM pct, const wchar_t* verb) {
    SendDlgItemMessageW(dlg, IDC_PROGRESS, PBM_SETPOS, pct, 0);
    std::wstring cur;
    { std::lock_guard<std::mutex> g(job->m); cur = job->current; }
    wchar_t buf[32]; swprintf(buf, 32, L"%d%%  ", (int)(pct / 10));
    SetDlgItemTextW(dlg, IDC_STATUS, (std::wstring(buf) + verb + L" " + cur).c_str());
}

// ------------------------------------------------------------------ ARHIVARE
struct ArchiveState {
    std::vector<fs::path> inputs;
    ArchiveSummary summary;
    Job job;
    std::wstring output;
    bool finished = false;
};

static std::wstring DefaultOutput(const std::vector<fs::path>& in) {
    fs::path first = in[0];
    fs::path dir = first.parent_path();
    std::wstring base;
    if (in.size() == 1) {
        DWORD a = GetFileAttributesW(first.c_str());
        base = (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) ? first.filename().wstring() : first.stem().wstring();
    } else {
        base = dir.filename().wstring();
    }
    if (base.empty()) base = TR(L"Arhiva", L"Archive");
    fs::path p = dir / (base + L".zip");
    for (int i = 2; GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES && i < 10000; ++i)
        p = dir / (base + L" (" + std::to_wstring(i) + L").zip");
    return p.wstring();
}

static void UpdateStrength(HWND dlg) {
    std::string pw = U(GetText(dlg, IDC_PW));
    static const wchar_t* names[] = { L"", TR(L"Putere parolă: slabă", L"Password strength: weak"), TR(L"Putere parolă: medie", L"Password strength: fair"), TR(L"Putere parolă: bună", L"Password strength: good"), TR(L"Putere parolă: foarte bună", L"Password strength: very good") };
    int s = PasswordStrength(pw);
    SetDlgItemTextW(dlg, IDC_STRENGTH, names[s]);
    SetWindowLongPtrW(GetDlgItem(dlg, IDC_STRENGTH), GWLP_USERDATA, s);
    InvalidateRect(GetDlgItem(dlg, IDC_STRENGTH), nullptr, TRUE);
}

static const int kArchiveInputs[] = { IDC_OUT, IDC_BROWSE, IDC_PW, IDC_PW2, IDC_SHOWPW, IDC_LEVEL, IDC_WARNFILE, IDC_HIDENAMES };

static INT_PTR CALLBACK ArchiveProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    ArchiveState* st = (ArchiveState*)GetWindowLongPtrW(dlg, DWLP_USER);
    switch (msg) {
    case WM_INITDIALOG: {
        st = (ArchiveState*)lp;
        SetWindowLongPtrW(dlg, DWLP_USER, (LONG_PTR)st);
        SetupCommon(dlg);
        LocalizeArchive(dlg);
        std::wstring sub;
        if (st->inputs.size() == 1) sub = st->inputs[0].filename().wstring() + L"  •  ";
        sub += std::to_wstring(st->summary.files) + TR(L" fișiere, ", L" files, ") + std::to_wstring(st->summary.dirs) + TR(L" foldere, ", L" folders, ") + FormatSize(st->summary.totalBytes);
        SetDlgItemTextW(dlg, IDC_SUBTITLE, sub.c_str());
        SetDlgItemTextW(dlg, IDC_OUT, DefaultOutput(st->inputs).c_str());
        HWND cb = GetDlgItem(dlg, IDC_LEVEL);
        for (auto s : { TR(L"Fără compresie (cel mai rapid)", L"No compression (fastest)"), TR(L"Rapidă", L"Fast"), TR(L"Normală", L"Normal"), TR(L"Maximă (mai lent)", L"Maximum (slower)") })
            SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)s);
        SendMessageW(cb, CB_SETCURSEL, 2, 0);
        CheckDlgButton(dlg, IDC_WARNFILE, BST_CHECKED);
        CheckDlgButton(dlg, IDC_HIDENAMES, BST_CHECKED); // implicit: si numele fisierelor sunt ascunse
        SendDlgItemMessageW(dlg, IDC_PROGRESS, PBM_SETRANGE32, 0, 1000);
        SendDlgItemMessageW(dlg, IDC_PW, EM_SETCUEBANNER, TRUE, (LPARAM)TR(L"minim 12 caractere", L"at least 12 characters"));
        SetDlgItemTextW(dlg, IDC_STATUS, TR(L"Criptare: AES-256 (format WinZip AES, compatibil 7-Zip / WinRAR / PeaZip / WinZip)", L"Encryption: AES-256 (WinZip AES format, works with 7-Zip / WinRAR / PeaZip / WinZip)"));
        SetFocus(GetDlgItem(dlg, IDC_PW));
        return FALSE;
    }
    case WM_CTLCOLORSTATIC: {
        if ((HWND)lp == GetDlgItem(dlg, IDC_STRENGTH)) {
            static const COLORREF c[] = { RGB(0, 0, 0), RGB(200, 40, 40), RGB(200, 120, 0), RGB(40, 140, 60), RGB(0, 120, 60) };
            int s = (int)GetWindowLongPtrW((HWND)lp, GWLP_USERDATA);
            SetTextColor((HDC)wp, c[s < 0 || s > 4 ? 0 : s]);
            SetBkMode((HDC)wp, TRANSPARENT);
            return (INT_PTR)GetSysColorBrush(COLOR_BTNFACE);
        }
        return BrandColor(dlg, wp, lp);
    }
    case WM_DPICHANGED: SetupCommon(dlg); return FALSE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_PW: if (HIWORD(wp) == EN_CHANGE) UpdateStrength(dlg); break;
        case IDC_SHOWPW: TogglePassword(dlg, IsDlgButtonChecked(dlg, IDC_SHOWPW) == BST_CHECKED, { IDC_PW, IDC_PW2 }); break;
        case IDC_BROWSE: {
            std::wstring cur = GetText(dlg, IDC_OUT);
            wchar_t file[32768] = {};
            wcsncpy(file, fs::path(cur).filename().c_str(), 32767);
            std::wstring dir = fs::path(cur).parent_path().wstring();
            OPENFILENAMEW of{}; of.lStructSize = sizeof(of); of.hwndOwner = dlg;
            of.lpstrFilter = TR(L"Arhivă ZIP (*.zip)\0*.zip\0", L"ZIP archive (*.zip)\0*.zip\0"); of.lpstrFile = file; of.nMaxFile = 32768;
            of.lpstrInitialDir = dir.c_str(); of.lpstrDefExt = L"zip";
            of.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
            if (GetSaveFileNameW(&of)) SetDlgItemTextW(dlg, IDC_OUT, file);
            break;
        }
        case IDOK: {
            if (st->finished) { OpenFolder(st->output, true); EndDialog(dlg, IDOK); break; }
            std::wstring out = GetText(dlg, IDC_OUT);
            std::wstring pw = GetText(dlg, IDC_PW), pw2 = GetText(dlg, IDC_PW2);
            if (out.empty()) { Msg(dlg, TR(L"Alege unde se salvează arhiva.", L"Choose where to save the archive."), MB_ICONWARNING); break; }
            if (fs::path(out).extension().wstring().empty()) out += L".zip";
            if (pw.empty()) { Msg(dlg, TR(L"Introdu o parolă. Fără parolă arhiva nu ar fi criptată.", L"Enter a password. Without a password the archive would not be encrypted."), MB_ICONWARNING); SetFocus(GetDlgItem(dlg, IDC_PW)); break; }
            if (pw != pw2) { Msg(dlg, TR(L"Parolele nu coincid. Scrie aceeași parolă în ambele câmpuri.", L"The passwords do not match. Type the same password in both fields."), MB_ICONWARNING); SetFocus(GetDlgItem(dlg, IDC_PW2)); break; }
            std::string pwU = U(pw);
            if (PasswordStrength(pwU) <= 1) {
                if (MessageBoxW(dlg, TR(L"Parola este slabă și poate fi ghicită ușor.\n\n"
                                     L"Criptarea AES-256 e la fel de sigură ca parola: recomandăm cel puțin 12 caractere "
                                     L"cu litere mari, mici, cifre și simboluri.\n\nContinui oricum?",
                                     L"The password is weak and can be guessed easily.\n\n"
                                     L"AES-256 encryption is only as strong as the password: we recommend at least 12 characters "
                                     L"with upper and lower case letters, digits and symbols.\n\nContinue anyway?"),
                                L"RedaotZip", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES) break;
            }
            bool nonAscii = false; for (wchar_t c : pw) if (c > 127) nonAscii = true;
            if (nonAscii) {
                if (MessageBoxW(dlg, TR(L"Parola conține diacritice sau caractere speciale (ex. ă, î, ș, ț).\n\n"
                                     L"RedaotZip și 7-Zip le acceptă, dar unele programe pot interpreta altfel aceste caractere "
                                     L"și nu vor accepta parola. Pentru compatibilitate maximă folosește doar litere fără diacritice, cifre și simboluri.\n\n"
                                     L"Continui cu această parolă?",
                                     L"The password contains accented or special characters (e.g. ă, é, ñ, ü).\n\n"
                                     L"RedaotZip and 7-Zip accept them, but some programs may interpret these characters differently "
                                     L"and will not accept the password. For maximum compatibility use only plain letters, digits and symbols.\n\n"
                                     L"Continue with this password?"), L"RedaotZip", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES) break;
            }
            if (GetFileAttributesW(out.c_str()) != INVALID_FILE_ATTRIBUTES && GetText(dlg, IDC_OUT) == out) {
                if (MessageBoxW(dlg, (TR(L"Fișierul există deja:\n", L"The file already exists:\n") + out + TR(L"\n\nÎl înlocuiesc?", L"\n\nReplace it?")).c_str(), L"RedaotZip",
                                MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) != IDYES) break;
            }
            ArchiveOptions o;
            o.password = pwU;
            static const int levels[] = { 0, 1, 6, 9 };
            int sel = (int)SendDlgItemMessageW(dlg, IDC_LEVEL, CB_GETCURSEL, 0, 0);
            o.level = levels[sel < 0 || sel > 3 ? 2 : sel];
            o.addWarningFile = IsDlgButtonChecked(dlg, IDC_WARNFILE) == BST_CHECKED;
            o.hideNames = IsDlgButtonChecked(dlg, IDC_HIDENAMES) == BST_CHECKED;
            st->output = out;
            SetBusy(dlg, true, { IDC_OUT, IDC_BROWSE, IDC_PW, IDC_PW2, IDC_SHOWPW, IDC_LEVEL, IDC_WARNFILE, IDC_HIDENAMES });
            auto inputs = st->inputs;
            ProgressFn pf = MakeProgress(dlg, &st->job);
            RunJob(dlg, &st->job, [inputs, out, o, pf]() { CreateArchive(inputs, fs::path(out), o, pf); });
            break;
        }
        case IDCANCEL:
            if (st->job.running) {
                if (MessageBoxW(dlg, TR(L"Opresc arhivarea?", L"Stop archiving?"), L"RedaotZip", MB_ICONQUESTION | MB_YESNO) == IDYES) st->job.cancel = true;
            } else EndDialog(dlg, IDCANCEL);
            break;
        }
        return TRUE;
    case WM_RZ_PROGRESS: ShowProgress(dlg, &st->job, wp, TR(L"Arhivez", L"Archiving")); return TRUE;
    case WM_RZ_DONE: {
        SetBusy(dlg, false, { IDC_OUT, IDC_BROWSE, IDC_PW, IDC_PW2, IDC_SHOWPW, IDC_LEVEL, IDC_WARNFILE, IDC_HIDENAMES });
        if (st->job.ok) {
            SendDlgItemMessageW(dlg, IDC_PROGRESS, PBM_SETPOS, 1000, 0);
            WIN32_FILE_ATTRIBUTE_DATA d{};
            uint64_t sz = 0;
            if (GetFileAttributesExW(st->output.c_str(), GetFileExInfoStandard, &d)) sz = ((uint64_t)d.nFileSizeHigh << 32) | d.nFileSizeLow;
            SetDlgItemTextW(dlg, IDC_STATUS, (TR(L"Gata! Arhivă criptată creată (", L"Done! Encrypted archive created (") + FormatSize(sz) + L"): " + fs::path(st->output).filename().wstring()).c_str());
            SecureClear(dlg, IDC_PW); SecureClear(dlg, IDC_PW2);
            for (int id : kArchiveInputs) EnableWindow(GetDlgItem(dlg, id), FALSE);
            st->finished = true;
            SetDlgItemTextW(dlg, IDOK, TR(L"Arată arhiva", L"Show archive"));
            SetFocus(GetDlgItem(dlg, IDOK));
        } else if (st->job.cancelled) {
            SendDlgItemMessageW(dlg, IDC_PROGRESS, PBM_SETPOS, 0, 0);
            SetDlgItemTextW(dlg, IDC_STATUS, TR(L"Arhivarea a fost oprită. Nu s-a salvat nimic.", L"Archiving was stopped. Nothing was saved."));
        } else {
            SendDlgItemMessageW(dlg, IDC_PROGRESS, PBM_SETPOS, 0, 0);
            SetDlgItemTextW(dlg, IDC_STATUS, TR(L"Eroare - arhiva nu a fost creată.", L"Error - the archive was not created."));
            Msg(dlg, TR(L"Arhiva nu a putut fi creată:\n\n", L"The archive could not be created:\n\n") + st->job.error, MB_ICONERROR);
        }
        return TRUE;
    }
    case WM_CLOSE: SendMessageW(dlg, WM_COMMAND, IDCANCEL, 0); return TRUE;
    }
    return FALSE;
}

static int RunArchive(HWND owner, const std::vector<fs::path>& inputs) {
    ArchiveState st;
    st.inputs = inputs;
    try { st.summary = ScanInputs(inputs); }
    catch (std::exception& e) { Msg(owner, TR(L"Nu pot citi fișierele selectate:\n\n", L"Cannot read the selected files:\n\n") + W(e.what()), MB_ICONERROR); return 1; }
    DialogBoxParamW(g_inst, MAKEINTRESOURCEW(IDD_ARCHIVE), owner, ArchiveProc, (LPARAM)&st);
    return 0;
}

// ------------------------------------------------------------------ DEZARHIVARE
struct ExtractState {
    fs::path zip;
    ArchiveInfo info;
    Job job;
    std::wstring dest;
    std::wstring* sharedPassword = nullptr; // refolosita pentru mai multe arhive selectate
    int index = 0, count = 1;
};

static const int kExtractInputs[] = { IDC_DEST, IDC_BROWSE, IDC_PW, IDC_SHOWPW, IDC_OVERWRITE, IDC_OPENFOLDER };

static std::wstring DefaultDest(const fs::path& zip, const ArchiveInfo& info) {
    fs::path dir = zip.parent_path();
    if (!info.singleRoot.empty() && !info.nested) return dir.wstring(); // contine deja un singur folder
    return (dir / zip.stem()).wstring();
}

static INT_PTR CALLBACK ExtractProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    ExtractState* st = (ExtractState*)GetWindowLongPtrW(dlg, DWLP_USER);
    switch (msg) {
    case WM_INITDIALOG: {
        st = (ExtractState*)lp;
        SetWindowLongPtrW(dlg, DWLP_USER, (LONG_PTR)st);
        SetupCommon(dlg);
        LocalizeExtract(dlg);
        if (st->count > 1) {
            std::wstring t = TR(L"Dezarhivare (", L"Extract (") + std::to_wstring(st->index + 1) + TR(L" din ", L" of ") + std::to_wstring(st->count) + L")";
            SetDlgItemTextW(dlg, IDC_TITLE, t.c_str());
        }
        SetDlgItemTextW(dlg, IDC_SUBTITLE, st->zip.filename().c_str());
        std::wstring enc;
        switch (st->info.enc) {
        case EncKind::None: enc = TR(L"Arhivă necriptată", L"Not encrypted"); break;
        case EncKind::Aes: enc = TR(L"Criptare AES-", L"Encryption AES-") + std::to_wstring(st->info.aesBits); break;
        case EncKind::ZipCrypto: enc = TR(L"Criptare ZipCrypto (veche, slabă)", L"ZipCrypto encryption (old, weak)"); break;
        case EncKind::Mixed: enc = TR(L"Criptare parțială (unele fișiere nu sunt criptate)", L"Partial encryption (some files are not encrypted)"); break;
        case EncKind::Unsupported: enc = TR(L"Criptare nesuportată", L"Unsupported encryption"); break;
        }
        std::wstring info = enc + L"  •  ";
        if (st->info.nested) info += TR(L"nume de fișiere ascunse", L"file names hidden");
        else info += std::to_wstring(st->info.files) + TR(L" fișiere, ", L" files, ") + std::to_wstring(st->info.dirs) + TR(L" foldere, ", L" folders, ") + FormatSize(st->info.totalBytes);
        if (st->info.unsupportedMethod) info += TR(L"\nAtenție: conține compresii nesuportate (folosește 7-Zip).", L"\nWarning: contains unsupported compression (use 7-Zip).");
        SetDlgItemTextW(dlg, IDC_INFO, info.c_str());
        SetDlgItemTextW(dlg, IDC_DEST, DefaultDest(st->zip, st->info).c_str());
        bool needPw = st->info.enc != EncKind::None;
        EnableWindow(GetDlgItem(dlg, IDC_PW), needPw);
        EnableWindow(GetDlgItem(dlg, IDC_SHOWPW), needPw);
        if (!needPw) SetDlgItemTextW(dlg, IDC_PWLABEL, TR(L"Parolă: (nu este necesară)", L"Password: (not needed)"));
        else if (st->sharedPassword && !st->sharedPassword->empty()) SetDlgItemTextW(dlg, IDC_PW, st->sharedPassword->c_str());
        CheckDlgButton(dlg, IDC_OPENFOLDER, BST_CHECKED);
        SendDlgItemMessageW(dlg, IDC_PROGRESS, PBM_SETRANGE32, 0, 1000);
        SetFocus(GetDlgItem(dlg, needPw ? IDC_PW : IDOK));
        return FALSE;
    }
    case WM_CTLCOLORSTATIC: return BrandColor(dlg, wp, lp);
    case WM_DPICHANGED: SetupCommon(dlg); return FALSE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_SHOWPW: TogglePassword(dlg, IsDlgButtonChecked(dlg, IDC_SHOWPW) == BST_CHECKED, { IDC_PW }); break;
        case IDC_BROWSE: {
            std::wstring d = GetText(dlg, IDC_DEST);
            if (BrowseFolder(dlg, d, TR(L"Alege folderul în care se extrage", L"Choose the folder to extract to"))) SetDlgItemTextW(dlg, IDC_DEST, d.c_str());
            break;
        }
        case IDOK: {
            std::wstring dest = GetText(dlg, IDC_DEST);
            if (dest.empty()) { Msg(dlg, TR(L"Alege folderul de destinație.", L"Choose the destination folder."), MB_ICONWARNING); break; }
            std::wstring pw = GetText(dlg, IDC_PW);
            if (st->info.enc != EncKind::None && pw.empty()) {
                Msg(dlg, TR(L"Arhiva este criptată. Introdu parola.", L"The archive is encrypted. Enter the password."), MB_ICONWARNING); SetFocus(GetDlgItem(dlg, IDC_PW)); break;
            }
            ExtractOptions o;
            o.password = U(pw);
            o.overwrite = IsDlgButtonChecked(dlg, IDC_OVERWRITE) == BST_CHECKED;
            st->dest = dest;
            if (st->sharedPassword) *st->sharedPassword = pw;
            SetBusy(dlg, true, { IDC_DEST, IDC_BROWSE, IDC_PW, IDC_SHOWPW, IDC_OVERWRITE, IDC_OPENFOLDER });
            fs::path zip = st->zip;
            ExtractState* s = st;
            ProgressFn pf = MakeProgress(dlg, &st->job);
            RunJob(dlg, &st->job, [zip, dest, o, pf, s]() { s->job.xres = ExtractArchive(zip, fs::path(dest), o, pf); });
            break;
        }
        case IDCANCEL:
            if (st->job.running) {
                if (MessageBoxW(dlg, TR(L"Opresc dezarhivarea?\n(Fișierul la care s-a ajuns va fi șters.)", L"Stop extracting?\n(The file in progress will be deleted.)"), L"RedaotZip", MB_ICONQUESTION | MB_YESNO) == IDYES)
                    st->job.cancel = true;
            } else EndDialog(dlg, IDCANCEL);
            break;
        }
        return TRUE;
    case WM_RZ_PROGRESS: ShowProgress(dlg, &st->job, wp, TR(L"Extrag", L"Extracting")); return TRUE;
    case WM_RZ_DONE: {
        SetBusy(dlg, false, { IDC_DEST, IDC_BROWSE, IDC_PW, IDC_SHOWPW, IDC_OVERWRITE, IDC_OPENFOLDER });
        if (st->info.enc == EncKind::None) { EnableWindow(GetDlgItem(dlg, IDC_PW), FALSE); EnableWindow(GetDlgItem(dlg, IDC_SHOWPW), FALSE); }
        if (st->job.ok) {
            SecureClear(dlg, IDC_PW);
            if (st->job.xres.skippedUnsafe)
                Msg(dlg, TR(L"Atenție: ", L"Warning: ") + std::to_wstring(st->job.xres.skippedUnsafe) +
                             TR(L" fișiere au fost ignorate pentru că aveau căi periculoase (ex. „..\\”) care ar fi scris în afara folderului ales.", L" files were skipped because they had dangerous paths (e.g. \"..\\\") that would have written outside the chosen folder."),
                    MB_ICONWARNING);
            if (IsDlgButtonChecked(dlg, IDC_OPENFOLDER) == BST_CHECKED) OpenFolder(st->dest, false);
            EndDialog(dlg, IDOK);
        } else if (st->job.cancelled) {
            SendDlgItemMessageW(dlg, IDC_PROGRESS, PBM_SETPOS, 0, 0);
            SetDlgItemTextW(dlg, IDC_STATUS, TR(L"Dezarhivarea a fost oprită.", L"Extraction was stopped."));
        } else if (st->job.wrongPassword) {
            SendDlgItemMessageW(dlg, IDC_PROGRESS, PBM_SETPOS, 0, 0);
            SetDlgItemTextW(dlg, IDC_STATUS, TR(L"Parolă greșită. Încearcă din nou.", L"Wrong password. Try again."));
            if (st->sharedPassword) st->sharedPassword->clear();
            HWND pw = GetDlgItem(dlg, IDC_PW);
            SetFocus(pw); SendMessageW(pw, EM_SETSEL, 0, -1);
            MessageBeep(MB_ICONWARNING);
        } else {
            SendDlgItemMessageW(dlg, IDC_PROGRESS, PBM_SETPOS, 0, 0);
            SetDlgItemTextW(dlg, IDC_STATUS, TR(L"Eroare la dezarhivare.", L"Extraction error."));
            Msg(dlg, TR(L"Arhiva nu a putut fi extrasă complet:\n\n", L"The archive could not be fully extracted:\n\n") + st->job.error, MB_ICONERROR);
        }
        return TRUE;
    }
    case WM_CLOSE: SendMessageW(dlg, WM_COMMAND, IDCANCEL, 0); return TRUE;
    }
    return FALSE;
}

static int RunExtract(HWND owner, const std::vector<fs::path>& zips) {
    std::wstring sharedPw;
    for (size_t i = 0; i < zips.size(); ++i) {
        ExtractState st;
        st.zip = zips[i]; st.index = (int)i; st.count = (int)zips.size(); st.sharedPassword = &sharedPw;
        try { st.info = InspectArchive(st.zip); }
        catch (std::exception& e) {
            Msg(owner, TR(L"Nu pot deschide arhiva:\n", L"Cannot open the archive:\n") + zips[i].wstring() + L"\n\n" + W(e.what()), MB_ICONERROR);
            continue;
        }
        if (DialogBoxParamW(g_inst, MAKEINTRESOURCEW(IDD_EXTRACT), owner, ExtractProc, (LPARAM)&st) != IDOK) break;
    }
    SecureZeroMemory(&sharedPw[0], sharedPw.size() * sizeof(wchar_t));
    return 0;
}

// ------------------------------------------------------------------ fereastra principala
static bool PickFiles(HWND owner, std::vector<fs::path>& out, bool zipOnly) {
    IFileOpenDialog* d = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) return false;
    DWORD opts = 0; d->GetOptions(&opts);
    d->SetOptions(opts | FOS_ALLOWMULTISELECT | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
    if (zipOnly) {
        COMDLG_FILTERSPEC f[] = { { TR(L"Arhive ZIP", L"ZIP archives"), L"*.zip" }, { TR(L"Toate fișierele", L"All files"), L"*.*" } };
        d->SetFileTypes(2, f);
        d->SetTitle(TR(L"Alege arhivele ZIP de dezarhivat", L"Choose the ZIP archives to extract"));
    } else d->SetTitle(TR(L"Alege fișierele de arhivat", L"Choose the files to archive"));
    bool ok = false;
    if (SUCCEEDED(d->Show(owner))) {
        IShellItemArray* arr = nullptr;
        if (SUCCEEDED(d->GetResults(&arr))) {
            DWORD n = 0; arr->GetCount(&n);
            for (DWORD i = 0; i < n; ++i) {
                IShellItem* it = nullptr;
                if (SUCCEEDED(arr->GetItemAt(i, &it))) {
                    PWSTR p = nullptr;
                    if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) { out.push_back(fs::path(p)); CoTaskMemFree(p); }
                    it->Release();
                }
            }
            arr->Release();
            ok = !out.empty();
        }
    }
    d->Release();
    return ok;
}

static INT_PTR CALLBACK MainProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: SetupCommon(dlg); LocalizeMain(dlg); return TRUE;
    case WM_CTLCOLORSTATIC: return BrandColor(dlg, wp, lp);
    case WM_DPICHANGED: SetupCommon(dlg); return FALSE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_M_FILES: { std::vector<fs::path> v; if (PickFiles(dlg, v, false)) RunArchive(dlg, v); SetupCommon(dlg); break; }
        case IDC_M_FOLDER: { std::wstring d; if (BrowseFolder(dlg, d, TR(L"Alege folderul de arhivat", L"Choose the folder to archive"))) RunArchive(dlg, { fs::path(d) }); SetupCommon(dlg); break; }
        case IDC_M_EXTRACT: { std::vector<fs::path> v; if (PickFiles(dlg, v, true)) RunExtract(dlg, v); SetupCommon(dlg); break; }
        case IDCANCEL: EndDialog(dlg, 0); break;
        }
        return TRUE;
    case WM_CLOSE: EndDialog(dlg, 0); return TRUE;
    }
    (void)lp;
    return FALSE;
}

// ------------------------------------------------------------------ meniu clasic (rezerva) + reparare
static std::wstring SelfPath() {
    wchar_t b[MAX_PATH * 4]; DWORD n = GetModuleFileNameW(nullptr, b, MAX_PATH * 4);
    return std::wstring(b, n);
}

static void RegSet(HKEY root, const std::wstring& key, const wchar_t* name, const std::wstring& val) {
    HKEY h;
    if (RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &h, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(h, name, 0, REG_SZ, (const BYTE*)val.c_str(), (DWORD)((val.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(h);
    }
}

static int RegisterClassic(bool on) {
    const std::wstring base = L"Software\\Classes\\";
    const wchar_t* keys[] = { L"*\\shell\\RedaotZip.Archive", L"Directory\\shell\\RedaotZip.Archive",
                              L"SystemFileAssociations\\.zip\\shell\\RedaotZip.Extract" };
    if (!on) {
        for (auto k : keys) RegDeleteTreeW(HKEY_CURRENT_USER, (base + k).c_str());
    } else {
        std::wstring exe = SelfPath();
        for (int i = 0; i < 3; ++i) {
            bool ex = i == 2;
            std::wstring k = base + keys[i];
            RegSet(HKEY_CURRENT_USER, k, L"MUIVerb", ex ? TR(L"Dezarhivează cu REDAOT", L"Extract with REDAOT") : TR(L"Arhivează cu REDAOT (AES-256)", L"Archive with REDAOT (AES-256)"));
            RegSet(HKEY_CURRENT_USER, k, L"Icon", L"\"" + exe + L"\",0");
            RegSet(HKEY_CURRENT_USER, k, L"MultiSelectModel", L"Player");
            RegSet(HKEY_CURRENT_USER, k + L"\\command", nullptr, L"\"" + exe + (ex ? L"\" /xc \"%1\"" : L"\" /ac \"%1\""));
        }
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return 0;
}

// La pornirea Windows: verifica daca meniul din Windows 11 e inregistrat si il repara daca e nevoie.
static int Repair() {
    std::wstring dir = fs::path(SelfPath()).parent_path().wstring();
    std::wstring script = dir + L"\\scripts\\repair.ps1";
    if (GetFileAttributesW(script.c_str()) == INVALID_FILE_ATTRIBUTES) return 0;
    std::wstring cmd = L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"" + script + L"\"";
    STARTUPINFOW si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cl(cmd.begin(), cmd.end()); cl.push_back(0);
    if (CreateProcessW(nullptr, cl.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, dir.c_str(), &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 120000);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
    return 0;
}

// Meniul clasic porneste cate un proces pentru fiecare fisier selectat: le adunam intr-o singura fereastra.
static std::vector<fs::path> g_collected;
static ULONGLONG g_lastArrival = 0;
static LRESULT CALLBACK CollectorProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_COPYDATA) {
        auto* cd = (COPYDATASTRUCT*)lp;
        std::wstring p((const wchar_t*)cd->lpData, cd->cbData / sizeof(wchar_t));
        g_collected.push_back(fs::path(p));
        g_lastArrival = GetTickCount64();
        return TRUE;
    }
    return DefWindowProcW(h, m, wp, lp);
}

static bool CollectSelection(const wchar_t* mode, const fs::path& first, std::vector<fs::path>& all) {
    std::wstring cls = std::wstring(L"RedaotZipCollector") + mode;
    std::wstring mtxName = L"Local\\RedaotZip.Collect" + std::wstring(mode);
    HANDLE mtx = CreateMutexW(nullptr, TRUE, mtxName.c_str());
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        for (int i = 0; i < 40; ++i) {
            HWND w = FindWindowW(cls.c_str(), nullptr);
            if (w) {
                std::wstring p = first.wstring();
                COPYDATASTRUCT cd{ 1, (DWORD)(p.size() * sizeof(wchar_t)), (PVOID)p.c_str() };
                SendMessageW(w, WM_COPYDATA, 0, (LPARAM)&cd);
                if (mtx) CloseHandle(mtx);
                return false; // procesul principal se ocupa
            }
            Sleep(25);
        }
    }
    WNDCLASSW wc{}; wc.lpfnWndProc = CollectorProc; wc.hInstance = g_inst; wc.lpszClassName = cls.c_str();
    RegisterClassW(&wc);
    HWND w = CreateWindowW(cls.c_str(), L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, g_inst, nullptr);
    g_collected.push_back(first);
    g_lastArrival = GetTickCount64();
    while (GetTickCount64() - g_lastArrival < 700) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(15);
    }
    DestroyWindow(w);
    if (mtx) { ReleaseMutex(mtx); CloseHandle(mtx); }
    all = g_collected;
    return true;
}

// ------------------------------------------------------------------ argumente
static std::vector<fs::path> ReadArgs(int argc, wchar_t** argv, int start) {
    std::vector<fs::path> out;
    for (int i = start; i < argc; ++i) {
        std::wstring a = argv[i];
        if (!a.empty() && a[0] == L'@') {
            std::wstring listPath = a.substr(1);
            HANDLE h = CreateFileW(listPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER sz{}; GetFileSizeEx(h, &sz);
                std::vector<wchar_t> buf((size_t)sz.QuadPart / sizeof(wchar_t) + 1, 0);
                DWORD rd = 0; ReadFile(h, buf.data(), (DWORD)sz.QuadPart, &rd, nullptr);
                CloseHandle(h);
                std::wstring all(buf.data(), rd / sizeof(wchar_t));
                if (!all.empty() && all[0] == 0xFEFF) all.erase(0, 1);
                size_t p = 0;
                while (p < all.size()) {
                    size_t e = all.find_first_of(L"\r\n", p);
                    std::wstring line = all.substr(p, e == std::wstring::npos ? std::wstring::npos : e - p);
                    if (!line.empty()) { std::error_code ec; out.push_back(fs::absolute(fs::path(line), ec)); }
                    if (e == std::wstring::npos) break;
                    p = e + 1;
                }
                // stergem lista doar daca e in folderul nostru temporar
                wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
                if (_wcsnicmp(listPath.c_str(), (std::wstring(tmp) + L"RedaotZip\\").c_str(), wcslen(tmp) + 10) == 0)
                    DeleteFileW(listPath.c_str());
            }
        } else { std::error_code ec; out.push_back(fs::absolute(fs::path(a), ec)); }
    }
    return out;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    g_inst = inst;
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    auto pSetCtx = (BOOL(WINAPI*)(HANDLE))GetProcAddress(u32, "SetProcessDpiAwarenessContext");
    if (pSetCtx) pSetCtx((HANDLE)-4); // PER_MONITOR_AWARE_V2
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int rc = 0;
    std::wstring mode = argc > 1 ? argv[1] : L"";
    for (auto& c : mode) c = (wchar_t)towlower(c);

    if (mode == L"/a") {
        auto v = ReadArgs(argc, argv, 2);
        if (!v.empty()) rc = RunArchive(nullptr, v);
    } else if (mode == L"/x") {
        auto v = ReadArgs(argc, argv, 2);
        if (!v.empty()) rc = RunExtract(nullptr, v);
    } else if ((mode == L"/ac" || mode == L"/xc") && argc > 2) {
        std::vector<fs::path> all;
        if (CollectSelection(mode == L"/ac" ? L"A" : L"X", fs::path(argv[2]), all))
            rc = mode == L"/ac" ? RunArchive(nullptr, all) : RunExtract(nullptr, all);
    } else if (mode == L"/register-classic") rc = RegisterClassic(true);
    else if (mode == L"/unregister-classic") rc = RegisterClassic(false);
    else if (mode == L"/repair") rc = Repair();
    else if (mode == L"/selftest") rc = 42; // folosit de installer: confirma ca Windows lasa programul sa ruleze
    else if (mode == L"/version") Msg(nullptr, std::wstring(L"RedaotZip v") + RZ_VERSION_W + L"\n" + RZ_BRAND_W);
    else DialogBoxParamW(inst, MAKEINTRESOURCEW(IDD_MAIN), nullptr, MainProc, 0);

    LocalFree(argv);
    CoUninitialize();
    return rc;
}
