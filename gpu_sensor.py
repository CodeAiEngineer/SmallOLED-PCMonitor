"""
GPU Sensor Module - NVIDIA (pynvml) / AMD (pyamdgpuinfo)
Replaces LHM-based GPU temperature, VRAM, clock, and load monitoring.
"""

import warnings
warnings.filterwarnings('ignore', category=FutureWarning)

import time

# Try NVIDIA pynvml
NVML_AVAILABLE = False
try:
    import pynvml
    pynvml.nvmlInit()
    _gpu_count = pynvml.nvmlDeviceGetCount()
    NVML_AVAILABLE = True
except Exception:
    pass

# Try AMD
AMD_AVAILABLE = False
try:
    import pyamdgpuinfo
    _amd_gpu_count = pyamdgpuinfo.detect_gpus()
    if _amd_gpu_count > 0:
        AMD_AVAILABLE = True
except Exception:
    pass


def get_gpu_count():
    """Return number of available GPUs."""
    if NVML_AVAILABLE:
        return pynvml.nvmlDeviceGetCount()
    if AMD_AVAILABLE:
        return _amd_gpu_count
    return 0


def get_gpu_temp(gpu_index=0):
    """Get GPU temperature in Celsius. Returns None on failure."""
    try:
        if NVML_AVAILABLE:
            handle = pynvml.nvmlDeviceGetHandleByIndex(gpu_index)
            return pynvml.nvmlDeviceGetTemperature(handle, pynvml.NVML_TEMPERATURE_GPU)
        if AMD_AVAILABLE:
            gpu = pyamdgpuinfo.get_gpu(gpu_index)
            return gpu.query_temperature()
    except Exception:
        pass
    return None


def get_gpu_vram_percent(gpu_index=0):
    """Get GPU VRAM usage as percentage. Returns None on failure."""
    try:
        if NVML_AVAILABLE:
            handle = pynvml.nvmlDeviceGetHandleByIndex(gpu_index)
            mem = pynvml.nvmlDeviceGetMemoryInfo(handle)
            if mem.total > 0:
                return round((mem.used / mem.total) * 100, 1)
        if AMD_AVAILABLE:
            gpu = pyamdgpuinfo.get_gpu(gpu_index)
            mem = gpu.memory_info
            if mem.total > 0:
                return round((mem.used / mem.total) * 100, 1)
    except Exception:
        pass
    return None


def get_gpu_vram_used_mb(gpu_index=0):
    """Get GPU VRAM used in MB. Returns None on failure."""
    try:
        if NVML_AVAILABLE:
            handle = pynvml.nvmlDeviceGetHandleByIndex(gpu_index)
            mem = pynvml.nvmlDeviceGetMemoryInfo(handle)
            return round(mem.used / (1024 * 1024), 1)
        if AMD_AVAILABLE:
            gpu = pyamdgpuinfo.get_gpu(gpu_index)
            mem = gpu.memory_info
            return round(mem.used / (1024 * 1024), 1)
    except Exception:
        pass
    return None


def get_gpu_clock_mhz(gpu_index=0):
    """Get GPU core clock in MHz. Returns None on failure."""
    try:
        if NVML_AVAILABLE:
            handle = pynvml.nvmlDeviceGetHandleByIndex(gpu_index)
            return pynvml.nvmlDeviceGetClockInfo(handle, pynvml.NVML_CLOCK_GRAPHICS)
        if AMD_AVAILABLE:
            gpu = pyamdgpuinfo.get_gpu(gpu_index)
            return gpu.query_gpu_clk()
    except Exception:
        pass
    return None


def get_gpu_load_percent(gpu_index=0):
    """Get GPU utilization percentage. Returns None on failure."""
    try:
        if NVML_AVAILABLE:
            handle = pynvml.nvmlDeviceGetHandleByIndex(gpu_index)
            util = pynvml.nvmlDeviceGetUtilizationRates(handle)
            return util.gpu
        if AMD_AVAILABLE:
            gpu = pyamdgpuinfo.get_gpu(gpu_index)
            return gpu.query_load()
    except Exception:
        pass
    return None


