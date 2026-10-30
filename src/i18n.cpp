#include "i18n.h"
#ifdef _WIN32
#include <windows.h>
#include <cwchar>
static int ReadLang(HKEY root) {
    wchar_t v[16] = {0}; DWORD sz = sizeof(v) - sizeof(wchar_t);
    if (RegGetValueW(root, L"Software\\REDAOT\\RedaotZip", L"Language", RRF_RT_REG_SZ, nullptr, v, &sz) != ERROR_SUCCESS) return -1;
    if (_wcsicmp(v, L"ro") == 0) return 1;
    if (_wcsicmp(v, L"en") == 0) return 0;
    return -1;
}
bool rz::IsRo() {
    static int cached = -2;
    if (cached == -2) {
        int v = ReadLang(HKEY_CURRENT_USER);
        if (v < 0) v = ReadLang(HKEY_LOCAL_MACHINE);
        if (v < 0) v = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_ROMANIAN ? 1 : 0;
        cached = v;
    }
    return cached == 1;
}
#else
#include <cstdlib>
#include <cstring>
bool rz::IsRo() { const char* e = getenv("RZ_LANG"); return !(e && strcmp(e, "en") == 0); }
#endif
