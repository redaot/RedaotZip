# RedaotZip - functii comune pentru scripturi (made by REDAOT)
# Textele cu diacritice sunt scrise prin coduri [char], ca scriptul sa mearga in orice codare.

# Limba: setarea scrisa de installer (HKCU, apoi HKLM), altfel limba Windows.
function Get-RzLanguage {
    foreach ($root in 'HKCU:', 'HKLM:') {
        try { $v = (Get-ItemProperty "$root\Software\REDAOT\RedaotZip" -Name Language -ErrorAction Stop).Language } catch { $v = $null }
        if ($v -in @('ro', 'en')) { return $v }
    }
    if ((Get-UICulture).TwoLetterISOLanguageName -eq 'ro') { return 'ro' }
    return 'en'
}

function Get-RzText([string]$kind) {
    $a = [char]0x103   # ă
    $en = (Get-RzLanguage) -eq 'en'
    if ($kind -eq 'archive') { if ($en) { return 'Archive with REDAOT (AES-256)' }; return "Arhiveaz$a cu REDAOT (AES-256)" }
    if ($en) { return 'Extract with REDAOT' }
    return "Dezarhiveaz$a cu REDAOT"
}

# Meniul clasic (Windows 10 / rezerva pe Windows 11), scris direct in registry pentru utilizatorul curent.
function Set-RzClassicMenu([string]$Exe, [bool]$On) {
    $base = 'HKCU:\Software\Classes'
    $items = @(
        @{ Key = '*\shell\RedaotZip.Archive';                              Kind = 'archive'; Arg = '/ac' },
        @{ Key = 'Directory\shell\RedaotZip.Archive';                      Kind = 'archive'; Arg = '/ac' },
        @{ Key = 'SystemFileAssociations\.zip\shell\RedaotZip.Extract';    Kind = 'extract'; Arg = '/xc' }
    )
    foreach ($it in $items) {
        $k = Join-Path $base $it.Key
        if (-not $On) { if (Test-Path -LiteralPath $k) { Remove-Item -LiteralPath $k -Recurse -Force }; continue }
        New-Item -Path "$k\command" -Force | Out-Null   # New-Item -Path nu interpreteaza * ca wildcard la creare
        Set-ItemProperty -LiteralPath $k -Name 'MUIVerb' -Value (Get-RzText $it.Kind)
        Set-ItemProperty -LiteralPath $k -Name 'Icon' -Value "`"$Exe`",0"
        Set-ItemProperty -LiteralPath $k -Name 'MultiSelectModel' -Value 'Player'
        Set-ItemProperty -LiteralPath "$k\command" -Name '(default)' -Value "`"$Exe`" $($it.Arg) `"%1`""
    }
}

function Test-RzClassicMenu {
    return (Test-Path -LiteralPath 'HKCU:\Software\Classes\*\shell\RedaotZip.Archive\command') -and
           (Test-Path -LiteralPath 'HKCU:\Software\Classes\SystemFileAssociations\.zip\shell\RedaotZip.Extract\command')
}

# Verifica daca Windows (Smart App Control / antivirus) lasa RedaotZip.exe sa ruleze.
function Test-RzExeRuns([string]$Exe) {
    try {
        $p = Start-Process -FilePath $Exe -ArgumentList '/selftest' -Wait -PassThru -WindowStyle Hidden -ErrorAction Stop
        return ($p.ExitCode -eq 42)
    } catch { return $false }
}

function Get-RzSmartAppControl {
    try { return (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy' -ErrorAction Stop).VerifiedAndReputablePolicyState } catch { return $null }
}

# Salveaza pe Desktop detaliile unei erori de instalare a pachetului (pentru depanare).
function Save-RzDeployLog([string]$Message) {
    try {
        $file = Join-Path ([Environment]::GetFolderPath('Desktop')) 'RedaotZip-eroare-meniu.txt'
        $lines = @("RedaotZip - eroare la inregistrarea meniului Windows 11", (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'),
                   "Windows: $([Environment]::OSVersion.VersionString)", '', $Message, '')
        $m = [regex]::Match($Message, '[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}')
        if ($m.Success) {
            $lines += '--- Get-AppPackageLog ---'
            $lines += (Get-AppPackageLog -ActivityID $m.Value -ErrorAction SilentlyContinue | Format-List TimeCreated, Id, Message | Out-String -Width 400)
        }
        $lines | Set-Content -Path $file -Encoding UTF8
        return $file
    } catch { return $null }
}

# --- Pachetele pentru meniul Windows 11 -------------------------------------------------
# Windows 11 grupeaza intr-un submeniu comenzile multiple ale aceluiasi pachet, asa ca fiecare
# comanda are pachetul ei: astfel "Arhiveaza" si "Dezarhiveaza" apar direct, una sub alta.
function Get-RzPackages([string]$Dest) {
    return @(
        @{ Name = 'REDAOT.RedaotZip';             Msix = (Join-Path $Dest 'RedaotZip.msix');             Location = $Dest },
        @{ Name = 'REDAOT.RedaotZip.Dezarhivare'; Msix = (Join-Path $Dest 'RedaotZip-Dezarhivare.msix'); Location = (Join-Path $Dest 'meniu-zip') }
    )
}

function Remove-RzPackages([string]$Dest, [switch]$AllUsers) {
    foreach ($p in (Get-RzPackages $Dest)) {
        Get-AppxPackage -Name $p.Name -ErrorAction SilentlyContinue | Remove-AppxPackage -ErrorAction SilentlyContinue
        if ($AllUsers) { try { Get-AppxPackage -AllUsers -Name $p.Name -ErrorAction SilentlyContinue | Remove-AppxPackage -AllUsers -ErrorAction SilentlyContinue } catch {} }
    }
}

# Inregistreaza ambele pachete; arunca eroare daca unul nu reuseste.
function Register-RzPackages([string]$Dest) {
    foreach ($p in (Get-RzPackages $Dest)) {
        Get-AppxPackage -Name $p.Name -ErrorAction SilentlyContinue | Remove-AppxPackage -ErrorAction SilentlyContinue
        Add-AppxPackage -Path $p.Msix -ExternalLocation $p.Location
    }
}

# $true daca ambele pachete sunt inregistrate, in versiunea dorita (sau oricare, daca $Version e gol)
function Test-RzPackages([string]$Dest, [string]$Version) {
    foreach ($p in (Get-RzPackages $Dest)) {
        $pkg = Get-AppxPackage -Name $p.Name -ErrorAction SilentlyContinue
        if (-not $pkg) { return $false }
        if ($Version -and $pkg.Version -ne "$Version.0") { return $false }
        if ("$($pkg.Status)" -notin @('Ok', '')) { return $false }
    }
    return $true
}
