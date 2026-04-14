"""
UDP Connection Diagnostic Tool for PC Monitor
Tests if ESP32 is receiving UDP packets
"""
import socket
import json
import time

ESP32_IP = "192.168.1.162"
UDP_PORT = 4210

print("=" * 60)
print("  PC Monitor - UDP Connection Diagnostic")
print("=" * 60)
print()

# Step 1: Check network connectivity
print("[1/4] Testing network connectivity...")
try:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(2)
    result = sock.connect_ex((ESP32_IP, 80))
    sock.close()
    if result == 0:
        print(f"  ✓ ESP32 ({ESP32_IP}) is reachable on network")
    else:
        print(f"  ✗ ESP32 ({ESP32_IP}) is NOT reachable!")
        print(f"  → Check if ESP32 is powered on and connected to WiFi")
        exit(1)
except Exception as e:
    print(f"  ✗ Connection test failed: {e}")
    exit(1)

# Step 2: Send test UDP packets
print()
print("[2/4] Sending test UDP packets...")
test_payload = {
    "version": "3.0",
    "status": 1,
    "timestamp": "TEST",
    "metrics": [
        {"id": 1, "name": "TEST", "value": 99, "unit": "%"}
    ]
}

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

for i in range(10):
    message = json.dumps(test_payload).encode('utf-8')
    sock.sendto(message, (ESP32_IP, UDP_PORT))
    print(f"  Packet {i+1}/10 sent ({len(message)} bytes)")
    time.sleep(0.5)

sock.close()
print("  ✓ 10 packets sent to ESP32")

# Step 3: Check web interface
print()
print("[3/4] Checking ESP32 web interface...")
try:
    import urllib.request
    url = f"http://{ESP32_IP}/metrics"
    req = urllib.request.Request(url, method='GET')
    req.add_header('Accept', 'application/json')
    
    with urllib.request.urlopen(req, timeout=3) as response:
        data = json.loads(response.read().decode())
        metrics = data.get('metrics', [])
        
        if len(metrics) > 0:
            print(f"  ✓ ESP32 received {len(metrics)} metrics!")
            print(f"  → Web interface shows: {url}")
            for m in metrics:
                print(f"    - {m['name']}: {m['value']}{m['unit']}")
        else:
            print(f"  ✗ ESP32 shows 0 metrics")
            print(f"  → Packets NOT reaching ESP32!")
            print()
            print("  POSSIBLE CAUSES:")
            print("  1. Windows Firewall blocking UDP port 4210")
            print("  2. ESP32 firmware issue (check serial monitor)")
            print("  3. ESP32 not on port 4210 (check config)")
            print()
            print("  SOLUTIONS:")
            print("  → Check ESP32 serial monitor for 'UDP packet' messages")
            print("  → Add Windows Firewall rule for UDP port 4210")
            print(f"  → Open web interface: http://{ESP32_IP}")
except Exception as e:
    print(f"  ✗ Web check failed: {e}")
    print(f"  → ESP32 web server may not be running")

# Step 4: Test with broadcast
print()
print("[4/4] Testing with broadcast packets...")
broadcast_payload = {
    "version": "3.0",
    "status": 1,
    "timestamp": "BCAST",
    "metrics": [
        {"id": 1, "name": "BCAST", "value": 100, "unit": "X"}
    ]
}

broadcast_ip = "192.168.1.255"
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

message = json.dumps(broadcast_payload).encode('utf-8')
for i in range(5):
    sock.sendto(message, (broadcast_ip, UDP_PORT))
    print(f"  Broadcast {i+1}/5 sent")
    time.sleep(0.5)

sock.close()

print()
print("=" * 60)
print("  DIAGNOSTIC COMPLETE")
print("=" * 60)
print()
print("Next steps:")
print("1. Open ESP32 Serial Monitor (PlatformIO/Arduino IDE)")
print("2. Look for: 'UDP packet: X bytes, read: Y bytes'")
print("3. If NO messages appear → ESP32 not receiving packets")
print("4. If messages appear but 'JSON parse error' → Format issue")
print()
print(f"Web interface: http://{ESP32_IP}")
print()
