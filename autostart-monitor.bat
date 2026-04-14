@echo off
REM Autostart script for PC Stats Monitor v3.0
REM No LibreHardwareMonitor delay needed - pure Python!

REM Get the directory where this batch file is located
set SCRIPT_DIR=%~dp0

REM Use pythonw.exe to run without console window
start "" /B "C:\Users\batuh\AppData\Local\Programs\Python\Python310\pythonw.exe" "%SCRIPT_DIR%pc_stats_monitor_v2.py" --minimized
