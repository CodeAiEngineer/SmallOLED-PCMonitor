"""
Test UDP packet sending to ESP32
This verifies that UDP packets are being sent correctly
"""
import socket
import json

ESP32_IP = "192.168.1.162"
UDP_PORT = 4210

# Create test payload
test_payload = {
    "version": "3.0",
    "status": 1,
    "timestamp": "22:50",
    "metrics": [
        {"id": 1, "name": "CPU", "value": 25, "unit": "%"},
        {"id": 2, "name": "RAM", "value": 68, "unit": "%"},
        {"id": 3, "name": "GPUT", "value": 40, "unit": "C"},
        {"id": 4, "name": "VRAM", "value": 10, "unit": "%"},
        {"id": 5, "name": "NET", "value": 44, "unit": "KB/s"}
    ]
}

print(f"Sending test UDP packet to {ESP32_IP}:{UDP_PORT}")
print(f"Payload: {json.dumps(test_payload)}")

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
message = json.dumps(test_payload).encode('utf-8')

for i in range(5):
    sock.sendto(message, (ESP32_IP, UDP_PORT))
    print(f"Packet {i+1} sent!")
    import time
    time.sleep(1)

sock.close()
print("\nDone! Check ESP32 serial monitor for 'UDP packet: X bytes' messages")