def get_gpu_name(gpu_index=0):
    """Get GPU name string."""
    try:
        if NVML_AVAILABLE:
            handle = pynvml.nvmlDeviceGetHandleByIndex(gpu_index)
            name = pynvml.nvmlDeviceGetName(handle)
            # pynvml 13+ returns bytes, older versions return str
            if isinstance(name, bytes):
                name = name.decode('utf-8', errors='replace')
            return name
        if AMD_AVAILABLE:
            gpu = pyamdgpuinfo.get_gpu(gpu_index)
            return f"AMD GPU {gpu_index}"
    except Exception:
        pass
    return f"GPU {gpu_index}"


def enumerate_gpu_sensors():
    """
    Discover all available GPU sensors.
    Returns list of dicts compatible with sensor_database format.
    """
    sensors = []
    count = get_gpu_count()
    if count == 0:
        return sensors

    for gpu_idx in range(count):
        gpu_name = get_gpu_name(gpu_idx)
        gpu_suffix = "" if gpu_idx == 0 else str(gpu_idx)

        # GPU Temperature
        if get_gpu_temp(gpu_idx) is not None:
            sensors.append({
                "name": f"GPUT{gpu_suffix}",
                "display_name": f"GPU Core Temp{gpu_suffix}",
                "source": "nvidia" if NVML_AVAILABLE else "amd",
                "type": "temperature",
                "unit": "C",
                "gpu_method": "temp",
                "gpu_index": gpu_idx,
                "custom_label": "",
                "current_value": get_gpu_temp(gpu_idx),
            })

        # GPU VRAM %
        if get_gpu_vram_percent(gpu_idx) is not None:
            sensors.append({
                "name": f"VRAM{gpu_suffix}",
                "display_name": f"GPU VRAM Usage{gpu_suffix}",
                "source": "nvidia" if NVML_AVAILABLE else "amd",
                "type": "load",
                "unit": "%",
                "gpu_method": "vram_percent",
                "gpu_index": gpu_idx,
                "custom_label": "",
                "current_value": int(get_gpu_vram_percent(gpu_idx)),
            })

        # GPU VRAM used (MB)
        if get_gpu_vram_used_mb(gpu_idx) is not None:
            sensors.append({
                "name": f"VRAM_MB{gpu_suffix}",
                "display_name": f"GPU VRAM Used{gpu_suffix}",
                "source": "nvidia" if NVML_AVAILABLE else "amd",
                "type": "data",
                "unit": "MB",
                "gpu_method": "vram_used_mb",
                "gpu_index": gpu_idx,
                "custom_label": "",
                "current_value": int(get_gpu_vram_used_mb(gpu_idx)),
            })

        # GPU Clock
        if get_gpu_clock_mhz(gpu_idx) is not None:
            sensors.append({
                "name": f"GPUCLK{gpu_suffix}",
                "display_name": f"GPU Clock{gpu_suffix}",
                "source": "nvidia" if NVML_AVAILABLE else "amd",
                "type": "clock",
                "unit": "MHz",
                "gpu_method": "clock_mhz",
                "gpu_index": gpu_idx,
                "custom_label": "",
                "current_value": get_gpu_clock_mhz(gpu_idx),
            })

        # GPU Load
        if get_gpu_load_percent(gpu_idx) is not None:
            sensors.append({
                "name": f"GPU{gpu_suffix}",
                "display_name": f"GPU Load{gpu_suffix}",
                "source": "nvidia" if NVML_AVAILABLE else "amd",
                "type": "load",
                "unit": "%",
                "gpu_method": "load_percent",
                "gpu_index": gpu_idx,
                "custom_label": "",
                "current_value": int(get_gpu_load_percent(gpu_idx)),
            })

    return sensors


def cleanup():
    """Clean up NVML resources."""
    global NVML_AVAILABLE
    if NVML_AVAILABLE:
        try:
            pynvml.nvmlShutdown()
        except Exception:
            pass


# Initialize NVML at module load
if NVML_AVAILABLE:
    print(f"  ✓ NVIDIA NVML initialized ({_gpu_count} GPU(s) detected)")
if AMD_AVAILABLE:
    print(f"  ✓ AMD GPU support initialized ({_amd_gpu_count} GPU(s) detected)")
if not NVML_AVAILABLE and not AMD_AVAILABLE:
    print("  ⚠ No GPU detected or GPU libraries not available")
