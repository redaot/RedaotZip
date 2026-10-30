#pragma once
// Limba interfetei: romana sau engleza.
// Ordine: registry HKCU, apoi HKLM  Software\REDAOT\RedaotZip  "Language" = "ro" | "en" (scrisa de installer),
// altfel limba Windows (romana -> ro, orice altceva -> en).
namespace rz { bool IsRo(); }
#define TR(ro, en) (rz::IsRo() ? (ro) : (en))
