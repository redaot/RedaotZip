// Varianta Windows a utilitarului de test (argumente UTF-16 -> UTF-8).
#include <windows.h>
#include <string>
#include <vector>
#define main cli_main
#include "cli.cpp"
#undef main
int wmain(int argc, wchar_t** argv) {
    std::vector<std::string> s; std::vector<char*> p;
    for (int i = 0; i < argc; ++i) {
        int n = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        std::string a(n, 0); WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, &a[0], n, nullptr, nullptr);
        a.resize(n - 1); s.push_back(a);
    }
    for (auto& a : s) p.push_back(&a[0]);
    return cli_main(argc, p.data());
}
