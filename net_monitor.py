"""
Network Throughput Monitor - Pure psutil based
Replaces LHM-based network speed monitoring.
Tracks per-interface bytes_sent/bytes_recv deltas over time.
"""

import psutil
import time


class NetThroughputMonitor:
    """
    Monitors per-interface network throughput (KB/s) using psutil deltas.
    """

    def __init__(self):
        self._last_counters = None
        self._last_time = None
        self._interface_names = []  # Ordered list of active interface names

    def _snapshot(self):
        """Take a snapshot of network counters."""
        return psutil.net_io_counters(pernic=True)

    def initialize(self, min_bytes_threshold=1000):
        """
        Initialize with first snapshot.
        Only include interfaces with meaningful traffic.
        Returns list of (interface_name, is_active) tuples.
        """
        counters = self._snapshot()
        self._last_counters = counters
        self._last_time = time.time()

        result = []
        for name in counters:
            nic = counters[name]
            is_active = (nic.bytes_sent + nic.bytes_recv) > min_bytes_threshold
            self._interface_names.append(name)
            result.append((name, is_active))
        return result

    def get_throughput(self, interface_name=None):
        """
        Get current throughput for an interface.
        Returns (upload_kb_s, download_kb_s) tuple.
        Returns (0, 0) if called before initialize or on error.
        Empty string is treated the same as None (auto-detect).
        """
        if not interface_name:
            interface_name = None

        if self._last_counters is None or self._last_time is None:
            return 0, 0

        try:
            counters = self._snapshot()
            now = time.time()
            dt = now - self._last_time

            if dt <= 0:
                return 0, 0

            if interface_name is None:
                # Use first active non-loopback interface
                for name in self._interface_names:
                    if self._is_real_nic(name):
                        interface_name = name
                        break

            if interface_name not in counters:
                return 0, 0

            last = self._last_counters.get(interface_name)
            curr = counters[interface_name]

            if last is None:
                self._last_counters = counters
                self._last_time = now
                return 0, 0

            sent_delta = curr.bytes_sent - last.bytes_sent
            recv_delta = curr.bytes_recv - last.bytes_recv

            sent_kb_s = (sent_delta / 1024) / dt
            recv_kb_s = (recv_delta / 1024) / dt

            self._last_counters = counters
            self._last_time = now

            return sent_kb_s, recv_kb_s

        except Exception:
            return 0, 0

    def _is_real_nic(self, name):
        """Check if this is a real network interface (not loopback/virtual)."""
        name_lower = name.lower()
        for keyword in ['loopback', 'veth', 'docker', 'hyper-v', 'wsl', 'virtualbox', 'vmnet']:
            if keyword in name_lower:
                return False
        return True

    def get_active_interfaces(self):
        """Return list of real (non-virtual) network interface names."""
        return [name for name in self._interface_names if self._is_real_nic(name)]


# Module-level singleton
net_monitor = NetThroughputMonitor()


def initialize_net_monitor():
    """Initialize the network monitor. Returns list of (name, is_active) tuples."""
    return net_monitor.initialize()


def get_net_throughput_kb_s(interface_name=None):
    """
    Get network throughput.
    Returns (upload_kb_s, download_kb_s) tuple.
    """
    return net_monitor.get_throughput(interface_name)


def enumerate_net_sensors():
    """
    Discover network sensors. Returns list compatible with sensor_database format.
    Called during startup to populate the sensor database.
    """
    sensors = []
    interfaces = initialize_net_monitor()
    active_ifaces = net_monitor.get_active_interfaces()

    for name, is_active in interfaces:
        if not net_monitor._is_real_nic(name):
            continue

        # Use shortened interface name for display
        short_name = name[:12]  # Truncate long names
        if len(name) > 12:
            short_name = name[:9] + "..."

        # Upload Speed sensor
        sensors.append({
            "name": f"NET_UP",
            "display_name": f"Upload Speed [{name}]",
            "source": "net",
            "type": "throughput",
            "unit": "KB/s",
            "net_method": "upload",
            "net_interface": name,
            "custom_label": "",
            "current_value": 0,
            "is_active_nic": is_active,
        })

        # Download Speed sensor
        sensors.append({
            "name": f"NET_DN",
            "display_name": f"Download Speed [{name}]",
            "source": "net",
            "type": "throughput",
            "unit": "KB/s",
            "net_method": "download",
            "net_interface": name,
            "custom_label": "",
            "current_value": 0,
            "is_active_nic": is_active,
        })

    return sensors
