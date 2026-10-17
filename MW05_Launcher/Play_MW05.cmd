@echo off
rem One-click MW05 + transparent telemetry compatibility helper. No YES prompt.
start "MW05 Auto Telemetry" /min "%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0Launch_MW05.ps1"
exit /b
