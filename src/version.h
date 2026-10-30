#pragma once
// Versiunea programului - modifica aici si in build.sh (APP_VERSION)
#define RZ_VER_MAJOR 1
#define RZ_VER_MINOR 0
#define RZ_VER_PATCH 8
#define RZ_VER_BUILD 0

#define RZ_STR2(x) #x
#define RZ_STR(x) RZ_STR2(x)
#define RZ_VERSION_STR RZ_STR(RZ_VER_MAJOR) "." RZ_STR(RZ_VER_MINOR) "." RZ_STR(RZ_VER_PATCH)
#define RZ_VERSION_W L"" RZ_VERSION_STR
#define RZ_APP_NAME "RedaotZip"
#define RZ_APP_NAME_W L"RedaotZip"
#define RZ_BRAND_W L"made by REDAOT"
#define RZ_HOMEPAGE "https://github.com/redaot/RedaotZip/releases"
#define RZ_HOMEPAGE_W L"https://github.com/redaot/RedaotZip/releases"
