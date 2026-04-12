"""
HWiNFO Sensor Module - Reads CPU temperature and other sensors
from HWiNFO64 shared memory (no CSV, real-time).
Based on the reverse-engineered shared memory format.

Requires:
  1. HWiNFO64 installed and running
  2. "Shared Memory Support" enabled in HWiNFO settings
"""

import ctypes
import ctypes.wintypes
import struct
import time

# Shared memory constants
HWINFO_MAP_ID_64 = "CoreTempHWINFO_MAPPING_NAME"  # fallback
HWINFO_SHARED_ID = "Global\\Access"

# Attempt to import the shared memory reader
HWINFOSHARE_AVAILABLE = False
_hwinfo_data = {}
_last_update = 0
_update_interval = 1.0  # Cache for 1 second


class HWINFO_SHARED_MEMORY(ctypes.Structure):
    """
    Simplified HWiNFO64 shared memory structure.
    The actual format is complex and version-dependent.
    This is a minimal reader for sensor values.
    """
    _pack_ = 1
    # First 4 bytes = signature
    # Followed by version, number of sensors, then sensor data blocks
    # Each sensor: 416 bytes with name, value, unit, min, max
    _fields_ = [
        ("Signature", ctypes.c_uint32),
        ("Version", ctypes.c_uint32),
        ("Revision", ctypes.c_uint32),
        ("PollTime", ctypes.c_uint32),
        ("NumSensors", ctypes.c_uint16),
        ("SensorsOffset", ctypes.c_uint32),
    ]


class HWINFOSensor(ctypes.Structure):
    """Single sensor entry in HWiNFO shared memory."""
    _pack_ = 1
    _fields_ = [
        ("Name", ctypes.c_char * 128),
        ("Unit", ctypes.c_char * 16),
        ("Value", ctypes.c_double),
        ("ValueMin", ctypes.c_double),
        ("ValueMax", ctypes.c_double),
        ("AlertTriggered", ctypes.c_bool),
    ]


SENSOR_SIZE = ctypes.sizeof(HWINFOSensor)  # Should be ~152 bytes


def read_hwinfo_shared_memory():
    """
    Read all sensors from HWiNFO64 shared memory.
    Returns dict of {sensor_name: (value, unit)} or None on failure.
    """
    try:
        # Try the primary mapping name used by HWiNFO64
        map_names = [
            "Global\\Access",
            "Access",
        ]

        hmap = None
        for map_name in map_names:
            try:
                hmap = ctypes.windll.kernel32.OpenFileMappingA(
                    0x0004,  # FILE_MAP_READ
                    False,
                    map_name.encode('ascii')
                )
                if hmap:
                    break
            except Exception:
                continue

        if not hmap:
            return None

        try:
            pbuf = ctypes.windll.kernel32.MapViewOfFile(
                hmap,
                0x0004,  # FILE_MAP_READ
                0, 0, 0
            )
            if not pbuf:
                return None

            try:
                # Read header
                header = HWINFO_SHARED_MEMORY.from_address(pbuf)

                if header.Signature != 0x46495748:  # "HWIF" reversed
                    # Different signature - format may vary
                    pass

                sensors = {}
                sensor_ptr = pbuf + header.SensorsOffset

                for i in range(header.NumSensors):
                    try:
                        sensor = HWINFOSensor.from_address(sensor_ptr + i * SENSOR_SIZE)
                        name = sensor.Name.decode('utf-8', errors='replace').strip('\x00')
                        unit = sensor.Unit.decode('utf-8', errors='replace').strip('\x00')
                        if name:
                            sensors[name] = (sensor.Value, unit)
                    except Exception:
                        continue

                return sensors
            finally:
                ctypes.windll.kernel32.UnmapViewOfFile(pbuf)
        finally:
            ctypes.windll.kernel32.CloseHandle(hmap)

    except Exception:
        return None


def _refresh_cache():
    """Refresh the sensor data cache."""
    global _hwinfo_data, _last_update
    now = time.time()
    if now - _last_update < _update_interval:
        return

    data = read_hwinfo_shared_memory()
    if data:
        _hwinfo_data = data
        _last_update = now


def get_sensor_value(name_contains, unit_filter=None):
    """
    Find a sensor by name substring and return its value.
    Returns (value, unit) tuple or (None, None) if not found.
    """
    _refresh_cache()
    name_lower = name_contains.lower()

    for sensor_name, (value, unit) in _hwinfo_data.items():
        if name_lower in sensor_name.lower():
            if unit_filter is None or unit_filter.lower() in unit.lower():
                return value, unit

    return None, None


