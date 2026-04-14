"""
PC Stats Monitor v3.0 - Pure Python Hardware Monitoring
No external monitoring app needed. Uses psutil, pynvml (GPU), and HWiNFO (CPU temp).
"""

import psutil
import socket
import time
import json
import os
import sys
import argparse
from datetime import datetime
import tkinter as tk
from tkinter import ttk, messagebox
import re

# Import native sensor modules
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gpu_sensor
import hwinfo_sensor
import net_monitor

# Try to import pystray for system tray support
try:
    import pystray
    from PIL import Image, ImageDraw
    TRAY_AVAILABLE = True
except ImportError:
    TRAY_AVAILABLE = False

# Try to import pythoncom for COM initialization (needed with pythonw.exe)
try:
    import pythoncom
    PYTHONCOM_AVAILABLE = True
except ImportError:
    PYTHONCOM_AVAILABLE = False

# Configuration file path - use absolute path to work correctly from any working directory
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CONFIG_FILE = os.path.join(SCRIPT_DIR, "monitor_config.json")

# Default configuration
DEFAULT_CONFIG = {
    "version": "3.0",
    "esp32_ip": "192.168.0.163",
    "udp_port": 4210,
    "update_interval": 3,
    "metrics": []
}

# Maximum metrics supported by ESP32
MAX_METRICS = 20

# Global sensor database
sensor_database = {
    "system": [],       # psutil-based metrics (CPU%, RAM%, Disk%)
    "temperature": [],  # CPU/GPU temperature from HWiNFO + GPU module
    "fan": [],          # Fan speeds from HWiNFO
    "load": [],         # GPU load, VRAM usage
    "clock": [],        # GPU clock
    "power": [],        # Power from HWiNFO
    "data": [],         # VRAM used in MB
    "throughput": [],   # Network upload/download speed
    "other": []
}


def discover_sensors():
    """
    Discover all available sensors from native Python sources.
    Populates the sensor_database dictionary.
    """
    print("\nDiscovering sensors...")

    # 1. psutil-based system metrics (always available)
    _add_psutil_sensors()

    # 2. GPU sensors (NVIDIA via pynvml, AMD via pyamdgpuinfo)
    _add_gpu_sensors()

    # 3. Network throughput sensors (psutil-based)
    _add_net_sensors()

    # 4. HWiNFO sensors (CPU temp, fans, voltages - optional)
    _add_hwinfo_sensors()

    # Print summary
    total = sum(len(v) for v in sensor_database.values())
    print(f"\n  Found {total} total sensors:")
    for key, sensors in sensor_database.items():
        if sensors:
            print(f"    - {key}: {len(sensors)}")


def _add_psutil_sensors():
    """Add psutil-based system sensors."""
    # CPU usage
    psutil.cpu_percent(interval=0)  # Prime it
    sensor_database["system"].append({
        "name": "CPU",
        "display_name": "CPU Usage",
        "source": "psutil",
        "type": "percent",
        "unit": "%",
        "psutil_method": "cpu_percent",
        "custom_label": "",
        "current_value": 0,
    })

    # RAM usage %
    sensor_database["system"].append({
        "name": "RAM",
        "display_name": "RAM Usage",
        "source": "psutil",
        "type": "percent",
        "unit": "%",
        "psutil_method": "virtual_memory.percent",
        "custom_label": "",
        "current_value": psutil.virtual_memory().percent,
    })

    # RAM used (GB)
    sensor_database["system"].append({
        "name": "RAM_GB",
        "display_name": "RAM Used",
        "source": "psutil",
        "type": "data",
        "unit": "GB",
        "psutil_method": "virtual_memory.used",
        "custom_label": "",
        "current_value": int(psutil.virtual_memory().used / (1024**3)),
    })

    # Disk usage %
    try:
        sensor_database["system"].append({
            "name": "DISK",
            "display_name": "Disk Usage (C:\\)",
            "source": "psutil",
            "type": "percent",
            "unit": "%",
            "psutil_method": "disk_usage",
            "custom_label": "",
            "current_value": psutil.disk_usage('C:\\').percent,
        })
    except Exception:
        pass


def _add_gpu_sensors():
    """Add GPU sensors from gpu_sensor module."""
    gpu_sensors = gpu_sensor.enumerate_gpu_sensors()
    for sensor in gpu_sensors:
        stype = sensor["type"]
        if stype in sensor_database:
            sensor_database[stype].append(sensor)
        else:
            sensor_database["other"].append(sensor)


def _add_net_sensors():
    """Add network throughput sensors from net_monitor module."""
    net_sensors = net_monitor.enumerate_net_sensors()
    for sensor in net_sensors:
        sensor_database["throughput"].append(sensor)


def _add_hwinfo_sensors():
    """Add HWiNFO sensors if available."""
    hwinfo_sensors_list = hwinfo_sensor.enumerate_hwinfo_sensors()
    for sensor in hwinfo_sensors_list:
        stype = sensor["type"]
        if stype in sensor_database:
            sensor_database[stype].append(sensor)
        else:
            sensor_database["other"].append(sensor)


def _warmup_sensors():
    """
    Pre-warm all sensor sources to avoid first-call failures.
    Initializes network throughput monitor and verifies GPU access.
    """
    # Initialize network throughput monitor and wait for first delta
    net_monitor.initialize_net_monitor()
    time.sleep(1.0)  # Wait for meaningful throughput delta

    # Verify GPU access
    gpu_sensor.get_gpu_count()

    # Verify we can actually read all configured metrics
    psutil.cpu_percent(interval=0.5)


