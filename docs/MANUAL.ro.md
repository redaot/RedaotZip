# Manual RedaotZip

**RedaotZip v1.0.7 — made by REDAOT**

## Ce este RedaotZip

RedaotZip creează și deschide arhive ZIP protejate cu parolă și criptate cu AES-256. Funcționează din meniul de click dreapta al Windows 11 și Windows 10, direct, fără „Show more options”. Interfața este în română sau engleză.

Arhivele folosesc formatul standard WinZip AES, deci se deschid și cu **7-Zip, PeaZip, WinRAR sau WinZip**. RedaotZip deschide și arhive ZIP obișnuite, arhive AES create cu alte programe și arhive vechi cu ZipCrypto.

**Limitare importantă:** utilitarul ZIP inclus în Windows nu poate deschide arhive criptate AES-256. Afișează o eroare (de obicei 0x80004005) și nu cere parola. Nu e o greșeală a arhivei: Windows nu are deloc această funcție. Cine primește arhiva are nevoie de unul dintre programele de mai sus.

| Ce face | Detalii |
| --- | --- |
| Criptare | AES-256 pe conținutul fiecărui fișier, verificare de autenticitate HMAC-SHA1 |
| Compresie | Deflate, 4 niveluri; fișierele deja comprimate (jpg, mp4, zip…) se stochează direct |
| Mărime | Fără limită practică (ZIP64: fișiere de peste 4 GB, peste 65.535 de fișiere) |
| Nume de fișiere | Unicode, cu diacritice (ă, î, ș, ț, â) |
| Limbă | Română sau engleză (se alege la instalare) |
| Sistem | Windows 11 sau Windows 10 versiunea 2004+, 64 de biți |

## Instalare

Instalarea durează sub un minut și cere o singură dată drepturi de administrator.

1. Descarcă **`RedaotZip-1.0.7-Setup.exe`**.
2. Dă dublu-click pe el. Dacă apare „Windows protected your PC”, apasă **More info → Run anyway** (mesajul apare pentru că programul nu e semnat de o firmă de certificare plătită).
3. Răspunde **Da** la cererea de administrator.
4. Alege limba: **Română** sau **English**.
5. Apasă **Înainte**. La final Explorer se repornește automat (ferestrele de foldere deschise se închid) și meniul apare imediat.

Alternativ, fără Setup.exe: dezarhivează `RedaotZip-1.0.7-Windows.zip` și dă dublu-click pe **Instaleaza.cmd** (limba se ia după limba Windows).

Ce face instalarea:

- Copiază programul și manualul în `C:\Program Files\RedaotZip`.
- Pe Windows 11 adaugă certificatul REDAOT în „Persoane de încredere” (Trusted People). Windows 11 cere pachete semnate pentru meniul din primul nivel; certificatul e folosit doar pentru pachetele RedaotZip.
- Înregistrează meniul de click dreapta: pe Windows 11 două pachete mici (unul pentru „Arhivează”, unul pentru „Dezarhivează”), ca ambele opțiuni să apară direct, fără submeniu; pe Windows 10 meniul clasic, care acolo apare direct.
- Adaugă **verificarea la pornire**: la fiecare pornire a Windows, `RedaotZip.exe /repair` verifică invizibil (sub o secundă) dacă meniul e înregistrat și îl repune dacă un update de Windows l-a scos.
- Creează scurtăturile **RedaotZip** și **Manual RedaotZip** în Start Menu și intrarea din **Setări → Aplicații**.

Dacă meniul Windows 11 nu se poate înregistra, se pune meniul clasic (la „Show more options”), iar meniul nou se reîncearcă la fiecare pornire.

## Arhivare

Selectează unul sau mai multe fișiere ori foldere, click dreapta → **Arhivează cu REDAOT (AES-256)**. Se deschide fereastra de arhivare, cu „made by REDAOT • v1.0.7” jos în stânga.

