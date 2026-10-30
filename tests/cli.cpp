// Utilitar de test pentru motor (Linux/Windows):
//   cli a <out.zip> <parola|-> <nivel> [hide] <fisiere...>
//   cli x <arhiva.zip> <dest> <parola|-> [overwrite]
//   cli i <arhiva.zip>
#include "../src/zipcore.h"
#include <cstdio>
#include <cstring>
using namespace rz;
int main(int argc, char** argv) {
    if (argc < 3) return 2;
    try {
        if (!strcmp(argv[1], "a")) {
            ArchiveOptions o; o.password = strcmp(argv[3], "-") ? argv[3] : ""; o.level = atoi(argv[4]);
            int i = 5; if (i < argc && !strcmp(argv[i], "hide")) { o.hideNames = true; ++i; }
            std::vector<fs::path> in; for (; i < argc; ++i) in.push_back(Utf8ToPath(argv[i]));
            CreateArchive(in, Utf8ToPath(argv[2]), o, nullptr);
        } else if (!strcmp(argv[1], "x")) {
            ExtractOptions o; o.password = strcmp(argv[4], "-") ? argv[4] : ""; o.overwrite = argc > 5;
            auto r = ExtractArchive(Utf8ToPath(argv[2]), Utf8ToPath(argv[3]), o, nullptr);
            printf("files=%llu dirs=%llu unsafe=%llu\n", (unsigned long long)r.files, (unsigned long long)r.dirs, (unsigned long long)r.skippedUnsafe);
        } else if (!strcmp(argv[1], "i")) {
            auto i = InspectArchive(Utf8ToPath(argv[2]));
            printf("files=%llu dirs=%llu bytes=%llu enc=%d bits=%d nested=%d warn=%d root=%s\n",
                   (unsigned long long)i.files, (unsigned long long)i.dirs, (unsigned long long)i.totalBytes,
                   (int)i.enc, i.aesBits, i.nested, i.hasWarningFile, i.singleRoot.c_str());
        }
    } catch (WrongPassword& e) { printf("WRONGPW: %s\n", e.what()); return 3; }
    catch (std::exception& e) { printf("ERR: %s\n", e.what()); return 1; }
    return 0;
}