def _sensor_key(sensor):
    """Generate a unique key for any sensor regardless of source."""
    # Backward compat: old configs may have wmi_identifier
    if sensor.get("wmi_identifier"):
        return sensor["wmi_identifier"]
    # New sources
    if sensor.get("gpu_method"):
        return f"gpu_{sensor['gpu_method']}_{sensor.get('gpu_index', 0)}"
    if sensor.get("hwinfo_sensor_name"):
        return f"hwinfo_{sensor['hwinfo_sensor_name']}"
    if sensor.get("net_method"):
        return f"net_{sensor['net_method']}_{sensor.get('net_interface', '')}"
    if sensor.get("psutil_method"):
        return f"psutil_{sensor['psutil_method']}"
    return f"{sensor['source']}_{sensor['display_name']}"


def _migrate_old_config(config):
    """
    Migrate config from v2.x (LHM/WMI-based) to v3.x (native Python-based).
    Converts wmi-source metrics to nvidia/net/psutil sources.
    """
    for metric in config.get("metrics", []):
        if metric.get("source") != "wmi":
            continue

        wmi_id = metric.get("wmi_identifier", "").lower()
        wmi_name = metric.get("wmi_sensor_name", "").lower()
        sensor_type = metric.get("type", "").lower()

        # GPU temperature
        if "gpu-nvidia" in wmi_id and sensor_type == "temperature":
            metric["source"] = "nvidia"
            metric["gpu_method"] = "temp"
            metric["gpu_index"] = 0
            metric.pop("wmi_identifier", None)
            metric.pop("wmi_sensor_name", None)
            metric.pop("is_active_nic", None)
            metric.pop("parent_hardware", None)

        # GPU VRAM / load
        elif "gpu-nvidia" in wmi_id and sensor_type == "load":
            if "memory" in wmi_name or "vram" in wmi_name or "frame" in wmi_name:
                metric["source"] = "nvidia"
                metric["gpu_method"] = "vram_percent"
                metric["gpu_index"] = 0
            else:
                metric["source"] = "nvidia"
                metric["gpu_method"] = "load_percent"
                metric["gpu_index"] = 0
            metric.pop("wmi_identifier", None)
            metric.pop("wmi_sensor_name", None)
            metric.pop("is_active_nic", None)
            metric.pop("parent_hardware", None)

        # Network throughput (download/upload)
        elif "nic" in wmi_id and sensor_type == "throughput":
            metric["source"] = "net"
            metric["unit"] = "KB/s"
            if "download" in wmi_name or "receive" in wmi_name:
                metric["net_method"] = "download"
            else:
                metric["net_method"] = "upload"
            # Auto-detect active network interface
            metric["net_interface"] = "_auto_"
            metric.pop("wmi_identifier", None)
            metric.pop("wmi_sensor_name", None)
            metric["is_active_nic"] = metric.get("is_active_nic", False)

        # GPU clock
        elif "gpu-nvidia" in wmi_id and sensor_type == "clock":
            metric["source"] = "nvidia"
            metric["gpu_method"] = "clock_mhz"
            metric["gpu_index"] = 0
            metric.pop("wmi_identifier", None)
            metric.pop("wmi_sensor_name", None)

        # GPU power
        elif "gpu-nvidia" in wmi_id and sensor_type == "power":
            metric["source"] = "nvidia"
            metric["gpu_method"] = "power_watts"
            metric["gpu_index"] = 0
            metric.pop("wmi_identifier", None)
            metric.pop("wmi_sensor_name", None)

        # CPU temp (LHM WMI) - mark as unavailable
        elif "intelcpu" in wmi_id or "amdcpu" in wmi_id:
            if sensor_type == "temperature":
                metric["source"] = "hwinfo"
                metric["hwinfo_sensor_name"] = "CPU Package"
                metric.pop("wmi_identifier", None)
                metric.pop("wmi_sensor_name", None)

    return config


def load_config():
    """
    Load configuration from file with version checking and auto-migration.
    """
    if not os.path.exists(CONFIG_FILE):
        return None

    try:
        with open(CONFIG_FILE, 'r') as f:
            config = json.load(f)

        config_version = config.get("version", "1.0")

        if config_version < "3.0":
            # Auto-migrate old config instead of forcing reconfiguration
            print(f"\n  Migrating config from v{config_version} → v3.0...")

            # Backup old config
            backup_path = CONFIG_FILE.replace(".json", f"_v{config_version}_backup.json")
            try:
                import shutil
                shutil.copy(CONFIG_FILE, backup_path)
                print(f"  Old config backed up to: {backup_path}")
            except Exception as e:
                print(f"  Warning: Could not backup config: {e}")

            # Migrate wmi-source metrics to new sources
            config = _migrate_old_config(config)
            config["version"] = "3.0"
            save_config(config)
            print("  Config migrated successfully!")

        print(f"\n[OK] Loaded configuration from {CONFIG_FILE}")
        print(f"  Selected metrics: {len(config.get('metrics', []))}")
        return config
    except Exception as e:
        print(f"\n[ERR] Error loading config: {e}")
        return None


def save_config(config):
    """
    Save configuration to file
    """
    try:
        with open(CONFIG_FILE, 'w') as f:
            json.dump(config, f, indent=2)
        print(f"\n[OK] Configuration saved to {CONFIG_FILE}")
        return True
    except Exception as e:
        print(f"\n[ERR] Error saving config: {e}")
        return False


