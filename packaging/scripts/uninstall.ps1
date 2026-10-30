# RedaotZip - dezinstalare completa (made by REDAOT)
$ErrorActionPreference = 'Continue'
$PkgName = 'REDAOT.RedaotZip'
$Dest    = Join-Path $env:ProgramFiles 'RedaotZip'

. (Join-Path $PSScriptRoot 'common.ps1')
$Lang = Get-RzLanguage
function T([string]$ro, [string]$en) { if ($Lang -eq 'en') { return $en }; return $ro }
function Say([string]$m, [string]$c = 'Gray') { Write-Host $m -ForegroundColor $c }

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    try { Start-Process powershell.exe -Verb RunAs -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"") } catch {}
    exit
}

Clear-Host
Say (T 'RedaotZip - dezinstalare  (made by REDAOT)' 'RedaotZip - uninstall  (made by REDAOT)') 'Cyan'
Say ''
$r = Read-Host (T 'Sigur vrei sa dezinstalezi RedaotZip? Arhivele tale NU se sterg. (D/N)' 'Uninstall RedaotZip? Your archives are NOT deleted. (Y/N)')
if ($r -notmatch '^[dDyY]') { exit }

Set-Location $env:TEMP
Say (T 'Opresc programul...' 'Stopping the program...')
Get-Process RedaotZip -ErrorAction SilentlyContinue | Stop-Process -Force

Say (T 'Scot meniul de click dreapta...' 'Removing the right-click menu...')
$exe = Join-Path $Dest 'RedaotZip.exe'
Remove-RzPackages $Dest -AllUsers
Set-RzClassicMenu $exe $false

Say (T 'Scot pornirea automata...' 'Removing the startup check...')
Remove-ItemProperty -Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run' -Name 'RedaotZip' -ErrorAction SilentlyContinue
Remove-ItemProperty -Path 'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run' -Name 'RedaotZip' -ErrorAction SilentlyContinue

Say (T 'Scot certificatul REDAOT...' 'Removing the REDAOT certificate...')
$cer = Join-Path $Dest 'REDAOT.cer'
if (Test-Path $cer) {
    $thumb = (New-Object System.Security.Cryptography.X509Certificates.X509Certificate2 $cer).Thumbprint
    Get-ChildItem 'Cert:\LocalMachine\TrustedPeople' | Where-Object { $_.Thumbprint -eq $thumb } | Remove-Item -ErrorAction SilentlyContinue
}

Say (T 'Sterg fisierele programului...' 'Deleting the program files...')
Remove-Item (Join-Path $env:ProgramData 'Microsoft\Windows\Start Menu\Programs\RedaotZip.lnk') -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $env:ProgramData 'Microsoft\Windows\Start Menu\Programs\Manual RedaotZip.lnk') -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $env:ProgramData 'Microsoft\Windows\Start Menu\Programs\RedaotZip Manual.lnk') -Force -ErrorAction SilentlyContinue
Remove-Item 'HKLM:\SOFTWARE\REDAOT\RedaotZip' -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\RedaotZip' -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $env:TEMP 'RedaotZip') -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $Dest -Recurse -Force -ErrorAction SilentlyContinue
if (Test-Path $Dest) {
    Say (T 'Unele fisiere sunt inca folosite de Explorer; repornesc Explorer si incerc din nou...' 'Some files are still in use by Explorer; restarting Explorer and trying again...') 'Yellow'
    Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
    Get-Process dllhost -ErrorAction SilentlyContinue | ForEach-Object {
        try { if ($_.Modules.FileName -like "$Dest*") { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue } } catch {}
    }
    Start-Sleep -Seconds 2
    Remove-Item $Dest -Recurse -Force -ErrorAction SilentlyContinue
    if (-not (Get-Process explorer -ErrorAction SilentlyContinue)) { Start-Process explorer.exe }
}
Say ''
if (Test-Path $Dest) { Say (T "Aproape gata. Folderul $Dest se poate sterge manual dupa repornirea calculatorului." "Almost done. The folder $Dest can be deleted manually after a restart.") 'Yellow' }
else { Say (T 'RedaotZip a fost dezinstalat complet.' 'RedaotZip was completely uninstalled.') 'Green' }
Write-Host ''
Read-Host (T 'Apasa Enter pentru a inchide fereastra' 'Press Enter to close this window') | Out-Null
