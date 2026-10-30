# RedaotZip - instalare (made by REDAOT)
# - copiaza programul si manualul in C:\Program Files\RedaotZip
# - adauga certificatul REDAOT (necesar pentru meniul din Windows 11)
# - inregistreaza meniul de click dreapta (direct, fara "Show more options")
# - adauga verificarea automata la pornirea Windows
param([switch]$Silent, [string]$Lang = '')   # -Silent: rulat de Setup.exe; -Lang ro|en: limba programului
$ErrorActionPreference = 'Stop'
$Src     = Split-Path -Parent $PSScriptRoot
$Dest    = Join-Path $env:ProgramFiles 'RedaotZip'
$Version = (Get-Content (Join-Path $Src 'version.txt') -ErrorAction SilentlyContinue | Select-Object -First 1)
if (-not $Version) { $Version = '?' }
$ManualRo = 'Manual RedaotZip.pdf'
$ManualEn = 'RedaotZip Manual (EN).pdf'
. (Join-Path $PSScriptRoot 'common.ps1')

# limba: aleasa in Setup.exe (-Lang), altfel cea deja setata sau limba Windows
if ($Lang -notin @('ro', 'en')) { $Lang = Get-RzLanguage }
function T([string]$ro, [string]$en) { if ($Lang -eq 'en') { return $en }; return $ro }

function Say([string]$m, [string]$c = 'Gray') { Write-Host $m -ForegroundColor $c }
function Pause-End { if ($Silent) { return }; Write-Host ''; Read-Host (T 'Apasa Enter pentru a inchide fereastra' 'Press Enter to close this window') | Out-Null }

# --- drepturi de administrator (se cer o singura data)
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    if ($Silent) { Say (T 'Este nevoie de drepturi de administrator.' 'Administrator rights are required.') 'Red'; exit 1 }
    Say (T 'Se cer drepturi de administrator...' 'Requesting administrator rights...') 'Yellow'
    try {
        Start-Process powershell.exe -Verb RunAs -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"", '-Lang', $Lang)
    } catch { Say (T 'Instalarea a fost anulata (nu s-au acordat drepturi de administrator).' 'Installation cancelled (administrator rights were not granted).') 'Red'; Pause-End }
    exit
}

