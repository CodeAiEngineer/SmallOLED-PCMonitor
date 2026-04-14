@echo off
REM setup-autostart-task.bat
REM Creates a Windows Task Scheduler task to run PC Monitor 12 seconds after login

echo ============================================
echo   Setting up Autostart Task (12s delay)
echo ============================================
echo.

set SCRIPT_DIR=%~dp0
set VBS_PATH="%SCRIPT_DIR%autostart-delayed.vbs"

echo Creating scheduled task...
echo - Task will run 12 seconds after user login
echo - Script will run hidden in system tray
echo.

schtasks /Create /TN "PC Monitor - Delayed Start" /TR "wscript.exe %VBS_PATH%" /SC ONLOGON /RL HIGHEST /F

if %ERRORLEVEL% EQU 0 (
    echo.
    echo SUCCESS! Task created.
    echo The script will start 12 seconds after Windows login.
    echo.
    echo To verify: Task Scheduler ^> Task Scheduler Library ^> PC Monitor - Delayed Start
    echo To disable: schtasks /Delete /TN "PC Monitor - Delayed Start" /F
) else (
    echo.
    echo ERROR! Failed to create task.
    echo Run this script as Administrator.
)

echo.
pause
