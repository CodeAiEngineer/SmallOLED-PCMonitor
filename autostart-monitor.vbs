' Autostart script for PC Stats Monitor v3.0
' This VBScript runs the Python script hidden in the system tray
Set WshShell = CreateObject("WScript.Shell")
scriptPath = CreateObject("Scripting.FileSystemObject").GetParentFolderName(WScript.ScriptFullName)
pythonwPath = "C:\Users\batuh\AppData\Local\Programs\Python\Python310\pythonw.exe"
scriptFile = scriptPath & "\pc_stats_monitor_v2.py"
WshShell.Run """" & pythonwPath & """ """ & scriptFile & """ --minimized", 0, False
Set WshShell = Nothing
