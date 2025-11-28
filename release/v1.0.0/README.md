# PC Stats Monitor - Firmware Release v1.0.0

Pre-compiled firmware for ESP32-C3 boards - ready to flash!

## Quick Start (Easiest Method!)

### 🌐 Web Flasher - No Installation Required!
**Recommended for everyone - works on Windows, Mac, and Linux!**

1. Visit **[ESP Web Flasher](https://espressif.github.io/esptool-js/)**
2. Connect ESP32-C3 via USB
3. Click **"Connect"** and select your port
4. Click **"Choose File"** and select `firmware-complete.bin`
5. Set **Flash Address** to `0x0`
6. Click **"Program"** and wait ~30 seconds
7. Done! 🎉

### Alternative Methods

**Windows:**
1. Double-click `flash.bat` and follow prompts

**Linux/Mac:**
1. Run `./flash.sh` and follow prompts

**Command Line:**
```bash
esptool.py --chip esp32c3 --port COM3 --baud 460800 write_flash 0x0 firmware-complete.bin
```

### Detailed Instructions
See [FLASH_INSTRUCTIONS.md](FLASH_INSTRUCTIONS.md) for more flashing options.

## Files Included

### Firmware
- `firmware-complete.bin` - **Complete firmware (1 MB) - Easiest to flash!**

### Tools
- `flash.bat` - Automated flash script for Windows
- `flash.sh` - Automated flash script for Linux/Mac

### Documentation
- `FLASH_INSTRUCTIONS.md` - Detailed flashing guide
- `README.md` - This file

## After Flashing

1. ESP32 creates WiFi AP: **PCMonitor-Setup** (password: **monitor123**)
2. Connect and open `192.168.4.1` to configure WiFi
3. Note the IP address displayed on the OLED
4. Set up the Python monitoring script with this IP
5. Enjoy your PC stats monitor!

## Requirements

- ESP32-C3 board (tested on ESP32-C3-DevKitM-1)
- SSD1306 OLED display (128x64, I2C)
- USB cable with data support
- Chrome, Edge, or Opera browser (for web flasher)
- *OR* esptool.py (for command-line flashing)

## Support

For complete setup instructions and Python script configuration, see the main [README](../../README.md).

For issues or questions, please open an issue on GitHub.

## Version Info

- **Version**: 1.0.0
- **Release Date**: 2025-11-28
- **Chip**: ESP32-C3
- **Flash Usage**: 71.5% (936,816 bytes)
- **RAM Usage**: 12.6% (41,396 bytes)

## Changelog

**v1.0.0 (2025-11-28)**
- Customizable display labels via web interface
- Dual display modes (PC online/offline)
- Three clock styles (Mario, Standard, Large)
- Web configuration portal for all settings
- Persistent settings in flash memory
- WiFi portal for easy setup
