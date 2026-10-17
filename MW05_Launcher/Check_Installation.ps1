# Read-only installation check, Windows PowerShell 5.1.
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetDirectoryName($PSScriptRoot)
$lines = New-Object 'System.Collections.Generic.List[string]'
$failures = 0
function Say([string]$s) { $lines.Add($s); Write-Host $s }
function IsX86([string]$path) {
    $bytes = [IO.File]::ReadAllBytes($path)
    if ($bytes.Length -lt 64 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) { return $false }
    $pe = [BitConverter]::ToInt32($bytes,60)
    if ($pe -lt 0 -or $pe -gt ($bytes.Length-26)) { return $false }
    return ([BitConverter]::ToUInt32($bytes,$pe) -eq 0x4550 -and
            [BitConverter]::ToUInt16($bytes,$pe+4) -eq 0x14c -and
            [BitConverter]::ToUInt16($bytes,$pe+24) -eq 0x10b)
}
try {
    Say ('MW05 installation check: ' + (Get-Date -Format o))
    Say ('Game folder: ' + $root)
    $manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'ExpectedFiles.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($item in $manifest.binaries.PSObject.Properties) {
        $p = Join-Path $root $item.Name
        if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { Say ('FAIL missing: ' + $item.Name); $failures++; continue }
        if (-not (IsX86 $p)) { Say ('FAIL not PE32/x86: ' + $item.Name); $failures++ }
        if ((Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash -ine $item.Value) {
            Say ('FAIL changed binary: ' + $item.Name); $failures++
        } else { Say ('OK original release binary: ' + $item.Name) }
    }
    # Dependencies are deliberately not redistributed. No automatic downloads.
    foreach ($item in $manifest.external_binaries.PSObject.Properties) {
        $p = Join-Path $root $item.Name
        if (-not (Test-Path -LiteralPath $p -PathType Leaf)) {
            Say ('FAIL external dependency missing: ' + $item.Name)
            Say ('  Obtain from: ' + $item.Value.source)
            $failures++; continue
        }
        if (-not (IsX86 $p)) {
            Say ('FAIL external dependency is not PE32/x86: ' + $item.Name)
            $failures++; continue
        }
        $actual = (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash
        Say ('EXTERNAL SHA256 ' + $item.Name + ': ' + $actual)
        if ($actual -ieq $item.Value.tested_sha256) {
            Say ('OK exact previously tested dependency: ' + $item.Name)
        } else {
            Say ('WARNING unverified dependency version: ' + $item.Name + '. Different hash does not by itself mean corruption.')
        }
    }
    $game = Join-Path $root 'speed.exe' 
    if (-not (Test-Path -LiteralPath $game -PathType Leaf)) { Say 'FAIL speed.exe missing: install into the game root.'; $failures++ }
    else {
        if (-not (IsX86 $game)) { Say 'FAIL speed.exe is not PE32/x86.'; $failures++ }
        $hash = (Get-FileHash -LiteralPath $game -Algorithm SHA256).Hash
        Say ('GAME SHA256: ' + $hash)
        if ($hash -ieq $manifest.tested_game_sha256) { Say 'OK exact tested game executable.' }
        else { Say 'WARNING different speed.exe. Compatibility is NOT confirmed; the mod may refuse its hooks. Do not obtain replacement EXEs from random websites.' }
    }
    $asi = @(Get-ChildItem -LiteralPath $root -Filter '*.asi' -Recurse -File -ErrorAction SilentlyContinue)
    foreach ($p in $asi) { Say ('ASI found: ' + $p.FullName) }
    if ($asi.Count -gt 1) { Say 'WARNING other ASIs found. Check for old DirectDrive duplicates and conflicts. Legitimate unrelated mods are not removed.' }
    $ini = Join-Path $root 'mw05_directdrive.ini'
    if (-not (Test-Path -LiteralPath $ini -PathType Leaf)) { Say 'FAIL mw05_directdrive.ini missing.'; $failures++ }
    else {
        Say 'INI (editable; no hash comparison):'
        Get-Content -LiteralPath $ini | ForEach-Object { Say ('  ' + $_) }
        Say 'CHECK manually: WheelRangeDegrees must match Pit House; Telemetry Enabled=1 Port=20066 for LED.'
    }
    foreach ($name in @('Play_MW05.cmd','Launch_MW05.ps1','TelemetryMarker.cs','PitHousePath.txt')) {
        if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot $name) -PathType Leaf)) { Say ('FAIL launcher file missing: ' + $name); $failures++ }
    }
    Say ('PowerShell: ' + $PSVersionTable.PSVersion.ToString())
    Say ('Pit House running: ' + (@(Get-Process -Name 'MOZA Pit House' -ErrorAction SilentlyContinue).Count -gt 0))
    Say ('Binary/file errors: ' + $failures)
    Say 'This is a static check, NOT proof of working input, hooks, receiver or physical LEDs.'
} catch { Say ('ERROR: ' + $_.Exception.Message); $failures++ }
try { $lines | Out-File -LiteralPath (Join-Path $PSScriptRoot 'installation_check.log') -Encoding UTF8 }
catch { Write-Warning 'Cannot write installation_check.log. Use a writable game folder.' }
if ($failures -gt 0) { exit 1 }
exit 0