1. **Arhiva care se creează:** implicit lângă fișiere, cu numele fișierului sau folderului (ex. `Documente.zip`). Butonul „...” alege alt loc sau alt nume.
2. **Parolă** și **Confirmă parola:** scrie aceeași parolă de două ori. Indicatorul arată puterea ei: slabă, medie, bună, foarte bună. „Arată parola” o face vizibilă.
3. **Compresie:** Normală e potrivită aproape mereu. Maximă e puțin mai mică și mai lentă. Fără compresie e cea mai rapidă, bună pentru poze și filme.
4. Apasă **Arhivează**. Bara arată progresul. **Oprește** anulează fără să lase o arhivă pe jumătate.
5. La final, **Arată arhiva** o deschide selectată în Explorer.

| Opțiune | Implicit | Ce face |
| --- | --- | --- |
| Fișier cu instrucțiuni pentru Windows | bifat | Pune în arhivă `!!! CITESTE-MA - ARHIVA CRIPTATA.txt`, necriptat, care explică ce program trebuie folosit. RedaotZip îl ignoră la dezarhivare. |
| Ascunde și numele fișierelor | bifat | Fișierele intră într-o arhivă internă `continut.zip`, criptată. Fără parolă nu se văd nici numele fișierelor. |

Dacă parola e scurtă sau conține diacritice, programul te avertizează înainte. Diacriticele merg în RedaotZip și 7-Zip, dar alte programe le pot interpreta diferit.

Fără click dreapta: deschide RedaotZip din Start Menu și alege **Arhivează fișiere...** sau **Arhivează un folder...**.

## Dezarhivare

Click dreapta pe o arhivă `.zip` → **Dezarhivează cu REDAOT**. Pe arhive apar ambele opțiuni, una sub alta: **Dezarhivează** și **Arhivează**.

1. Fereastra arată tipul de criptare (AES-256, ZipCrypto sau fără criptare) și câte fișiere conține arhiva.
2. **Extrage în folderul:** implicit un folder nou cu numele arhivei, lângă ea. Dacă arhiva conține deja un singur folder, se extrage direct lângă arhivă, fără folder în plus.
3. **Parolă:** scrie parola. La o arhivă necriptată câmpul e dezactivat.
4. Apasă **Extrage**. La parolă greșită apare „Parolă greșită. Încearcă din nou.” și nu se scrie nimic pe disc.
5. La final se deschide folderul cu fișierele (dacă „Deschide folderul după extragere” e bifat).

Fișierele care există deja nu se suprascriu: se salvează ca `fișier (2).txt`. Bifează **Suprascrie fișierele existente** ca să fie înlocuite.

**Mai multe arhive odată:** selectează-le pe toate, click dreapta → Dezarhivează cu REDAOT. Se deschid pe rând, iar parola scrisă la prima se completează la următoarele.

**Arhive cu nume ascunse** (`continut.zip`): RedaotZip extrage direct fișierele din interior. Cu 7-Zip sau WinRAR se extrage întâi `continut.zip`, care apoi se deschide fără parolă.

**Protecție:** fișierele din arhivă cu căi periculoase (ex. `..\..\Windows\fișier`) sunt ignorate, ca o arhivă să nu poată scrie în afara folderului ales. Programul anunță câte a ignorat.

## Trimiterea arhivei altcuiva

Destinatarul are nevoie de un program care știe AES-256. Utilitarul din Windows nu ajunge.

