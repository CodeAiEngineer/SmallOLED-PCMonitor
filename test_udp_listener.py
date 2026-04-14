"""
UDP Listener Test - Verifies Python script is sending packets correctly
This acts as a fake ESP32 to catch the UDP packets
"""
import socket
import json

UDP_PORT = 4210

print("=" * 60)
print("  UDP Packet Receiver Test")
print("=" * 60)
print()
print(f"Listening on UDP port {UDP_PORT}...")
print("Start the Python monitor script in another terminal")
print("Press Ctrl+C to stop\n")

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('', UDP_PORT))

packet_count = 0

try:
    while True:
        data, addr = sock.recvfrom(2048)
        packet_count += 1
        
        print(f"\n[Packet #{packet_count}] From: {addr}")
        print(f"Size: {len(data)} bytes")
        
        try:
            payload = json.loads(data.decode('utf-8'))
            print(f"Version: {payload.get('version', 'unknown')}")
            print(f"Status: {payload.get('status', 0)}")
            print(f"Timestamp: {payload.get('timestamp', 'empty')}")
            print(f"Metrics: {len(payload.get('metrics', []))}")
            
            for m in payload.get('metrics', [])[:5]:
                print(f"  - {m['name']}: {m['value']}{m['unit']}")
        except Exception as e:
            print(f"JSON parse error: {e}")
            print(f"Raw data: {data[:200]}")
            
except KeyboardInterrupt:
    print(f"\n\nStopped. Received {packet_count} packets total.")
    sock.close()
