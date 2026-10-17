# Windows PowerShell 5.1, one-click launch explicitly requested by user.
# NO confirmation prompt. NO startup task, registry, firewall, INI or profile edits.
# Starts Pit House if needed, then selected speed.exe, then our locally-built marker.
param(
    [string]$GameExe = '',
    [string]$PitHouseExe = ''
)
$ErrorActionPreference = 'Stop'
# Portable release v1.2: only path resolution/preflight added to the tested v1.1.
# Keep this whole folder directly inside the game root. No registry writes.
if ([string]::IsNullOrWhiteSpace($GameExe)) {
    $GameExe = Join-Path ([IO.Path]::GetDirectoryName($PSScriptRoot)) 'speed.exe'
}
function ResolvePitHousePath {
    if (-not [string]::IsNullOrWhiteSpace($PitHouseExe)) { return $PitHouseExe }
    $config = Join-Path $PSScriptRoot 'PitHousePath.txt'
    if (Test-Path -LiteralPath $config -PathType Leaf) {
        $lines = @(Get-Content -LiteralPath $config -Encoding UTF8 | Where-Object {
            -not [string]::IsNullOrWhiteSpace($_) -and -not $_.TrimStart().StartsWith('#')
        })
        if ($lines.Count -gt 1) { throw 'PitHousePath.txt must contain only one uncommented full executable path.' }
        if ($lines.Count -eq 1) {
            $configured = $lines[0].Trim().Trim('"')
            if (-not [IO.Path]::IsPathRooted($configured)) { throw 'Use a full path in PitHousePath.txt.' }
            return $configured
        }
    }
    $candidates = @()
    foreach ($programRoot in @($env:ProgramFiles, ${env:ProgramFiles(x86)}, $env:LOCALAPPDATA)) {
        if ([string]::IsNullOrWhiteSpace($programRoot)) { continue }
        foreach ($suffix in @('MOZA Pit House\bin\MOZA Pit House.exe',
                             'MOZA Pit House\MOZA Pit House.exe',
                             'MOZA Racing\MOZA Pit House\bin\MOZA Pit House.exe',
                             'MOZA Racing\MOZA Pit House.exe')) {
            $candidates += Join-Path $programRoot $suffix
        }
    }
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    throw 'Pit House was not found. Start MOZA Pit House manually and retry, or put its full EXE path in MW05_Launcher\PitHousePath.txt. See README_RU.txt.'
}
$report = Join-Path $PSScriptRoot 'auto_telemetry_launcher.log'
$child=$null
$game=$null
$work=$null
$mutex=$null
$ownsMutex=$false
$failed=$false
$lease=$null
$markerReport=$null
$heartbeatVersion=0
function Note([string]$s) {
    $line='[' + (Get-Date -Format o) + '] ' + $s
    Write-Host $line
    try { Add-Content -LiteralPath $report -Value $line -Encoding UTF8 -ErrorAction Stop }
    catch { Write-Warning ('Cannot write launcher log: ' + $_.Exception.Message) }
}
function ReceiverPresent {
    try {
        $rows=@(Get-NetUDPEndpoint -LocalPort 20066 -ErrorAction SilentlyContinue)
        foreach($row in $rows) {
            try {
                $owner=Get-Process -Id $row.OwningProcess -ErrorAction Stop
                if($owner.ProcessName -eq 'MOZA Pit House') {
                    Note ('RECEIVER observed local=' + $row.LocalAddress + ':20066 ownerPID=' + $row.OwningProcess)
                    return $true
                }
            } catch {}
        }
    } catch { Note ('Receiver query unavailable: ' + $_.Exception.Message) }
    return $false
}
try {
    if ($PSVersionTable.PSEdition -eq 'Core') { throw 'Use Windows PowerShell (powershell.exe), not pwsh.' }
    $mutex=New-Object System.Threading.Mutex($false,'Local\MW05_AutoTelemetry_Launcher_v1')
    try { $ownsMutex=$mutex.WaitOne(0) }
    catch [System.Threading.AbandonedMutexException] { $ownsMutex=$true }
    if (-not $ownsMutex) { throw 'The MW05 telemetry launcher is already running. No second game or marker started.' }
    Note 'BEGIN launcher v1.2 portable - no YES prompt; no changes to ASI/INI/Pit House settings.'
    Note 'REQUIREMENT: Pit House preset auto-loading must already be OFF; this tool cannot verify that setting.'
    if (-not (Test-Path -LiteralPath $GameExe -PathType Leaf)) { throw ('Game executable not found: ' + $GameExe) }
    $GameExe=[IO.Path]::GetFullPath($GameExe)
    $gameRoot=[IO.Path]::GetDirectoryName($GameExe)
    foreach ($relative in @('dinput8.dll','MOZA_SDK.dll','MW05_MozaBridge.dll',
                            'mw05_directdrive.ini','scripts\mw05_mirror_probe.asi')) {
        if (-not (Test-Path -LiteralPath (Join-Path $gameRoot $relative) -PathType Leaf)) {
            throw ('Missing installed component: ' + $relative + '. Copy the CONTENTS of COPY_TO_GAME into the folder containing speed.exe.')
        }
    }
    if ([IO.Path]::GetFileName($GameExe) -ine 'speed.exe') { throw 'GameExe must point to speed.exe.' }
    $existingGames=@(Get-Process -Name speed -ErrorAction SilentlyContinue)
    if ($existingGames.Count -gt 0) { throw 'speed.exe is already running. Close the game and old test launchers, then use this launcher.' }
    if (@(Get-Process -Name ForzaHorizon6 -ErrorAction SilentlyContinue).Count -gt 0) {
        throw 'Actual ForzaHorizon6 or another compatibility marker is running. Close it yourself first; no process killed.'
    }
    $source=Join-Path $PSScriptRoot 'TelemetryMarker.cs'
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw 'TelemetryMarker.cs is missing. Extract the whole package.' }
    $work=Join-Path ([IO.Path]::GetTempPath()) ('MW05_AutoTelemetry_' + [Guid]::NewGuid().ToString('N'))
    [void](New-Item -ItemType Directory -Path $work)
    $markerExe=Join-Path $work 'forzahorizon6.exe'
    # Compile our source before opening the game. No downloaded or patched executable.
    Add-Type -Path $source -OutputAssembly $markerExe -OutputType WindowsApplication
    Note 'Local compatibility marker compiled (no separate console).'
    $pit=@(Get-Process -Name 'MOZA Pit House' -ErrorAction SilentlyContinue)
    if ($pit.Count -eq 0) {
        $PitHouseExe=ResolvePitHousePath
        if (-not (Test-Path -LiteralPath $PitHouseExe -PathType Leaf)) { throw ('Pit House executable not found: ' + $PitHouseExe) }
        $PitHouseExe=[IO.Path]::GetFullPath($PitHouseExe)
        Start-Process -FilePath $PitHouseExe -WorkingDirectory ([IO.Path]::GetDirectoryName($PitHouseExe)) | Out-Null
        Note ('Pit House start requested: ' + $PitHouseExe)
        $wait=[Diagnostics.Stopwatch]::StartNew()
        do {
            Start-Sleep -Milliseconds 500
            $pit=@(Get-Process -Name 'MOZA Pit House' -ErrorAction SilentlyContinue)
        } while ($pit.Count -eq 0 -and $wait.Elapsed.TotalSeconds -lt 20)
        if ($pit.Count -eq 0) { throw 'Pit House did not appear within20seconds. Start it manually, then retry.' }
        Start-Sleep -Seconds 2
        Note 'Pit House process found; full initialization/receiver readiness is checked separately.'
    } else { Note 'Pit House already running; left unchanged.' }
    # Recheck after compilation/startup to avoid an accidental second game.
    if (@(Get-Process -Name speed -ErrorAction SilentlyContinue).Count -gt 0) { throw 'speed.exe appeared during setup; no second copy started.' }
    if (@(Get-Process -Name ForzaHorizon6 -ErrorAction SilentlyContinue).Count -gt 0) { throw 'Another ForzaHorizon6/marker appeared during setup; nothing killed.' }
    $game=Start-Process -FilePath $GameExe -WorkingDirectory ([IO.Path]::GetDirectoryName($GameExe)) -PassThru
    $game.Refresh()
    if ($game.HasExited) { throw 'The launched speed.exe exited immediately. Check the normal game startup; helper was not started.' }
    # No cross-process StartTime reads by helper. Only this launcher's game Process object is watched.
    $lease=Join-Path $work 'supervisor.lease'
    $markerReport=Join-Path $work 'marker_status.log'
    $token=[Guid]::NewGuid().ToString('N')
    [IO.File]::WriteAllText($lease,$token,[Text.Encoding]::ASCII)
    $leaseBase=[DateTime]::UtcNow
    # Quote arguments: the temporary path may contain spaces or Cyrillic characters.
    $markerArgs=@(('"{0}"' -f $lease),$token,('"{0}"' -f $markerReport))
    $child=Start-Process -FilePath $markerExe -ArgumentList $markerArgs -WorkingDirectory $work -PassThru
    Note ('GAME started PID=' + $game.Id + ' path=' + $GameExe)
    Note ('MARKER started PID=' + $child.Id + ' temporaryPath=' + $markerExe)
    Note 'LEASE heartbeat every1second; marker exits after15seconds without updates. No PID/StartTime check in helper.'
    Note 'Return to the game. This minimized PowerShell supervisor must stay running; no minute limit.'
    $observe=[Diagnostics.Stopwatch]::StartNew()
    $receiverSeen=$false
    $receiverWarning=$false
    $tick=0
    while ($true) {
        Start-Sleep -Seconds 1
        $game.Refresh(); $child.Refresh()
        if ($game.HasExited) { Note ('GAME exited, code=' + $game.ExitCode); break }
        if ($child.HasExited) { throw ('Compatibility helper exited unexpectedly, code=' + $child.ExitCode + '. See MARKER_DIAG lines below. Game remains running.') }
        # Change a private file timestamp only; token content stays immutable.
        # Deterministic one-second increments avoid low-resolution filesystem collisions.
        $heartbeatVersion++
        [IO.File]::SetLastWriteTimeUtc($lease,$leaseBase.AddSeconds($heartbeatVersion))
        $tick++
        if (-not $receiverSeen -and ($tick % 2) -eq 0 -and $observe.Elapsed.TotalSeconds -le 30) {
            $receiverSeen=ReceiverPresent
        }
        if (-not $receiverSeen -and -not $receiverWarning -and $observe.Elapsed.TotalSeconds -gt 30) {
            $receiverWarning=$true
            Note 'WARNING: Pit House listener20066 not observed in30seconds. Helper remains running; check log and physical LEDs. No ports/settings changed.'
        }
    }
} catch {
    $failed=$true
    $errorMessage=$_.Exception.Message
    Note ('ERROR: ' + $errorMessage)
    Note 'Game and Pit House are NOT killed. Do not disable security software or organizational policy.'
    if ($null -ne $lease) { try { Remove-Item -LiteralPath $lease -Force -ErrorAction SilentlyContinue } catch {} }
    if ($null -ne $markerReport -and (Test-Path -LiteralPath $markerReport)) {
        try { Get-Content -LiteralPath $markerReport -Tail 12 | ForEach-Object { Note ('MARKER_DIAG ' + $_) } } catch {}
    }
    try {
        Add-Type -AssemblyName System.Windows.Forms
        [void][System.Windows.Forms.MessageBox]::Show(
            ('Launch failed or telemetry helper stopped. The game, if started, was left running.' + [Environment]::NewLine +
             $errorMessage + [Environment]::NewLine + 'Log: ' + $report),
            'MW05 Auto Telemetry', 'OK', 'Warning')
    } catch { Write-Warning ('Details: ' + $report) }
} finally {
    # Removing our private lease asks the marker to exit. Force only our own exact child if needed.
    if ($null -ne $lease) { try { Remove-Item -LiteralPath $lease -Force -ErrorAction SilentlyContinue } catch {} }
    if ($null -ne $child) {
        try {
            $child.Refresh()
            if (-not $child.HasExited) {
                $child.WaitForExit(1500) | Out-Null; $child.Refresh()
                if (-not $child.HasExited) { $child.Kill(); $child.WaitForExit(3000) | Out-Null }
            }
            Note 'Own compatibility marker stopped.'
        } catch { Note ('Marker cleanup warning: ' + $_.Exception.Message) }
    }
    if ($null -ne $markerReport -and (Test-Path -LiteralPath $markerReport)) {
        try { Get-Content -LiteralPath $markerReport -Tail 12 | ForEach-Object { Note ('MARKER_DIAG_FINAL ' + $_) } } catch {}
    }
    if ($null -ne $work) {
        try { Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction Stop }
        catch { Note ('Temporary marker remains at: ' + $work + '; helper also expires after heartbeat loss.') }
    }
    if ($ownsMutex) { try { $mutex.ReleaseMutex() } catch {} }
    if ($null -ne $mutex) { $mutex.Dispose() }
    Note ('END failed=' + $failed)
}