def setup_autostart(enable=True):
    """
    Add/remove script to Windows startup folder
    """
    import winshell
    from win32com.client import Dispatch

    startup_folder = winshell.startup()
    shortcut_path = os.path.join(startup_folder, "PC Monitor.lnk")

    if enable:
        # Create shortcut
        shell = Dispatch('WScript.Shell')
        shortcut = shell.CreateShortCut(shortcut_path)

        # Use pythonw.exe to run without console window
        python_exe = sys.executable.replace("python.exe", "pythonw.exe")
        script_path = os.path.abspath(__file__)

        shortcut.TargetPath = python_exe
        shortcut.Arguments = f'"{script_path}" --minimized'
        shortcut.WorkingDirectory = os.path.dirname(script_path)
        shortcut.IconLocation = python_exe
        shortcut.save()

        print(f"\n[OK] Autostart enabled!")
        print(f"  Shortcut created: {shortcut_path}")
        return True
    else:
        # Remove shortcut
        if os.path.exists(shortcut_path):
            os.remove(shortcut_path)
            print(f"\n[OK] Autostart disabled!")
            print(f"  Shortcut removed: {shortcut_path}")
            return True
        else:
            print("\n[WARN] Autostart shortcut not found")
            return False


# AutoConfigPreviewDialog class removed - will be revisited later


