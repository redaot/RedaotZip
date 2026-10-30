; RedaotZip - installer Setup.exe (made by REDAOT)
; Compilat cu: makensis -DVERSION=x.y.z -DSRCDIR=<folder distributie> -DOUTFILE=<Setup.exe> setup.nsi
; Installerul copiaza fisierele intr-un folder temporar si ruleaza scripts\install.ps1 -Silent -Lang ro|en,
; care face instalarea propriu-zisa (aceeasi logica precum Instaleaza.cmd).
Unicode true
Target amd64-unicode
SetCompressor /SOLID lzma
RequestExecutionLevel admin
ManifestDPIAware true

!include "MUI2.nsh"
!include "x64.nsh"

Name "RedaotZip ${VERSION}"
OutFile "${OUTFILE}"
BrandingText "RedaotZip ${VERSION}  -  made by REDAOT"
ShowInstDetails show

VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductName" "RedaotZip"
VIAddVersionKey /LANG=1033 "CompanyName" "REDAOT"
VIAddVersionKey /LANG=1033 "FileDescription" "RedaotZip Setup - ZIP archiver with AES-256 encryption"
VIAddVersionKey /LANG=1033 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "made by REDAOT"

!define MUI_ICON "..\assets\app.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "setup-welcome.bmp"
!define MUI_ABORTWARNING
!define MUI_LANGDLL_ALLLANGUAGES
!define MUI_LANGDLL_WINDOWTITLE "RedaotZip ${VERSION}"
!define MUI_LANGDLL_INFO "Alege limba / Choose the language:"
!define MUI_WELCOMEPAGE_TITLE "$(TxtWelcomeTitle)"
!define MUI_WELCOMEPAGE_TEXT "$(TxtWelcome)"
!define MUI_FINISHPAGE_TITLE "$(TxtFinishTitle)"
!define MUI_FINISHPAGE_TEXT "$(TxtFinish)"
!define MUI_FINISHPAGE_SHOWREADME "$PROGRAMFILES64\RedaotZip\$(TxtManualFile)"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "$(TxtOpenManual)"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_LANGUAGE "Romanian"
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_RESERVEFILE_LANGDLL

LangString TxtWelcomeTitle ${LANG_ROMANIAN} "Instalare RedaotZip ${VERSION}"
LangString TxtWelcomeTitle ${LANG_ENGLISH}  "RedaotZip ${VERSION} Setup"
LangString TxtWelcome ${LANG_ROMANIAN} "Arhivator ZIP cu criptare AES-256 - made by REDAOT.$\r$\n$\r$\nSe vor instala programul, manualul și opțiunile din meniul de click dreapta:$\r$\n$\r$\n   • Arhivează cu REDAOT (AES-256)$\r$\n   • Dezarhivează cu REDAOT$\r$\n$\r$\nLa final Explorer se repornește automat (ferestrele de foldere deschise se închid).$\r$\n$\r$\nApasă Înainte pentru a continua."
LangString TxtWelcome ${LANG_ENGLISH}  "ZIP archiver with AES-256 encryption - made by REDAOT.$\r$\n$\r$\nThis installs the program, the manual and the right-click menu options:$\r$\n$\r$\n   • Archive with REDAOT (AES-256)$\r$\n   • Extract with REDAOT$\r$\n$\r$\nAt the end Explorer restarts automatically (open folder windows will close).$\r$\n$\r$\nClick Next to continue."
LangString TxtFinishTitle ${LANG_ROMANIAN} "RedaotZip ${VERSION} a fost instalat"
LangString TxtFinishTitle ${LANG_ENGLISH}  "RedaotZip ${VERSION} is installed"
LangString TxtFinish ${LANG_ROMANIAN} "Dă click dreapta pe un fișier sau pe o arhivă .zip ca să folosești RedaotZip.$\r$\n$\r$\nmade by REDAOT"
LangString TxtFinish ${LANG_ENGLISH}  "Right-click a file or a .zip archive to use RedaotZip.$\r$\n$\r$\nmade by REDAOT"
LangString TxtOpenManual ${LANG_ROMANIAN} "Deschide manualul"
LangString TxtOpenManual ${LANG_ENGLISH}  "Open the manual"
LangString TxtManualFile ${LANG_ROMANIAN} "Manual RedaotZip.pdf"
LangString TxtManualFile ${LANG_ENGLISH}  "RedaotZip Manual (EN).pdf"
LangString TxtNeed64 ${LANG_ROMANIAN} "RedaotZip necesită Windows pe 64 de biți."
LangString TxtNeed64 ${LANG_ENGLISH}  "RedaotZip requires 64-bit Windows."
LangString TxtInstalling ${LANG_ROMANIAN} "Instalez RedaotZip ${VERSION}..."
LangString TxtInstalling ${LANG_ENGLISH}  "Installing RedaotZip ${VERSION}..."
LangString TxtOk ${LANG_ROMANIAN} "Instalare reușită."
LangString TxtOk ${LANG_ENGLISH}  "Installation successful."
LangString TxtFailed ${LANG_ROMANIAN} "Instalarea RedaotZip nu s-a terminat.$\r$\nVezi detaliile din fereastră sau manualul, secțiunea Probleme."
LangString TxtFailed ${LANG_ENGLISH}  "The RedaotZip installation did not finish.$\r$\nSee the details in the window or the manual, Troubleshooting section."
LangString LangCode ${LANG_ROMANIAN} "ro"
LangString LangCode ${LANG_ENGLISH}  "en"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "$(TxtNeed64)"
    Abort
  ${EndIf}
  !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd

Section "RedaotZip"
  InitPluginsDir
  SetOutPath "$PLUGINSDIR\pkg"
  File /r "${SRCDIR}\*.*"
  DetailPrint "$(TxtInstalling)"
  nsExec::ExecToLog '"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PLUGINSDIR\pkg\scripts\install.ps1" -Silent -Lang $(LangCode)'
  Pop $0
  SetOutPath "$TEMP"
  RMDir /r "$PLUGINSDIR\pkg"
  ${If} $0 == "0"
    DetailPrint "$(TxtOk)"
  ${Else}
    DetailPrint "Error code $0"
    MessageBox MB_ICONSTOP "$(TxtFailed)"
    Abort
  ${EndIf}
SectionEnd