def get_cpu_temp():
    """Get CPU package temperature. Returns None if not available."""
    # Try common CPU temp sensor names
    candidates = [
        ("CPU Package", "°C"),
        ("CPU (Tctl/Tdie)", "°C"),
        ("CPU Core", "°C"),
        ("Core #1 Max", "°C"),
        ("Package", "°C"),
    ]
    for name, unit in candidates:
        val, u = get_sensor_value(name, unit)
        if val is not None:
            return val
    return None


def get_fan_speed(fan_name="CPU"):
    """Get fan speed in RPM. Returns None if not available."""
    val, u = get_sensor_value(fan_name, "RPM")
    return val


def enumerate_hwinfo_sensors():
    """
    Discover all available HWiNFO sensors.
    Returns list of dicts compatible with sensor_database format.
    """
    sensors = []
    _refresh_cache()

    if not _hwinfo_data:
        return sensors

    # Track used names to avoid duplicates
    used_names = set()

    for sensor_name, (value, unit) in _hwinfo_data.items():
        # Determine type and short name
        sensor_type = "other"
        short_name = None

        name_lower = sensor_name.lower()

        # Temperature
        if unit == "°C" or "temp" in name_lower:
            sensor_type = "temperature"
            if "cpu" in name_lower and "package" in name_lower:
                short_name = "CPU_TEMP"
            elif "cpu" in name_lower:
                short_name = f"CPU_T{len([n for n in used_names if 'CPU_T' in n])}"
            elif "gpu" in name_lower:
                short_name = "GPUT"
            elif "hotspot" in name_lower or "junction" in name_lower:
                short_name = "GPU_HOT"
            elif "memory" in name_lower or "vram" in name_lower:
                short_name = "VRAM_T"
            else:
                short_name = f"T{len(used_names)}"

        # Fan
        elif unit == "RPM" or "fan" in name_lower:
            sensor_type = "fan"
            if "cpu" in name_lower:
                short_name = "CPU_FAN"
            elif "gpu" in name_lower or "graphics" in name_lower:
                short_name = "GPU_FAN"
            elif "case" in name_lower or "chassis" in name_lower:
                short_name = "CASE_FAN"
            elif "pump" in name_lower:
                short_name = "PUMP"
            else:
                short_name = f"FAN{len([n for n in used_names if 'FAN' in n])}"

        # Voltage
        elif unit == "V" or "voltage" in name_lower:
            sensor_type = "voltage"
            if "cpu" in name_lower:
                short_name = "CPU_V"
            elif "gpu" in name_lower:
                short_name = "GPU_V"
            else:
                short_name = f"V{len(used_names)}"

        # Power
        elif unit == "W" or "power" in name_lower:
            sensor_type = "power"
            if "cpu" in name_lower:
                short_name = "CPU_W"
            elif "gpu" in name_lower:
                short_name = "GPU_W"
            else:
                short_name = f"W{len(used_names)}"

        # Load/Utilization
        elif unit == "%" and ("load" in name_lower or "utilization" in name_lower):
            sensor_type = "load"
            if "cpu" in name_lower:
                short_name = "CPU_LOAD"
            elif "gpu" in name_lower:
                short_name = "GPU_LOAD"
            else:
                short_name = f"LD{len(used_names)}"

        # Clock
        elif unit == "MHz" or "clock" in name_lower:
            sensor_type = "clock"
            if "cpu" in name_lower:
                short_name = "CPU_CLK"
            elif "gpu" in name_lower:
                short_name = "GPU_CLK"
            else:
                short_name = f"CLK{len(used_names)}"

        if short_name is None:
            continue

        # Ensure unique short name
        base_name = short_name
        counter = 0
        while short_name in used_names:
            counter += 1
            short_name = f"{base_name}_{counter}"
        used_names.add(short_name)

        sensors.append({
            "name": short_name[:10],
            "display_name": sensor_name,
            "source": "hwinfo",
            "type": sensor_type,
            "unit": unit,
            "hwinfo_sensor_name": sensor_name,
            "custom_label": "",
            "current_value": int(value) if value is not None else 0,
        })

    return sensors


def is_hwinfo_running():
    """Check if HWiNFO64 shared memory is accessible."""
    _refresh_cache()
    return len(_hwinfo_data) > 0


# Initialize on module load
if is_hwinfo_running():
    print(f"  ✓ HWiNFO64 shared memory connected ({len(_hwinfo_data)} sensors)")
else:
    print("  ⚠ HWiNFO64 not detected or shared memory not enabled")
    print("    Install HWiNFO64 and enable 'Shared Memory Support' in settings")