class MetricSelectorGUI:
    """
    Tkinter GUI for selecting metrics and configuring settings
    """
    def __init__(self, root, existing_config=None):
        self.root = root
        self.root.title("PC Monitor v3.0 - Configuration")
        self.root.geometry("1200x800")
        self.root.resizable(False, False)

        self.selected_metrics = []
        self.checkboxes = []
        self.label_entries = {}

        # Load existing config if available
        if existing_config:
            self.config = existing_config
        else:
            self.config = DEFAULT_CONFIG.copy()

        self.create_widgets()

        # Load existing selections if editing
        if existing_config and existing_config.get("metrics"):
            self.load_existing_metrics(existing_config["metrics"])

    def create_widgets(self):
        # Title
        title_frame = tk.Frame(self.root, bg="#1e1e1e", height=45)
        title_frame.pack(fill=tk.X)
        title_frame.pack_propagate(False)

        title_label = tk.Label(
            title_frame,
            text="PC Monitor Configuration",
            font=("Arial", 18, "bold"),
            bg="#1e1e1e",
            fg="#00d4ff"
        )
        title_label.pack(pady=8)

        # Settings frame (ESP IP, Port, Interval)
        settings_frame = tk.Frame(self.root, bg="#2d2d2d")
        settings_frame.pack(fill=tk.X, padx=10, pady=5)

        # ESP IP
        tk.Label(settings_frame, text="ESP32 IP:", bg="#2d2d2d", fg="#ffffff", font=("Arial", 10)).grid(row=0, column=0, padx=10, pady=3, sticky="e")
        self.ip_var = tk.StringVar(value=self.config.get("esp32_ip", "192.168.0.163"))
        tk.Entry(settings_frame, textvariable=self.ip_var, width=20).grid(row=0, column=1, padx=5, pady=3, sticky="w")

        # UDP Port
        tk.Label(settings_frame, text="UDP Port:", bg="#2d2d2d", fg="#ffffff", font=("Arial", 10)).grid(row=0, column=2, padx=10, pady=3, sticky="e")
        self.port_var = tk.StringVar(value=str(self.config.get("udp_port", 4210)))
        tk.Entry(settings_frame, textvariable=self.port_var, width=10).grid(row=0, column=3, padx=5, pady=3, sticky="w")

        # Update Interval
        tk.Label(settings_frame, text="Update Interval (seconds):", bg="#2d2d2d", fg="#ffffff", font=("Arial", 10)).grid(row=0, column=4, padx=10, pady=3, sticky="e")
        self.interval_var = tk.StringVar(value=str(self.config.get("update_interval", 3)))
        tk.Entry(settings_frame, textvariable=self.interval_var, width=10).grid(row=0, column=5, padx=5, pady=3, sticky="w")

        # Autostart section (second row)
        tk.Label(settings_frame, text="Windows Autostart:", bg="#2d2d2d", fg="#ffffff", font=("Arial", 10)).grid(row=1, column=0, padx=10, pady=3, sticky="e")

        autostart_frame = tk.Frame(settings_frame, bg="#2d2d2d")
        autostart_frame.grid(row=1, column=1, columnspan=2, padx=5, pady=3, sticky="w")

        self.autostart_status = tk.Label(
            autostart_frame,
            text=self.get_autostart_status_text(),
            bg="#2d2d2d",
            fg=self.get_autostart_status_color(),
            font=("Arial", 10, "bold")
        )
        self.autostart_status.pack(side=tk.LEFT, padx=5)

        enable_btn = tk.Button(
            autostart_frame,
            text="Enable",
            command=self.enable_autostart,
            bg="#00d4ff",
            fg="#000000",
            font=("Arial", 9),
            relief=tk.FLAT,
            padx=10,
            pady=2
        )
        enable_btn.pack(side=tk.LEFT, padx=5)

        disable_btn = tk.Button(
            autostart_frame,
            text="Disable",
            command=self.disable_autostart,
            bg="#ff6666",
            fg="#000000",
            font=("Arial", 9),
            relief=tk.FLAT,
            padx=10,
            pady=2
        )
        disable_btn.pack(side=tk.LEFT, padx=5)

        # Counter frame
        counter_frame = tk.Frame(self.root, bg="#2d2d2d", height=50)
        counter_frame.pack(fill=tk.X)
        counter_frame.pack_propagate(False)

        self.counter_label = tk.Label(
            counter_frame,
            text=f"Selected: 0/{MAX_METRICS}",
            font=("Arial", 12),
            bg="#2d2d2d",
            fg="#ffffff"
        )
        self.counter_label.pack(side=tk.LEFT, padx=20, pady=10)

        # Search box
        search_label = tk.Label(counter_frame, text="Search:", bg="#2d2d2d", fg="#ffffff")
        search_label.pack(side=tk.LEFT, padx=(50, 5))

        self.search_var = tk.StringVar()
        self.search_var.trace_add('write', lambda *_: self.on_search())
        search_entry = tk.Entry(counter_frame, textvariable=self.search_var, width=20)
        search_entry.pack(side=tk.LEFT, padx=5)

        # Info note about static values
        info_label = tk.Label(
            counter_frame,
            text="ℹ Values are static from GUI launch",
            bg="#2d2d2d",
            fg="#888888",
            font=("Arial", 9, "italic")
        )
        info_label.pack(side=tk.LEFT, padx=(20, 0))

        # Buttons
        clear_btn = tk.Button(
            counter_frame,
            text="Clear All",
            command=self.clear_all,
            bg="#444444",
            fg="#ffffff",
            relief=tk.FLAT,
            padx=10
        )
        clear_btn.pack(side=tk.RIGHT, padx=20)

        # Main scrollable frame
        main_frame = tk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        canvas = tk.Canvas(main_frame, bg="#ffffff")
        scrollbar = ttk.Scrollbar(main_frame, orient="vertical", command=canvas.yview)
        scrollable_frame = tk.Frame(canvas, bg="#ffffff")

        scrollable_frame.bind(
            "<Configure>",
            lambda e: canvas.configure(scrollregion=canvas.bbox("all"))
        )

        # Create window that fills canvas width
        canvas_window = canvas.create_window((0, 0), window=scrollable_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)

        # Make scrollable_frame fill canvas width
        def on_canvas_configure(event):
            canvas.itemconfig(canvas_window, width=event.width)
        canvas.bind("<Configure>", on_canvas_configure)

        # Mouse wheel scrolling
        def on_mousewheel(event):
            canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

        # Bind mousewheel to canvas and all children
        def bind_mousewheel(widget):
            widget.bind("<MouseWheel>", on_mousewheel)
            for child in widget.winfo_children():
                bind_mousewheel(child)

        canvas.bind("<MouseWheel>", on_mousewheel)
        scrollable_frame.bind("<MouseWheel>", on_mousewheel)

        # Store reference to bind mousewheel to new widgets later
        self.canvas = canvas
        self.on_mousewheel = on_mousewheel

        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        # Create checkboxes by category
        categories = [
            ("SYSTEM METRICS", "system"),
            ("TEMPERATURES", "temperature"),
            ("FANS & COOLING", "fan"),
            ("LOADS", "load"),
            ("CLOCKS", "clock"),
            ("POWER", "power"),
            ("NETWORK DATA", "data"),
            ("NETWORK THROUGHPUT", "throughput")
        ]

        # Pre-calculate visible categories to optimize layout
        visible_categories = [(title, key) for title, key in categories if sensor_database.get(key)]
        num_cols = min(3, len(visible_categories))  # Use fewer columns if fewer categories

        # Create column container frames
        column_frames = []
        for col in range(num_cols):
            scrollable_frame.columnconfigure(col, weight=1, uniform="columns")
            col_frame = tk.Frame(scrollable_frame, bg="#ffffff")
            col_frame.grid(row=0, column=col, sticky="nsew", padx=0, pady=0)
            col_frame.bind("<MouseWheel>", on_mousewheel)
            column_frames.append(col_frame)

        # Distribute categories into columns (vertical packing within each column)
        for idx, (cat_title, cat_key) in enumerate(visible_categories):
            col = idx % num_cols
            parent_frame = column_frames[col]

            # Category header
            cat_frame = tk.Frame(parent_frame, bg="#f0f0f0", relief=tk.RIDGE, borderwidth=2)
            cat_frame.pack(fill=tk.X, padx=5, pady=5, anchor="n")

            cat_label = tk.Label(
                cat_frame,
                text=cat_title,
                font=("Arial", 11, "bold"),
                bg="#f0f0f0",
                fg="#333333"
            )
            cat_label.pack(pady=5)

            # Bind mousewheel to category frame and label
            cat_frame.bind("<MouseWheel>", on_mousewheel)
            cat_label.bind("<MouseWheel>", on_mousewheel)

            # Sensors in category
            for sensor in sensor_database[cat_key]:
                var = tk.BooleanVar()

                # Highlight active network interfaces
                is_active = sensor.get('is_active_nic', False)
                if is_active:
                    frame_bg = "#d4ffd4"  # Light green background
                    text_color = "#006600"  # Dark green text
                    active_marker = " ★"  # Star to mark active
                else:
                    frame_bg = "#f0f0f0"
                    text_color = "#000000"
                    active_marker = ""

                # Create sensor row frame
                sensor_frame = tk.Frame(cat_frame, bg=frame_bg)
                sensor_frame.pack(fill=tk.X, padx=10, pady=2)

                # Checkbox with current value
                value_text = f" - {sensor['current_value']}{sensor['unit']}" if sensor.get('current_value') is not None else ""
                cb = tk.Checkbutton(
                    sensor_frame,
                    text=f"{sensor['display_name']} ({sensor['name']}){value_text}{active_marker}",
                    variable=var,
                    bg=frame_bg,
                    fg=text_color,
                    selectcolor="#ffffff",
                    anchor="w",
                    command=lambda s=sensor, v=var: self.on_checkbox_toggle(s, v)
                )
                cb.pack(side=tk.TOP, fill=tk.X)

                # Custom label entry (small, below checkbox)
                label_frame = tk.Frame(sensor_frame, bg=frame_bg)
                label_frame.pack(side=tk.TOP, fill=tk.X, padx=20)

                tk.Label(label_frame, text="Label:", bg=frame_bg, fg="#666", font=("Arial", 8)).pack(side=tk.LEFT)
                label_entry = tk.Entry(label_frame, width=15, font=("Arial", 8))
                label_entry.pack(side=tk.LEFT, padx=5)

                # Update preview when label text changes
                label_entry.bind("<KeyRelease>", lambda e: self.update_counter())

                # Store reference to label entry and label frame
                sensor_key = _sensor_key(sensor)
                self.label_entries[sensor_key] = {
                    'entry': label_entry,
                    'frame': label_frame
                }

                # Bind mousewheel to all created widgets
                for widget in [sensor_frame, cb, label_frame, label_entry]:
                    widget.bind("<MouseWheel>", on_mousewheel)

                self.checkboxes.append((cb, sensor, var, sensor_frame))

        # Preview frame
        preview_frame = tk.Frame(self.root, bg="#2d2d2d", height=40)
        preview_frame.pack(fill=tk.X)
        preview_frame.pack_propagate(False)

        preview_label = tk.Label(
            preview_frame,
            text="SELECTED PREVIEW (names sent to ESP32):",
            font=("Arial", 9, "bold"),
            bg="#2d2d2d",
            fg="#888888"
        )
        preview_label.pack(anchor="w", padx=10, pady=(5, 0))

        self.preview_text = tk.Label(
            preview_frame,
            text="",
            font=("Courier", 9),
            bg="#2d2d2d",
            fg="#00ff00",
            anchor="w",
            justify=tk.LEFT
        )
        self.preview_text.pack(fill=tk.X, padx=10, pady=(0, 5))

        # Bottom buttons
        button_frame = tk.Frame(self.root, bg="#1e1e1e", height=50)
        button_frame.pack(fill=tk.X)
        button_frame.pack_propagate(False)

        cancel_btn = tk.Button(
            button_frame,
            text="Cancel",
            command=self.root.quit,
            bg="#666666",
            fg="#ffffff",
            font=("Arial", 12),
            relief=tk.FLAT,
            padx=20,
            pady=5
        )
        cancel_btn.pack(side=tk.LEFT, padx=20, pady=8)

        save_btn = tk.Button(
            button_frame,
            text="Save & Start Monitoring",
            command=self.save_and_start,
            bg="#00d4ff",
            fg="#000000",
            font=("Arial", 12, "bold"),
            relief=tk.FLAT,
            padx=20,
            pady=5
        )
        save_btn.pack(side=tk.RIGHT, padx=20, pady=8)

        # Update counter
        self.update_counter()

    def load_existing_metrics(self, metrics):
        """Load existing metric selections when editing config"""
        for metric in metrics:
            for cb, sensor, var, frame in self.checkboxes:
                sensor_k = _sensor_key(sensor)
                metric_k = _sensor_key(metric)
                if sensor_k == metric_k:
                    if sensor not in self.selected_metrics:
                        self.selected_metrics.append(sensor)
                    var.set(True)
                    if metric.get('custom_label') and sensor_k in self.label_entries:
                        self.label_entries[sensor_k]['entry'].insert(0, metric['custom_label'])
                    break
        self.root.after(100, self.update_counter)

    def on_checkbox_toggle(self, sensor, var):
        if var.get():
            if len(self.selected_metrics) >= MAX_METRICS:
                messagebox.showwarning(
                    "Limit Reached",
                    f"Maximum {MAX_METRICS} metrics allowed!\nDeselect some metrics first."
                )
                var.set(False)
                return
            # Check for duplicates before appending
            if sensor not in self.selected_metrics:
                self.selected_metrics.append(sensor)
        else:
            if sensor in self.selected_metrics:
                self.selected_metrics.remove(sensor)

        self.update_counter()

    def get_display_label_for_metric(self, sensor):
        """Get custom label if set, otherwise return sensor name"""
        sensor_key = _sensor_key(sensor)
        if sensor_key in self.label_entries:
            custom = self.label_entries[sensor_key]['entry'].get().strip()
            if custom:
                return custom[:10]
        return sensor['name']

    def update_counter(self):
        count = len(self.selected_metrics)
        self.counter_label.config(text=f"Selected: {count}/{MAX_METRICS}")

        if count >= MAX_METRICS:
            self.counter_label.config(fg="#ff6666")
        else:
            self.counter_label.config(fg="#ffffff")

        # Update preview - now shows custom labels
        preview = " | ".join([f"{i+1}. {self.get_display_label_for_metric(m)}" for i, m in enumerate(self.selected_metrics[:MAX_METRICS])])
        self.preview_text.config(text=preview if preview else "(none selected)")

    def clear_all(self):
        for cb, sensor, var, frame in self.checkboxes:
            var.set(False)
        self.selected_metrics.clear()
        self.update_counter()

    def on_search(self, *args):
        search_term = self.search_var.get().lower()
        for cb, sensor, var, frame in self.checkboxes:
            if search_term in sensor['display_name'].lower() or search_term in sensor['name'].lower():
                cb.config(bg="#ffffcc")
                frame.config(bg="#ffffcc")
            else:
                cb.config(bg="#f0f0f0")
                frame.config(bg="#f0f0f0")

    def get_autostart_status_text(self):
        """Check if autostart is enabled"""
        try:
            import winshell
            startup_folder = winshell.startup()
            shortcut_path = os.path.join(startup_folder, "PC Monitor.lnk")
            if os.path.exists(shortcut_path):
                return "[OK] Enabled"
            else:
                return "[X] Disabled"
        except Exception:
            return "? Unknown"

    def get_autostart_status_color(self):
        """Get color for autostart status"""
        status = self.get_autostart_status_text()
        if "Enabled" in status:
            return "#00ff00"
        elif "Disabled" in status:
            return "#ff6666"
        else:
            return "#888888"

    def update_autostart_status(self):
        """Update the autostart status label"""
        self.autostart_status.config(
            text=self.get_autostart_status_text(),
            fg=self.get_autostart_status_color()
        )

    def enable_autostart(self):
        """Enable autostart"""
        try:
            success = setup_autostart(enable=True)
            if success:
                self.update_autostart_status()
                messagebox.showinfo("Success", "Autostart enabled!\n\nThe script will run minimized to system tray on Windows startup.\nRight-click the tray icon to configure or quit.")
            else:
                messagebox.showerror("Error", "Failed to enable autostart")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to enable autostart:\n{str(e)}\n\nMake sure pywin32 is installed:\npip install pywin32")

    def disable_autostart(self):
        """Disable autostart"""
        try:
            success = setup_autostart(enable=False)
            if success:
                self.update_autostart_status()
                messagebox.showinfo("Success", "Autostart disabled!")
            else:
                messagebox.showwarning("Warning", "Autostart shortcut not found")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to disable autostart:\n{str(e)}")

    def save_and_start(self):
        if len(self.selected_metrics) == 0:
            messagebox.showwarning("No Selection", "Please select at least one metric!")
            return

        # Validate settings
        try:
            esp_ip = self.ip_var.get().strip()
            udp_port = int(self.port_var.get())
            update_interval = float(self.interval_var.get())

            if not esp_ip:
                raise ValueError("ESP32 IP cannot be empty")
            if udp_port < 1 or udp_port > 65535:
                raise ValueError("Invalid port number")
            if update_interval < 0.5:
                raise ValueError("Update interval must be at least 0.5 seconds")
        except ValueError as e:
            messagebox.showerror("Invalid Settings", str(e))
            return

        # Build config
        config = {
            "version": "3.0",
            "esp32_ip": esp_ip,
            "udp_port": udp_port,
            "update_interval": update_interval,
            "metrics": []
        }

        # Assign IDs and add custom labels
        for i, sensor in enumerate(self.selected_metrics):
            metric_config = sensor.copy()
            metric_config["id"] = i + 1

            # Get custom label if set
            sensor_key = _sensor_key(sensor)
            if sensor_key in self.label_entries:
                custom_label = self.label_entries[sensor_key]['entry'].get().strip()
                if custom_label:
                    metric_config["custom_label"] = custom_label[:10]

            config["metrics"].append(metric_config)

        if save_config(config):
            messagebox.showinfo("Success", f"Configuration saved!\n{len(self.selected_metrics)} metrics will be monitored.")
            self.root.quit()
        else:
            messagebox.showerror("Error", "Failed to save configuration!")


