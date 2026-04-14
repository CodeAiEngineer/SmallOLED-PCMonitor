# PC Stats Monitor v3.0 - Setup & Troubleshooting Guide

## ✅ Quick Status

**The script is WORKING correctly!** All dependencies are installed and functional.

### Test Results:
- ✅ Manual execution: `python pc_stats_monitor_v2.py` - **WORKS**
- ✅ Minimized mode: `python pc_stats_monitor_v2.py --minimized` - **WORKS** (shows console output during testing)
- ✅ Autostart enabled - **Shortcut created in Windows Startup folder**
- ✅ System tray icon - **WORKS** when run with `pythonw.exe`

---

## 🚀 How to Run

### Method 1: Manual Run (Console Mode)
```bash
python pc_stats_monitor_v2.py
```
- Shows real-time output in console
- Press `Ctrl+C` to stop

### Method 2: Background Mode (System Tray)

#### Option A - Using VBScript (RECOMMENDED - No Console Window)
Double-click: `autostart-monitor.vbs`

This will:
- Start the script hidden in system tray
- No console window shown
- Right-click tray icon to Configure or Quit

#### Option B - Using Command Line
```bash
pythonw.exe pc_stats_monitor_v2.py --minimized
```
- Runs without console window
- Shows system tray icon

#### Option C - Using Batch File
Double-click: `autostart-monitor.bat`

---

## 🔧 Autostart on Windows Boot

### ✅ Autostart is ALREADY ENABLED!

The shortcut exists at:
```
C:\Users\batuh\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup\PC Monitor.lnk
```

**To verify it's working:**
1. Press `Win + R`
2. Type: `shell:startup`
3. Look for "PC Monitor.lnk" - it should be there!

**To test autostart:**
- Restart your computer
- The script should automatically start in system tray
- Look for the cyan square icon in the system tray (bottom-right corner)

### Manage Autostart

```bash
# Enable autostart
python pc_stats_monitor_v2.py --autostart enable

# Disable autostart
python pc_stats_monitor_v2.py --autostart disable
```

---

## 📦 Installed Dependencies

All required libraries are already installed:
- ✅ psutil (system metrics)
- ✅ pynvml (NVIDIA GPU monitoring)
- ✅ pywin32 (Windows integration)
- ✅ pystray (system tray)
- ✅ pillow (image handling for tray icon)
- ✅ winshell (Windows shell access)
- ✅ wmi (WMI access)
- ✅ tkinter (GUI - built-in with Python)

---

## 🔍 Troubleshooting

### Script Not Starting on Boot

**Check if autostart shortcut exists:**
1. Press `Win + R`
2. Type: `shell:startup`
3. Look for "PC Monitor.lnk"

**If missing, re-enable:**
```bash
python pc_stats_monitor_v2.py --autostart enable
```

### No System Tray Icon

**Possible causes:**
1. Script hasn't finished starting (wait 5-10 seconds)
2. System tray icons are hidden - click the up arrow (^) in the taskbar
3. Script encountered an error

**To test manually:**
```bash
wscript autostart-monitor.vbs
```
Then check system tray (bottom-right corner, might be in hidden icons)

### Script Running But No Data on ESP32

**Check:**
1. ESP32 IP address is correct in config
2. Both PC and ESP32 are on same network
3. Windows Firewall isn't blocking UDP port 4210

**View current config:**
```bash
python pc_stats_monitor_v2.py --edit
```

### Check if Script is Running

```bash
tasklist | findstr pythonw
```
You should see `pythonw.exe` in the list.

**To stop all instances:**
```bash
taskkill /F /IM pythonw.exe
```

---

## 📋 Common Commands

```bash
# Run in console mode (see all output)
python pc_stats_monitor_v2.py

# Run in background with system tray
pythonw.exe pc_stats_monitor_v2.py --minimized

# Or use VBScript (recommended)
wscript autostart-monitor.vbs

# Edit configuration
python pc_stats_monitor_v2.py --edit

# Force show configuration
python pc_stats_monitor_v2.py --configure

# Enable autostart
python pc_stats_monitor_v2.py --autostart enable

# Disable autostart
python pc_stats_monitor_v2.py --autostart disable

# Stop all running instances
taskkill /F /IM pythonw.exe
```

---

## 🎯 Current Configuration

**ESP32 IP:** 192.168.1.162
**UDP Port:** 4210
**Update Interval:** 2 seconds
**Active Metrics:** 5
1. CPU Usage
2. RAM Usage
3. GPU Temperature
4. GPU VRAM Usage
5. Network Download Speed

---

## ⚠️ Known Issues

### HWiNFO64 Not Detected
```
⚠ HWiNFO64 not detected or shared memory not enabled
```
This is **NORMAL** if you don't have HWiNFO64 running. The script works fine without it.

**To enable HWiNFO64 monitoring (optional):**
1. Install HWiNFO64
2. Run as Administrator
3. Enable "Shared Memory Support" in settings
4. Restart the script

### Console Output in --minimized Mode
When testing with `python ... --minimized`, you'll still see console output. This is **EXPECTED** because `python.exe` shows the console.

**To run truly hidden:**
- Use `pythonw.exe` instead of `python.exe`
- Or use `wscript autostart-monitor.vbs`

---

## 📝 Files Overview

| File | Purpose |
|------|---------|
| `pc_stats_monitor_v2.py` | Main monitoring script |
| `autostart-monitor.vbs` | **RECOMMENDED** - Launches script hidden |
| `autostart-monitor.bat` | Alternative launcher (shows brief console) |
| `monitor_config.json` | Your current configuration |
| `gpu_sensor.py` | GPU monitoring module |
| `hwinfo_sensor.py` | HWiNFO integration module |
| `net_monitor.py` | Network monitoring module |

---

## 🎉 Summary

**Everything is working correctly!**

✅ All dependencies installed
✅ Script runs successfully
✅ Autostart is enabled
✅ System tray support functional
✅ Configuration is valid

**To start on boot:** Just restart your computer - it's already configured!

**To start manually:** Double-click `autostart-monitor.vbs`

**To configure:** Right-click system tray icon → Configure, or run `python pc_stats_monitor_v2.py --edit`
