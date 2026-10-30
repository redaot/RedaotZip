# RedaotZip - diagnostic (made by REDAOT)
# Strange informatii despre instalare si meniul de click dreapta in fisierul
# RedaotZip-diagnostic.txt de pe Desktop. Nu modifica nimic in sistem.
$ErrorActionPreference = 'Continue'
$PkgName = 'REDAOT.RedaotZip'
$Dest    = Join-Path $env:ProgramFiles 'RedaotZip'
$Exe     = Join-Path $Dest 'RedaotZip.exe'
$Out     = Join-Path ([Environment]::GetFolderPath('Desktop')) 'RedaotZip-diagnostic.txt'
. (Join-Path $PSScriptRoot 'common.ps1')

$r = New-Object System.Collections.Generic.List[string]
function Add([string]$title, $value) {
    $r.Add(''); $r.Add("=== $title ===")
    if ($null -eq $value) { $r.Add('(nimic)'); return }
    $r.Add(($value | Out-String -Width 300).TrimEnd())
}
Write-Host 'RedaotZip - colectez informatii de diagnostic...' -ForegroundColor Cyan

Add 'Data' (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
$os = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion' -ErrorAction SilentlyContinue
Add 'Windows' ("{0} {1} build {2}.{3}" -f $os.ProductName, $os.DisplayVersion, $os.CurrentBuildNumber, $os.UBR)
Add 'Smart App Control (0=oprit, 1=pornit, 2=evaluare)' (Get-RzSmartAppControl)
Add 'Fisiere instalate' (Get-ChildItem $Dest -Recurse -File -ErrorAction SilentlyContinue | Select-Object FullName, Length, LastWriteTime | Format-Table -AutoSize)
Add 'Versiune instalata' (Get-Content (Join-Path $Dest 'version.txt') -ErrorAction SilentlyContinue)
Add 'Semnaturi' (Get-ChildItem $Dest -Include *.exe, *.dll, *.msix -Recurse -ErrorAction SilentlyContinue | ForEach-Object { Get-AuthenticodeSignature $_.FullName } | Select-Object Path, Status, StatusMessage | Format-List)
Add 'RedaotZip.exe poate rula' (Test-RzExeRuns $Exe)
Add 'Certificat REDAOT in TrustedPeople' (Get-ChildItem 'Cert:\LocalMachine\TrustedPeople' -ErrorAction SilentlyContinue | Where-Object { $_.Subject -eq 'CN=REDAOT' } | Select-Object Subject, Thumbprint, NotAfter | Format-List)
Add 'Pachete MSIX' (Get-AppxPackage -Name 'REDAOT.RedaotZip*' -ErrorAction SilentlyContinue | Format-List Name, Version, Status, InstallLocation, SignatureKind, PackageFullName)
Add 'Pachete MSIX - toti utilizatorii' (Get-AppxPackage -AllUsers -Name 'REDAOT.RedaotZip*' -ErrorAction SilentlyContinue | Format-List Name, Version, PackageUserInformation)
Add 'Meniu clasic in registry' (Test-RzClassicMenu)
Add 'Pornire automata' (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run' -Name RedaotZip -ErrorAction SilentlyContinue | Select-Object RedaotZip)

# poate fi creat obiectul de meniu? (activare COM prin pachet)
foreach ($c in @('1D0E488B-5D6A-4EBF-8072-5B131E14919E', '12BA1B38-1C92-42C9-AE82-6FF2BFE6C1E3')) {
    try { $o = [Activator]::CreateInstance([type]::GetTypeFromCLSID([guid]$c)); $res = 'OK' ; [void][Runtime.InteropServices.Marshal]::ReleaseComObject($o) }
    catch { $res = "EROARE: $($_.Exception.InnerException.Message) $($_.Exception.Message)" }
    Add "Activare COM $c" $res
}

$log = Join-Path $env:LOCALAPPDATA 'RedaotZip'
Add 'shell.log (ce face extensia de meniu)' (Get-Content (Join-Path $log 'shell.log') -Tail 60 -ErrorAction SilentlyContinue)
Add 'repair.log (verificarea de la pornire)' (Get-Content (Join-Path $log 'repair.log') -Tail 30 -ErrorAction SilentlyContinue)

$since = (Get-Date).AddDays(-2)
foreach ($ln in @('Microsoft-Windows-AppModel-Runtime/Admin', 'Microsoft-Windows-AppXDeploymentServer/Operational', 'Microsoft-Windows-AppXDeployment/Operational')) {
    $ev = Get-WinEvent -FilterHashtable @{ LogName = $ln; StartTime = $since } -MaxEvents 400 -ErrorAction SilentlyContinue |
          Where-Object { $_.Message -match 'REDAOT|RedaotZip' } | Select-Object -First 25 TimeCreated, Id, LevelDisplayName, Message
    Add "Evenimente $ln" ($ev | Format-List)
}
$ev = Get-WinEvent -FilterHashtable @{ LogName = 'Application'; StartTime = $since; Level = 1, 2 } -MaxEvents 300 -ErrorAction SilentlyContinue |
      Where-Object { $_.Message -match 'RedaotZip|dllhost' } | Select-Object -First 15 TimeCreated, Id, ProviderName, Message
Add 'Erori aplicatii (Application)' ($ev | Format-List)

$r | Set-Content -Path $Out -Encoding UTF8
Write-Host "Gata. Fisierul a fost salvat pe Desktop: $(Split-Path $Out -Leaf)" -ForegroundColor Green
Start-Process notepad.exe $Out