def get_metric_value(metric_config):
    """
    Get current value for a configured metric from native Python sources.

    Returns: int value on success, None on failure.
    """
    source = metric_config["source"]

    if source == "psutil":
        method = metric_config["psutil_method"]
        if method == "cpu_percent":
            return int(psutil.cpu_percent(interval=0))
        elif method == "virtual_memory.percent":
            return int(psutil.virtual_memory().percent)
        elif method == "virtual_memory.used":
            return int(psutil.virtual_memory().used / (1024**3))
        elif method == "disk_usage":
            return int(psutil.disk_usage('C:\\').percent)

    elif source in ("nvidia", "amd"):
        # GPU sensors via pynvml / pyamdgpuinfo
        method = metric_config.get("gpu_method")
        gpu_idx = metric_config.get("gpu_index", 0)
        if method == "temp":
            val = gpu_sensor.get_gpu_temp(gpu_idx)
        elif method == "vram_percent":
            val = gpu_sensor.get_gpu_vram_percent(gpu_idx)
        elif method == "vram_used_mb":
            val = gpu_sensor.get_gpu_vram_used_mb(gpu_idx)
        elif method == "clock_mhz":
            val = gpu_sensor.get_gpu_clock_mhz(gpu_idx)
        elif method == "load_percent":
            val = gpu_sensor.get_gpu_load_percent(gpu_idx)
        else:
            return None
        return int(val) if val is not None else None

    elif source == "hwinfo":
        # HWiNFO shared memory reader
        sensor_name = metric_config.get("hwinfo_sensor_name", "")
        if not sensor_name:
            return None
        val, _ = hwinfo_sensor.get_sensor_value(sensor_name)
        return int(val) if val is not None else None

    elif source == "net":
        # Network throughput via psutil deltas
        method = metric_config.get("net_method")
        iface = metric_config.get("net_interface")

        # Auto-detect active interface if not specified
        if not iface or iface == "_auto_":
            real_ifaces = net_monitor.net_monitor.get_active_interfaces()
            iface = real_ifaces[0] if real_ifaces else None

        up_kbs, dn_kbs = net_monitor.get_net_throughput_kb_s(iface)
        if method == "upload":
            return int(up_kbs)
        elif method == "download":
            return int(dn_kbs)
        return None

    return None


