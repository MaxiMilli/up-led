from flask import Flask, request, jsonify
import socket
import threading
import time
import struct
from datetime import datetime
from queue import Queue
import json

app = Flask(__name__)

# Store connected nanos and their command queues
connected_nanos = {}
command_queues = {}

def get_local_ip():
    """Get the local IP address of the machine"""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(('10.255.255.255', 1))
        IP = s.getsockname()[0]
    except Exception:
        IP = '127.0.0.1'
    finally:
        s.close()
    return IP

def create_command_packet(effect, duration, intensity, red, green, blue, rainbow, speed, length):
    """Create a binary command packet"""
    return struct.pack('!BHBBBBBBHB', 
        effect,      # uint8  (1 byte)
        duration,    # uint16 (2 bytes)
        intensity,   # uint8  (1 byte)
        red,        # uint8  (1 byte)
        green,      # uint8  (1 byte)
        blue,       # uint8  (1 byte)
        rainbow,     # uint8  (1 byte)
        speed,      # uint16 (2 bytes)
        length,     # uint8  (1 byte)
        0           # dummy padding byte to match struct size
    )

def handle_nano_connection(client_socket, client_address):
    """Handle individual nano TCP connection"""
    print(f"\nNew TCP connection from {client_address}")
    mac = None
    received_data = ""
    
    try:
        # Read initial data
        data = client_socket.recv(1024).decode()
        received_data = data
        print(f"\nInitial data received ({len(data)} bytes):")
        print(data)
        
        if data.startswith("GET /nano/register"):
            try:
                # Keep reading until we have a complete request
                while "\r\n\r\n" not in received_data:
                    more_data = client_socket.recv(1024).decode()
                    received_data += more_data
                    print(f"Received additional data ({len(more_data)} bytes)")
                
                # Split headers and initial body
                headers, body = received_data.split('\r\n\r\n', 1)
                print("\nHeaders:")
                print(headers)
                print("\nInitial body:")
                print(body)
                
                # Parse Content-Length
                content_length = 0
                for line in headers.split('\r\n'):
                    if line.lower().startswith('content-length:'):
                        content_length = int(line.split(':')[1].strip())
                        print(f"Content-Length found: {content_length}")
                        break
                
                # Read remaining body if needed
                while len(body) < content_length:
                    more_data = client_socket.recv(1024).decode()
                    body += more_data
                    print(f"Read additional body data ({len(more_data)} bytes)")
                
                print("\nComplete body:")
                print(body)
                
                # Parse JSON body
                json_data = json.loads(body)
                mac = json_data['mac']
                # Store connection info
                connected_nanos[mac] = {
                    'ip': client_address[0],
                    'last_seen': datetime.now(),
                    'status': 'active',
                    'socket': client_socket
                }
                command_queues[mac] = Queue()
                print(f"Nano registered via TCP - MAC: {mac}, IP: {client_address[0]}")
                
                # Send success response
                response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
                response += json.dumps({'status': 'success', 'message': 'registered'})
                client_socket.send(response.encode())
                
                # Start command processing loop
                while True:
                    if mac in command_queues and not command_queues[mac].empty():
                        command = command_queues[mac].get()
                        client_socket.send(command)
                    time.sleep(0.01)  # Small delay to prevent CPU hogging
                    
            except json.JSONDecodeError as e:
                print(f"JSON parsing error: {e}")
                print(f"Received data: {body}")
                client_socket.close()
            except KeyError as e:
                print(f"Missing MAC address in request: {e}")
                print(f"Received JSON: {json_data}")
                client_socket.close()
            except Exception as e:
                print(f"Error processing registration: {e}")
                print(f"Full received data: {data}")
                client_socket.close()
        else:
            print(f"Invalid request: {data[:100]}...")  # Print first 100 chars
            client_socket.close()
    except Exception as e:
        print(f"Connection error: {e}")
        if 'data' in locals():
            print(f"Last received data: {data}")
    finally:
        if mac and mac in connected_nanos:
            del connected_nanos[mac]
            if mac in command_queues:
                del command_queues[mac]
        try:
            client_socket.close()
        except:
            pass

def tcp_server():
    """TCP server for nano connections"""
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(('0.0.0.0', 3000))
    server.listen(5)
    
    while True:
        client, address = server.accept()
        thread = threading.Thread(target=handle_nano_connection, args=(client, address))
        thread.daemon = True
        thread.start()