try {
    if (-not $Silent) { Clear-Host }
    Say '==========================================================' 'Cyan'
    Say "  RedaotZip v$Version  -  made by REDAOT" 'Cyan'
    Say (T '  Arhivator ZIP cu criptare AES-256' '  ZIP archiver with AES-256 encryption') 'Cyan'
    Say '==========================================================' 'Cyan'
    Say ''

    $build = [Environment]::OSVersion.Version.Build
    if ($build -lt 19041) { throw (T "Este nevoie de Windows 10 versiunea 2004 sau mai noua (build-ul tau: $build)." "Windows 10 version 2004 or newer is required (your build: $build).") }
    $isWin11 = $build -ge 22000

    # 1. oprim instantele vechi si scoatem pachetul vechi (elibereaza DLL-ul)
    Say (T '[1/7] Pregatesc instalarea...' '[1/7] Preparing the installation...')
    Get-Process RedaotZip -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Remove-RzPackages $Dest

    # 2. copiem fisierele
    Say (T "[2/7] Copiez programul in $Dest" "[2/7] Copying the program to $Dest")
    $zipDir = Join-Path $Dest 'meniu-zip'
    New-Item -ItemType Directory -Force -Path $Dest, (Join-Path $Dest 'scripts'), $zipDir | Out-Null
    $files = 'RedaotZip.exe', 'RedaotZipShell.dll', 'RedaotZip.msix', 'RedaotZip-Dezarhivare.msix', 'REDAOT.cer', 'version.txt', $ManualRo, $ManualEn, 'ISTORIC.txt', 'Diagnostic.cmd'
    # al doilea pachet (Dezarhiveaza) are folderul lui, cu propria copie a extensiei de meniu
    $copies = @($files | ForEach-Object { @{ From = $_; To = (Join-Path $Dest $_) } })
    $copies += @{ From = 'RedaotZipShell.dll'; To = (Join-Path $zipDir 'RedaotZipShell.dll') }
    $copies += @{ From = 'RedaotZip.exe';      To = (Join-Path $zipDir 'RedaotZip.exe') }
    foreach ($c in $copies) {
        $f = $c.From; $from = Join-Path $Src $f; $to = $c.To
        if (-not (Test-Path -LiteralPath $from)) { throw (T "Lipseste fisierul $f din folderul de instalare." "The file $f is missing from the installation folder.") }
        try { Copy-Item -LiteralPath $from -Destination $to -Force }
        catch {
            # fisier folosit de Explorer: il redenumim (permis si cand e incarcat) si copiem din nou
            $old = "$to.old-$([guid]::NewGuid().ToString('N').Substring(0,6))"
            Rename-Item -LiteralPath $to -NewName (Split-Path $old -Leaf) -Force
            Copy-Item -LiteralPath $from -Destination $to -Force
        }
    }
    Get-ChildItem $Dest -Recurse -Filter '*.old-*' -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
    Copy-Item (Join-Path $PSScriptRoot '*.ps1') (Join-Path $Dest 'scripts') -Force
    Get-ChildItem $Dest -Recurse -File | Unblock-File -ErrorAction SilentlyContinue
    $exe = Join-Path $Dest 'RedaotZip.exe'

    # 3. verificam ca Windows lasa programul sa ruleze
    Say (T '[3/7] Verific ca Windows permite rularea programului...' '[3/7] Checking that Windows allows the program to run...')
    $exeOk = Test-RzExeRuns $exe
    if (-not $exeOk) {
        Say (T '      ATENTIE: Windows a blocat RedaotZip.exe.' '      WARNING: Windows blocked RedaotZip.exe.') 'Red'
        $sac = Get-RzSmartAppControl
        if ($sac -eq 1) {
            Say (T '      Cauza: "Smart App Control" este pornit (Securitate Windows -> Control aplicatii si browser).' '      Cause: "Smart App Control" is on (Windows Security -> App & browser control).') 'Red'
            Say (T '      Acesta blocheaza programele care nu sunt semnate de o firma de certificare platita.' '      It blocks programs that are not signed by a paid certificate authority.') 'Red'
        } else {
            Say (T '      Verifica antivirusul (Windows Security -> Protection history) si permite RedaotZip.exe.' '      Check your antivirus (Windows Security -> Protection history) and allow RedaotZip.exe.') 'Red'
        }
    }

    # 4. certificatul REDAOT -> "Persoane de incredere" (doar pentru pachetul RedaotZip)
    if ($isWin11) {
        Say (T '[4/7] Adaug certificatul REDAOT...' '[4/7] Adding the REDAOT certificate...')
        Import-Certificate -FilePath (Join-Path $Dest 'REDAOT.cer') -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople' | Out-Null
    } else {
        Say (T '[4/7] Windows 10: certificatul nu este necesar.' '[4/7] Windows 10: the certificate is not needed.')
    }

    New-Item -Path 'HKLM:\SOFTWARE\REDAOT\RedaotZip' -Force | Out-Null
    Set-ItemProperty -Path 'HKLM:\SOFTWARE\REDAOT\RedaotZip' -Name 'Language' -Value $Lang
    Remove-ItemProperty -Path 'HKCU:\Software\REDAOT\RedaotZip' -Name 'Language' -ErrorAction SilentlyContinue

    # 5. meniul de click dreapta
    Say (T '[5/7] Inregistrez meniul de click dreapta...' '[5/7] Registering the right-click menu...')
    $menu = 'win11'
    $logFile = $null
    if (-not $isWin11) {
        # Windows 10 are un singur meniu: varianta clasica apare direct, fara "Show more options"
        $menu = 'win10'
        Set-RzClassicMenu $exe $true
    } else {
        try {
            Register-RzPackages $Dest
        } catch {
            $menu = 'classic'
            $msg = $_.Exception.Message
            Say (T "      Meniul Windows 11 nu a putut fi inregistrat: $msg" "      The Windows 11 menu could not be registered: $msg") 'Yellow'
            $logFile = Save-RzDeployLog $msg
            Say (T '      Folosesc meniul clasic (apare la "Show more options").' '      Using the classic menu (shown under "Show more options").') 'Yellow'
            Set-RzClassicMenu $exe $true
        }
    }
    if ($menu -eq 'win11') {
        # Windows 11 afiseaza singur comenzile meniului nou si in "Show more options";
        # meniul clasic ar aparea a doua oara, deci il scoatem.
        Set-RzClassicMenu $exe $false
    } elseif (-not (Test-RzClassicMenu)) { throw (T 'Meniul clasic nu a putut fi scris in registry.' 'The classic menu could not be written to the registry.') }

    # 6. pornire automata: la fiecare logare verifica/repara meniul (pentru toti utilizatorii)
    Say (T '[6/7] Adaug verificarea la pornirea Windows...' '[6/7] Adding the check at Windows startup...')
    Set-ItemProperty -Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run' -Name 'RedaotZip' -Value "`"$exe`" /repair"

    # 7. Start Menu (program + manual) + Programe si caracteristici
    Say (T '[7/7] Creez scurtaturile si intrarea de dezinstalare...' '[7/7] Creating shortcuts and the uninstall entry...')
    $smDir = Join-Path $env:ProgramData 'Microsoft\Windows\Start Menu\Programs'
    $ws = New-Object -ComObject WScript.Shell
    $lnk = $ws.CreateShortcut((Join-Path $smDir 'RedaotZip.lnk')); $lnk.TargetPath = $exe; $lnk.WorkingDirectory = $Dest
    $lnk.Description = 'RedaotZip - arhivator ZIP cu criptare AES-256 (made by REDAOT)'; $lnk.Save()
    $ManualName = (T $ManualRo $ManualEn)
    Remove-Item (Join-Path $smDir 'Manual RedaotZip.lnk'), (Join-Path $smDir 'RedaotZip Manual.lnk') -Force -ErrorAction SilentlyContinue
    $lnk = $ws.CreateShortcut((Join-Path $smDir (T 'Manual RedaotZip.lnk' 'RedaotZip Manual.lnk'))); $lnk.TargetPath = (Join-Path $Dest $ManualName)
    $lnk.Description = (T 'Manualul de utilizare RedaotZip' 'RedaotZip user manual'); $lnk.Save()
    $un = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\RedaotZip'
    New-Item -Path $un -Force | Out-Null
    $size = [int]((Get-ChildItem $Dest -Recurse -File | Measure-Object Length -Sum).Sum / 1KB)
    $props = @{
        DisplayName = 'RedaotZip'; DisplayVersion = $Version; Publisher = 'REDAOT'; DisplayIcon = "$exe,0"
        InstallLocation = $Dest; EstimatedSize = $size; NoModify = 1; NoRepair = 1
        UninstallString = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$Dest\scripts\uninstall.ps1`""
    }
    foreach ($k in $props.Keys) { Set-ItemProperty -Path $un -Name $k -Value $props[$k] }

    Say ''
    Say (T 'Instalare reusita!' 'Installation successful!') 'Green'
    if ($menu -eq 'classic') {
        Say (T 'Meniul a fost adaugat in varianta clasica ("Show more options").' 'The menu was added in its classic form ("Show more options").') 'Yellow'
        if ($logFile) { Say (T "Detaliile erorii sunt in fisierul de pe Desktop: $(Split-Path $logFile -Leaf)" "Error details are in this file on the Desktop: $(Split-Path $logFile -Leaf)") 'Yellow' }
    } else {
        Say (T 'In meniul de click dreapta apar acum:' 'The right-click menu now has:') 'Green'
        Say (T '   - "Arhiveaza cu REDAOT (AES-256)"  (pe fisiere si foldere)' '   - "Archive with REDAOT (AES-256)"  (on files and folders)')
        Say (T '   - "Dezarhiveaza cu REDAOT"         (pe arhive .zip)' '   - "Extract with REDAOT"            (on .zip archives)')
    }
    if (-not $exeOk) { Say (T 'Meniul apare, dar programul nu va porni pana nu il permiti in Windows (vezi mai sus).' 'The menu is there, but the program will not start until you allow it in Windows (see above).') 'Red' }
    Say (T "Manualul: Start Menu -> Manual RedaotZip  (sau $Dest\$ManualName)" "Manual: Start Menu -> RedaotZip Manual  (or $Dest\$ManualName)")
    Say (T "Daca meniul nu apare: ruleaza $Dest\Diagnostic.cmd si trimite fisierul creat pe Desktop." "If the menu does not appear: run $Dest\Diagnostic.cmd and send the file it creates on the Desktop.")
    Say ''
    # Explorer se reporneste automat ca meniul sa apara imediat
    Say (T 'Repornesc Explorer ca meniul sa apara imediat...' 'Restarting Explorer so the menu appears right away...')
    Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
    for ($i = 0; $i -lt 10 -and -not (Get-Process explorer -ErrorAction SilentlyContinue); $i++) { Start-Sleep -Milliseconds 500 }
    if (-not (Get-Process explorer -ErrorAction SilentlyContinue)) { Start-Process explorer.exe }
    Say (T 'Gata!' 'Done!') 'Green'
} catch {
    Say ''
    Say (T "EROARE: $($_.Exception.Message)" "ERROR: $($_.Exception.Message)") 'Red'
    Say (T 'Instalarea nu s-a terminat. Ruleaza din nou instalarea sau citeste manualul (sectiunea Probleme).' 'The installation did not finish. Run it again or read the manual (Troubleshooting section).') 'Red'
    Pause-End
    exit 1
}
Pause-End
exit 0