# Status codes (must match ESP32 config.h)
STATUS_OK = 1
STATUS_SENSOR_ERROR = 2
STATUS_HWINFO_NOT_RUNNING = 3
STATUS_STARTING = 4
STATUS_UNKNOWN_ERROR = 5


def send_metrics(sock, config, last_good_values=None, status_code=STATUS_OK):
    """
    Collect metric values and send to ESP32

    Args:
        sock: UDP socket
        config: Configuration dictionary
        last_good_values: Dict to track last known good values per metric ID
        status_code: Sensor status code (1=OK, 2=Sensor error, 3=HWiNFO not running, etc.)

    Returns:
        Tuple of (success: bool, last_good_values: dict, has_fresh_data: bool)
    """
    if last_good_values is None:
        last_good_values = {}

    has_fresh_data = False
    stale_count = 0

    payload = {
        "version": "3.0",
        "status": status_code,
        "timestamp": "",
        "metrics": []
    }

    for metric_config in config["metrics"]:
        value = get_metric_value(metric_config)
        metric_id = metric_config["id"]

        if value is not None:
            last_good_values[metric_id] = value
            has_fresh_data = True
        else:
            value = last_good_values.get(metric_id, 0)
            stale_count += 1

        display_name = metric_config.get("custom_label", "")
        if not display_name:
            display_name = metric_config["name"]

        metric_data = {
            "id": metric_id,
            "name": display_name,
            "value": value,
            "unit": metric_config["unit"]
        }
        payload["metrics"].append(metric_data)

    total_metrics = len(config["metrics"])
    if total_metrics > 0 and stale_count >= total_metrics:
        if status_code == STATUS_OK:
            status_code = STATUS_SENSOR_ERROR
        payload["status"] = status_code
    elif stale_count > 0 and stale_count >= total_metrics * 0.5:
        if status_code == STATUS_OK:
            status_code = STATUS_SENSOR_ERROR
        payload["status"] = status_code

    if has_fresh_data:
        payload["timestamp"] = datetime.now().strftime('%H:%M')
    else:
        payload["timestamp"] = ""

    # Send via UDP
    try:
        message = json.dumps(payload).encode('utf-8')
        sock.sendto(message, (config["esp32_ip"], config["udp_port"]))

        # Print status with stale indicator and status code
        timestamp = payload["timestamp"] if payload["timestamp"] else "STALE"
        metrics_str = " | ".join([f"{m['name']}:{m['value']}{m['unit']}" for m in payload["metrics"][:4]])
        if len(payload["metrics"]) > 4:
            metrics_str += f" ... +{len(payload['metrics'])-4} more"

        stale_indicator = f" [!{stale_count} stale]" if stale_count > 0 else ""

        # Status code indicator
        status_names = {
            STATUS_OK: "",
            STATUS_SENSOR_ERROR: " [SENSOR ERR]",
            STATUS_HWINFO_NOT_RUNNING: " [HWiNFO DOWN]",
            STATUS_STARTING: " [STARTING]",
            STATUS_UNKNOWN_ERROR: " [ERROR]"
        }
        status_indicator = status_names.get(status_code, f" [STATUS:{status_code}]")

        print(f"[{timestamp}] {metrics_str}{stale_indicator}{status_indicator}")

        return True, last_good_values, has_fresh_data
    except Exception as e:
        print(f"Error sending data: {e}")
        return False, last_good_values, has_fresh_data


