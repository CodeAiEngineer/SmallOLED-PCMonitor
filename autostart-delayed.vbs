' autostart-delayed.vbs - Waits 12 seconds then starts PC Monitor
Set WshShell = CreateObject("WScript.Shell")
Set FSO = CreateObject("Scripting.FileSystemObject")

' Get script directory
scriptPath = FSO.GetParentFolderName(WScript.ScriptFullName)

' Wait 12 seconds
WScript.Sleep 12000

' Start PC Monitor hidden
pythonwPath = "C:\Users\batuh\AppData\Local\Programs\Python\Python310\pythonw.exe"
scriptFile = scriptPath & "\pc_stats_monitor_v2.py"
WshShell.Run """" & pythonwPath & """ """ & scriptFile & """ --minimized", 0, False

Set WshShell = Nothing
Set FSO = Nothing
