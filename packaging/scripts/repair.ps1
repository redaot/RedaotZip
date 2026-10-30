# RedaotZip - verificare la pornirea Windows (ruleaza invizibil, fara drepturi de administrator).
# Daca meniul de click dreapta din Windows 11 lipseste sau e o versiune veche, il inregistreaza din nou.
$ErrorActionPreference = 'Stop'
$PkgName = 'REDAOT.RedaotZip'
$Dest    = Split-Path -Parent $PSScriptRoot
$Exe     = Join-Path $Dest 'RedaotZip.exe'
$Msix    = Join-Path $Dest 'RedaotZip.msix'
$LogDir  = Join-Path $env:LOCALAPPDATA 'RedaotZip'
$Log     = Join-Path $LogDir 'repair.log'
. (Join-Path $PSScriptRoot 'common.ps1')

function Log([string]$m) {
    try {
        New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
        if ((Test-Path $Log) -and (Get-Item $Log).Length -gt 200KB) { Remove-Item $Log -Force }
        Add-Content -Path $Log -Value "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')  $m"
    } catch {}
}

# Windows 10: meniul clasic (care acolo apare direct) e suficient
if ([Environment]::OSVersion.Version.Build -lt 22000) {
    if (-not (Test-RzClassicMenu)) {
        Set-RzClassicMenu $Exe $true
        Log 'Windows 10: meniul clasic a fost (re)inregistrat.'
    }
    exit 0
}
if (-not (Test-Path $Msix)) { exit 0 }
$want = (Get-Content (Join-Path $Dest 'version.txt') -ErrorAction SilentlyContinue | Select-Object -First 1)
if (Test-RzPackages $Dest $want) {
    # meniul nou e activ: Windows 11 il arata si in "Show more options", deci fara meniu clasic (altfel apare dublu)
    if (Test-RzClassicMenu) { Set-RzClassicMenu $Exe $false; Log 'Meniul clasic dublat a fost scos.' }
    exit 0
}

try {
    Register-RzPackages $Dest
    Set-RzClassicMenu $Exe $false
    Log 'Meniul Windows 11 a fost (re)inregistrat.'
} catch {
    Set-RzClassicMenu $Exe $true
    Log "Meniul Windows 11 nu a putut fi inregistrat ($($_.Exception.Message)); folosesc meniul clasic."
}