def create_tray_icon():
    """Create a simple system tray icon"""
    if not TRAY_AVAILABLE:
        return None

    # Create a simple icon
    def create_image():
        width = 64
        height = 64
        image = Image.new('RGB', (width, height), color='black')
        dc = ImageDraw.Draw(image)
        dc.rectangle([16, 16, 48, 48], fill='cyan')
        return image

    return create_image()


def run_minimized(config):
    """Run monitoring loop in background with system tray icon"""

    if not TRAY_AVAILABLE:
        print("\nWARNING: pystray not available, running in console mode")
        print("Install with: pip install pystray pillow")
        run_monitoring(config)
        return

    import threading
    stop_event = threading.Event()

    def monitoring_thread():
        if PYTHONCOM_AVAILABLE:
            try:
                pythoncom.CoInitialize()
            except Exception:
                pass

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        # Warm up all sensor sources before sending first packet
        psutil.cpu_percent(interval=1)
        _warmup_sensors()

        last_good_values = {}
        last_check = time.time()
        warmup_done = False

        # Send a primer packet first - verify all sensors work
        print("Verifying sensor readings...")
        primer_values = {}
        all_valid = True
        for mc in config["metrics"]:
            v = get_metric_value(mc)
            if v is None:
                print(f"  [WARN] {mc['name']} ({mc['source']}) returned None")
                all_valid = False
            else:
                primer_values[mc["id"]] = v
                print(f"  [OK] {mc['name']} = {v} {mc['unit']}")

        if all_valid:
            # Send primer packet to ESP32 with confirmed-good values
            send_metrics(sock, config, primer_values, STATUS_OK)
            time.sleep(config["update_interval"])
            warmup_done = True
            print("All sensors verified. Starting main loop...")
        else:
            print("  [WARN] Some sensors not ready, starting anyway...")

        while not stop_event.is_set():
            current_time = time.time()
            current_status = STATUS_OK

            if current_time - last_check >= 30:
                last_check = current_time
                has_hwinfo = any(m.get("source") == "hwinfo" for m in config["metrics"])
                if has_hwinfo and not hwinfo_sensor.is_hwinfo_running():
                    current_status = STATUS_HWINFO_NOT_RUNNING

            success, last_good_values, has_fresh = send_metrics(sock, config, last_good_values, current_status)
            if has_fresh and not warmup_done:
                warmup_done = True
                # First successful packet sent - wait a bit so ESP32 has time to process
                time.sleep(config["update_interval"])
                continue

            time.sleep(config["update_interval"])

        sock.close()

        # Uninitialize COM when thread exits
        if PYTHONCOM_AVAILABLE:
            try:
                pythoncom.CoUninitialize()
            except Exception:
                pass

    # Create tray icon
    def on_quit(icon, item):
        stop_event.set()
        icon.stop()

    def on_show_config(icon, item):
        stop_event.set()
        icon.stop()
        os.system(f'"{sys.executable}" "{os.path.abspath(__file__)}" --edit')

    icon = pystray.Icon(
        "pc_monitor",
        create_tray_icon(),
        "PC Monitor v3.0 - Running",
        menu=pystray.Menu(
            pystray.MenuItem("Configure", on_show_config),
            pystray.MenuItem("Quit", on_quit)
        )
    )

    # Start monitoring thread
    thread = threading.Thread(target=monitoring_thread, daemon=True)
    thread.start()

    # Run tray icon (blocking) - this hides the window when run with pythonw.exe
    try:
        icon.run()
    except KeyboardInterrupt:
        stop_event.set()
        icon.stop()


