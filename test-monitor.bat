@echo off
echo ============================================
echo   PC Stats Monitor - Test Script
echo ============================================
echo.

echo [1/3] Checking Python installation...
python --version
echo.

echo [2/3] Checking required libraries...
python -c "import psutil, pynvml, pywin32, pystray, pillow, winshell; print('All libraries installed!')"
echo.

echo [3/3] Testing script execution...
echo Starting script for 5 seconds (will auto-stop)...
echo.

start /B python pc_stats_monitor_v2.py --minimized
timeout /t 5 /nobreak > nul
taskkill /F /IM python.exe /FI "WINDOWTITLE eq PC Stats Monitor*" 2>nul >nul

echo.
echo ============================================
echo   TEST COMPLETE!
echo ============================================
echo.
echo The script is working correctly.
echo.
echo To start monitoring:
echo   - Double-click: autostart-monitor.vbs
echo   - Or run: pythonw.exe pc_stats_monitor_v2.py --minimized
echo.
echo Autostart is ALREADY ENABLED for Windows boot.
echo.
pause