# HTTP Endpoints
@app.route('/nano/register', methods=['GET'])
def register_nano():
    """Handle nano registration via HTTP (for testing)"""
    try:
        data = request.get_json(silent=True)
        if data and 'mac' in data:
            mac = data['mac']
            ip = request.remote_addr
            connected_nanos[mac] = {
                'ip': ip,
                'last_seen': datetime.now(),
                'status': 'active'
            }
            command_queues[mac] = Queue()
            print(f"Nano registered via HTTP - MAC: {mac}, IP: {ip}")
            return jsonify({'status': 'success', 'message': 'registered'})
        return jsonify({'status': 'error', 'message': 'invalid request'}), 400
    except Exception as e:
        print(f"Error in registration: {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

@app.route('/nano/heartbeat', methods=['GET'])
def heartbeat():
    """Handle nano heartbeat"""
    mac = request.args.get('mac')
    if mac in connected_nanos:
        connected_nanos[mac]['last_seen'] = datetime.now()
        return jsonify({'status': 'success'})
    return jsonify({'status': 'error', 'message': 'unknown device'}), 404

@app.route('/status', methods=['GET'])
def get_status():
    """Get status of all connected nanos"""
    status_data = {}
    for mac, data in connected_nanos.items():
        status_data[mac] = {
            'ip': data['ip'],
            'last_seen': data['last_seen'].isoformat(),
            'status': data['status']
        }
    return jsonify({
        'connected_nanos': status_data,
        'server_time': datetime.now().isoformat()
    })

@app.route('/command', methods=['POST'])
def send_command():
    """Send command to specific or all nanos"""
    try:
        data = request.get_json()
        if not data:
            return jsonify({'status': 'error', 'message': 'no data provided'}), 400
        
        # Validate and convert input values
        try:
            command = create_command_packet(
                effect=int(data.get('effect', 0)) & 0xFF,        # uint8
                duration=int(data.get('duration', 0)) & 0xFFFF,  # uint16
                intensity=int(data.get('intensity', 255)) & 0xFF,# uint8
                red=int(data.get('red', 0)) & 0xFF,             # uint8
                green=int(data.get('green', 0)) & 0xFF,         # uint8
                blue=int(data.get('blue', 0)) & 0xFF,           # uint8
                rainbow=int(data.get('rainbow', 0)) & 0xFF,      # uint8
                speed=int(data.get('speed', 0)) & 0xFFFF,       # uint16
                length=int(data.get('length', 0)) & 0xFF        # uint8
            )
        except ValueError as e:
            return jsonify({
                'status': 'error',
                'message': f'Invalid value in command: {str(e)}'
            }), 400
        
        # Send to specific nano or all
        target_mac = data.get('mac')
        if target_mac:
            if target_mac in command_queues:
                command_queues[target_mac].put(command)
                return jsonify({'status': 'success', 'message': f'command sent to {target_mac}'})
            return jsonify({'status': 'error', 'message': 'nano not found'}), 404
        else:
            # Send to all connected nanos
            for mac in command_queues:
                command_queues[mac].put(command)
                print(f"Command sent to nano {mac} - Effect: {data.get('effect')}, RGB: ({data.get('red')},{data.get('green')},{data.get('blue')})")
            return jsonify({'status': 'success', 'message': 'command sent to all nanos'})
            
    except Exception as e:
        return jsonify({'status': 'error', 'message': str(e)}), 500

@app.route('/effects', methods=['GET'])
def get_effects():
    """Get list of available effects"""
    effects = {
        '0x64': 'Off',
        '0x65': 'Standby',
        '0x66': 'Active Standby',
        '0x67': 'RGB',
        '30': 'Running Light',
        '31': 'Glitter',
        '32': 'Wave',
        '33': 'Pulsate',
        '35': 'Strobo',
        '36': 'Fade'
    }
    return jsonify(effects)

def cleanup_old_connections():
    """Remove nanos that haven't sent a heartbeat in 5 minutes"""
    while True:
        try:
            now = datetime.now()
            to_remove = []
            for mac, data in connected_nanos.items():
                if (now - data['last_seen']).total_seconds() > 300:  # 5 minutes
                    to_remove.append(mac)
            
            for mac in to_remove:
                if mac in command_queues:
                    del command_queues[mac]
                del connected_nanos[mac]
                print(f"Removed inactive nano: {mac}")
        except Exception as e:
            print(f"Error in cleanup: {e}")
        time.sleep(60)  # Check every minute

if __name__ == '__main__':
    local_ip = get_local_ip()
    
    print(f"Starting server on {local_ip}")
    print("Available endpoints:")
    print("TCP Server (port 3000):")
    print(f"  tcp://{local_ip}:3000 - Direct nano connections")
    print("\nHTTP Server (port 3001):")
    print(f"  http://{local_ip}:3001/nano/register - Register nano")
    print(f"  http://{local_ip}:3001/nano/heartbeat - Heartbeat endpoint")
    print(f"  http://{local_ip}:3001/status - Get status of connected nanos")
    print(f"  http://{local_ip}:3001/command - Send commands (POST)")
    print(f"  http://{local_ip}:3001/effects - Get available effects")
    
    # Start TCP server thread
    tcp_thread = threading.Thread(target=tcp_server, daemon=True)
    tcp_thread.start()
    
    # Start cleanup thread
    cleanup_thread = threading.Thread(target=cleanup_old_connections, daemon=True)
    cleanup_thread.start()
    
    # Start Flask server
    app.run(host='0.0.0.0', port=3001, debug=True, use_reloader=False)
