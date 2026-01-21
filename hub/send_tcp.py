import socket
import time

# Konfiguration
TARGET_IP = "192.168.220.1"  # Ziel IP-Adresse
TARGET_PORT = 9000           # TCP Port
MESSAGE = bytes([
    0x67,           # Effect (RGB Command)
    0x00, 0x00,    # Duration (0 = permanent)
    0xFF,           # Intensity (max brightness)
    0xFF,           # Red (max)
    0x00,           # Green (0)
    0x00,           # Blue (0)
    0x00,           # Rainbow (off)
    0x00, 0x00,    # Speed (0)
    0x00            # Length (0)
])

def send_tcp_packet():
    try:
        # Socket erstellen
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
        # Verbindung aufbauen
        print(f"Verbinde mit {TARGET_IP}:{TARGET_PORT}...")
        sock.connect((TARGET_IP, TARGET_PORT))
        
        # Nachricht senden
        print(f"Sende Paket: {[hex(b)[2:].zfill(2) for b in MESSAGE]}")
        sock.send(MESSAGE)
        
        # Kurz warten und Socket schließen
        time.sleep(0.1)
        sock.close()
        print("Paket erfolgreich gesendet!")
        
    except Exception as e:
        print(f"Fehler beim Senden: {str(e)}")

if __name__ == "__main__":
    send_tcp_packet() 