| Program | Preț | Cum se deschide arhiva |
| --- | --- | --- |
| [RedaotZip](https://github.com/redaot/RedaotZip) | gratuit | Click dreapta → Dezarhivează cu REDAOT |
| [7-Zip](https://www.7-zip.org) | gratuit | Click dreapta → 7-Zip → Extract Here, apoi parola |
| [PeaZip](https://peazip.github.io) | gratuit | Click dreapta → PeaZip → Extract here |
| [WinRAR](https://www.win-rar.com) | cu licență, perioadă de probă | Click dreapta → Extract Here |
| [WinZip](https://www.winzip.com) | cu licență | Dublu-click pe arhivă |

Dacă destinatarul încearcă totuși cu Windows, primește o eroare, dar vede în arhivă fișierul **!!! CITESTE-MA - ARHIVA CRIPTATA.txt**. Acesta nu e criptat și explică, în română și engleză, ce programe trebuie instalate, cu linkuri. Același mesaj e și în comentariul arhivei (îl afișează WinRAR).

**Trimite parola pe alt canal decât arhiva** (de exemplu arhiva pe email, parola prin SMS sau telefon).

## Securitate

AES-256 nu poate fi spart direct. Singurul atac realist e ghicirea parolei, așa că **siguranța arhivei depinde de parolă**.

| Ce e protejat | Ce NU e protejat |
| --- | --- |
| Conținutul fiecărui fișier (AES-256) | Numele fișierelor (doar dacă debifezi „Ascunde și numele fișierelor”) |
| Modificarea arhivei: orice schimbare e detectată la extragere (HMAC) | Mărimea aproximativă a arhivei |
| Fiecare fișier are cheia lui (sare aleatoare de 16 octeți) | Parola, dacă e scurtă sau ușor de ghicit |

Formatul WinZip AES derivă cheia din parolă cu un număr fix de pași (PBKDF2, 1.000 de iterații), impus de standard ca arhiva să se deschidă în 7-Zip, WinRAR și celelalte. De aceea o parolă lungă contează mult.

- Cel puțin **12 caractere**, ideal 16 sau mai multe.
- Litere mari și mici, cifre și simboluri, sau o frază din 4–5 cuvinte fără legătură (ex. `Lampa-Tren-Nor7-Cizma`).
- Fără date personale și fără diacritice.
- Altă parolă pentru fiecare arhivă importantă.

**Dacă uiți parola, arhiva nu mai poate fi deschisă** de nimeni. Nu există cheie de rezervă.

Criptarea veche **ZipCrypto** e slabă; RedaotZip nu o folosește la arhivare, doar deschide arhive vechi.

## Probleme și soluții

| Problemă | Soluție |
| --- | --- |
| Meniul nu apare după instalare | Repornește calculatorul. Dacă tot nu apare, rulează `C:\Program Files\RedaotZip\Diagnostic.cmd`: creează pe Desktop `RedaotZip-diagnostic.txt`. Trimite fișierul la REDAOT. |
| „Windows a blocat RedaotZip.exe” la instalare | **Smart App Control** sau antivirusul blochează programul. Smart App Control: Securitate Windows → Control aplicații și browser → Setări Smart App Control. Antivirus: Windows Security → Protection history → Allow. |
| Meniul apare doar la „Show more options” | Meniul Windows 11 nu s-a putut înregistra; pe Desktop e `RedaotZip-eroare-meniu.txt` cu detaliile. Trimite-l la REDAOT. |
| Meniul a dispărut după un update de Windows | Se repune singur la următoarea pornire, sau reinstalează. |
| „Windows protected your PC” | More info → Run anyway. |
| Explorer zice 0x80004005 la o arhivă | Arhiva e AES-256: deschide-o cu RedaotZip sau 7-Zip. |
| „Parolă greșită” deși parola e bună | Verifică Caps Lock, limba tastaturii și diacriticele; bifează „Arată parola”. |
| „Verificarea autenticității a eșuat” | Arhiva a fost modificată sau s-a deteriorat la copiere. |
| „Folosește compresia LZMA/BZip2…, nesuportată” | Arhiva a fost făcută cu setări speciale în 7-Zip. Deschide-o cu 7-Zip. |

Jurnale: `%LOCALAPPDATA%\RedaotZip\repair.log` (verificarea de la pornire) și `shell.log` (meniul).

## Dezinstalare și actualizare

**Dezinstalare:** **Setări → Aplicații → Aplicații instalate → RedaotZip → Dezinstalare** (sau `Dezinstaleaza.cmd`). Se scot meniul, pornirea automată, certificatul, scurtăturile și folderul programului. **Arhivele tale nu se șterg.**

**Actualizare:** rulează Setup.exe din versiunea nouă, peste cea veche. Versiunea curentă apare în fiecare fereastră, lângă „made by REDAOT”.
