@echo off
REM Creates Windows Task to start PC Monitor 12 seconds after login
REM Run this script AS ADMINISTRATOR

echo ============================================
echo   Setting up Autostart Task (12s delay)
echo ============================================
echo.
echo This script MUST be run as Administrator!
echo.

cd /d "%~dp0"

echo Creating scheduled task...
schtasks /Create /TN "PCMonitorDelayedStart" /TR "wscript.exe \"%~dp0autostart-delayed.vbs\"" /SC ONLOGON /RL HIGHEST /F

if %ERRORLEVEL% EQU 0 (
    echo.
    echo SUCCESS!
    echo Task: PCMonitorDelayedStart
    echo Trigger: User logon
    echo Delay: 12 seconds
    echo.
    echo Test it: Double-click autostart-delayed.vbs
) else (
    echo.
    echo FAILED! Right-click this file -^> "Run as administrator"
)

echo.
pause
