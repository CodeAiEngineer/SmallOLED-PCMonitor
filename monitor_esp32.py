"""
Continuous ESP32 Metrics Monitor
Checks if the ESP32 is receiving UDP packets consistently
"""
import urllib.request
import json
import time

ESP32_IP = "192.168.1.162"
URL = f"http://{ESP32_IP}/metrics"

print("=" * 60)
print("  ESP32 Metrics Monitor")
print("=" * 60)
print()
print("Checking metrics every 2 seconds...")
print("Press Ctrl+C to stop\n")

last_online = False
consecutive_offline = 0

try:
    while True:
        try:
            req = urllib.request.Request(URL, method='GET')
            req.add_header('Accept', 'application/json')
            
            with urllib.request.urlopen(req, timeout=2) as response:
                data = json.loads(response.read().decode())
                
                online = data.get('online', False)
                timestamp = data.get('timestamp', '')
                metrics = data.get('metrics', [])
                metric_count = len(metrics)
                
                status = "ONLINE" if online else "OFFLINE"
                icon = "✓" if online else "✗"
                
                print(f"[{time.strftime('%H:%M:%S')}] {icon} {status:8s} | Metrics: {metric_count} | Timestamp: {timestamp}")
                
                if metric_count > 0:
                    values_str = " | ".join([f"{m['name']}:{m['value']}{m['unit']}" for m in metrics[:3]])
                    if metric_count > 3:
                        values_str += f" ... +{metric_count-3}"
                    print(f"           {values_str}")
                
                # Track online/offline transitions
                if online and not last_online:
                    print("           >>> CAME ONLINE <<<")
                elif not online and last_online:
                    print("           >>> WENT OFFLINE (timeout!) <<<")
                    consecutive_offline += 1
                
                last_online = online
                
        except Exception as e:
            print(f"[{time.strftime('%H:%M:%S')}] ✗ ERROR: {e}")
            last_online = False
        
        time.sleep(2)
        
except KeyboardInterrupt:
    print("\n\nStopped monitoring.")
    if consecutive_offline > 0:
        print(f"Went offline {consecutive_offline} time(s) - UDP packets NOT being received consistently!")
        print("Check:")
        print("  1. Python script is running and sending")
        print("  2. Windows Firewall not blocking UDP")
        print("  3. ESP32 and PC on same network")
        print("  4. ESP32 UDP listener working correctly")
