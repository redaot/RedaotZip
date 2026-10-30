@echo off
title RedaotZip - dezinstalare
if exist "%ProgramFiles%\RedaotZip\scripts\uninstall.ps1" (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%ProgramFiles%\RedaotZip\scripts\uninstall.ps1"
) else (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\uninstall.ps1"
)
