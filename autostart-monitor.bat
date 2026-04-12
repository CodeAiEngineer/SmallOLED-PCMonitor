@echo off
REM Autostart script for PC Stats Monitor v3.0
REM No LibreHardwareMonitor delay needed - pure Python!
powershell.exe -command "& {Start-Process -FilePath \"pythonw.exe\" -ArgumentList '\"C:\script\pc_stats_monitor.py\" --minimized' -WindowStyle hidden}"