def run_monitoring(config):
    """Run monitoring loop in console mode"""
    print(f"\nMonitoring {len(config['metrics'])} metrics:")
    for m in config["metrics"]:
        label_info = f" (Label: {m['custom_label']})" if m.get('custom_label') else ""
        print(f"  {m['id']}. {m['display_name']} ({m['name']}){label_info} - {m['source']}")

    print(f"\nESP32 IP: {config['esp32_ip']}")
    print(f"UDP Port: {config['udp_port']}")
    print(f"Update Interval: {config['update_interval']}s")
    print("\nWarming up sensors...")
    _warmup_sensors()
    print("Starting monitoring... (Press Ctrl+C to stop)\n")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    psutil.cpu_percent(interval=1)

    last_good_values = {}
    last_check = time.time()

    # Verify sensors before main loop
    print("Verifying sensor readings...")
    primer_values = {}
    all_valid = True
    for mc in config["metrics"]:
        v = get_metric_value(mc)
        if v is None:
            print(f"  [WARN] {mc['name']} ({mc['source']}) returned None")
            all_valid = False
        else:
            primer_values[mc["id"]] = v
            print(f"  [OK] {mc['name']} = {v} {mc['unit']}")

    if all_valid:
        send_metrics(sock, config, primer_values, STATUS_OK)
        time.sleep(config["update_interval"])
        print("All sensors verified. Starting main loop...")

    try:
        while True:
            current_time = time.time()
            current_status = STATUS_OK

            if current_time - last_check >= 30:
                last_check = current_time
                has_hwinfo = any(m.get("source") == "hwinfo" for m in config["metrics"])
                if has_hwinfo and not hwinfo_sensor.is_hwinfo_running():
                    current_status = STATUS_HWINFO_NOT_RUNNING

            success, last_good_values, has_fresh = send_metrics(sock, config, last_good_values, current_status)
            time.sleep(config["update_interval"])

    except KeyboardInterrupt:
        print("\n\nMonitoring stopped.")
    finally:
        sock.close()


def main():
    """
    Main entry point
    """
    parser = argparse.ArgumentParser(description='PC Stats Monitor v3.0 - Pure Python Hardware Monitoring')
    parser.add_argument('--configure', action='store_true', help='Force configuration GUI')
    parser.add_argument('--edit', action='store_true', help='Edit existing configuration')
    parser.add_argument('--autostart', choices=['enable', 'disable'], help='Enable/disable autostart')
    parser.add_argument('--minimized', action='store_true', help='Run minimized to system tray')
    args = parser.parse_args()

    if PYTHONCOM_AVAILABLE:
        try:
            pythoncom.CoInitialize()
        except Exception:
            pass

    if args.autostart:
        try:
            success = setup_autostart(args.autostart == 'enable')
            if success:
                print("\nTIP: The script will run minimized to system tray on startup")
                print("     Right-click the tray icon to configure or quit")
        except Exception as e:
            print(f"\n[ERR] Error setting up autostart: {e}")
            print("  Make sure pywin32 is installed: pip install pywin32")
        return

    print("\n" + "=" * 60)
    print("  PC STATS MONITOR v3.0")
    print("  Pure Python - No LibreHardwareMonitor needed")
    print("=" * 60 + "\n")

    # Check for config file
    config = load_config()

    # Force configuration or edit mode
    if args.configure or args.edit or config is None:
        if config is None:
            print("\nNo configuration found. Starting GUI...")
        else:
            print("\nOpening configuration editor...")

        # Discover sensors
        discover_sensors()

        # Show GUI
        root = tk.Tk()
        app = MetricSelectorGUI(root, config if args.edit else None)
        root.mainloop()
        # Only destroy if window still exists (user might have closed it via X button)
        try:
            if root.winfo_exists():
                root.destroy()
        except tk.TclError:
            pass  # Window already destroyed

        # Reload config after GUI
        config = load_config()
        if config is None:
            print("\nNo configuration saved. Exiting.")
            return

    # Run monitoring (minimized or console)
    if args.minimized:
        run_minimized(config)
    else:
        run_monitoring(config)


if __name__ == "__main__":
    main()
