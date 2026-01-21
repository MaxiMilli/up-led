from datetime import datetime, timedelta
from typing import Dict, Optional
from dataclasses import dataclass, asdict
import asyncio
from ..websocket.websocket_manager import WebSocketManager
import json
import os
from .nano_info import NanoInfo, Position

@dataclass
class NanoConnection:
    ip: str
    last_seen: datetime
    status: str
    writer: asyncio.StreamWriter
    reader: asyncio.StreamReader

    def to_dict(self):
        return {
            "ip": self.ip,
            "last_seen": self.last_seen.isoformat(),
            "status": self.status
        }

class NanoManager:
    _instance = None
    STORAGE_FILE = "data/nano_info.json"
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(NanoManager, cls).__new__(cls)
            cls._instance.connected_nanos: Dict[str, NanoConnection] = {}
            cls._instance.nano_info: Dict[str, NanoInfo] = {}
            cls._instance.websocket_manager = WebSocketManager()
            cls._instance._load_nano_info()
        return cls._instance

    def _load_nano_info(self):
        """Load nano information from storage file"""
        os.makedirs(os.path.dirname(self.STORAGE_FILE), exist_ok=True)
        try:
            if os.path.exists(self.STORAGE_FILE):
                with open(self.STORAGE_FILE, 'r') as f:
                    data = json.load(f)
                    self.nano_info = {
                        mac: NanoInfo.from_dict(info) for mac, info in data.items()
                    }
        except Exception as e:
            print(f"Error loading nano info: {e}")
            self.nano_info = {}

    def _save_nano_info(self):
        """Save nano information to storage file"""
        try:
            with open(self.STORAGE_FILE, 'w') as f:
                data = {
                    mac: info.to_dict() for mac, info in self.nano_info.items()
                }
                json.dump(data, f, indent=2)
        except Exception as e:
            print(f"Error saving nano info: {e}")

    async def update_nano_info(self, mac: str, **updates) -> bool:
        """Update nano information and persist changes"""
        try:
            if mac not in self.nano_info:
                self.nano_info[mac] = NanoInfo(mac=mac)
            
            info = self.nano_info[mac]
            updated = False
            
            # Handle special updates that require TCP commands
            if 'led_count' in updates:
                # Create LED count command (108)
                led_command = bytes([
                    108,            # Effect (Set LED Count)
                    0x00, 0x00,    # Duration (0)
                    0xFF,          # Intensity (max)
                    0x00,          # Red (unused)
                    0x00,          # Green (unused)
                    0x00,          # Blue (unused)
                    0x00,          # Rainbow (unused)
                    0x00, 0x00,    # Speed (unused)
                    updates['led_count']  # Length (LED count)
                ])
                if await self._send_command(mac, led_command):
                    setattr(info, 'led_count', updates['led_count'])
                    print(f"Successfully updated led_count for {mac}")
                    updated = True
                else:
                    return False

            if 'gwaendli_color' in updates:
                # Map color names to RGB values
                COLOR_RGB_MAP = {
                    'cyan': (0, 255, 255),
                    'magenta': (255, 0, 255),
                    'yellow': (255, 255, 0)
                }
                rgb = COLOR_RGB_MAP.get(updates['gwaendli_color'])
                if rgb is not None:
                    # Create color command (109)
                    color_command = bytes([
                        109,            # Effect (Set Color)
                        0x00, 0x00,    # Duration (0)
                        0xFF,          # Intensity (max)
                        rgb[0],        # Red
                        rgb[1],        # Green
                        rgb[2],        # Blue
                        0x00,          # Rainbow (off)
                        0x00, 0x00,    # Speed (unused)
                        0x00           # Length (unused)
                    ])
                    print(f"Sending color command to {mac}: {[hex(b)[2:].zfill(2) for b in color_command]}")
                    if await self._send_command(mac, color_command):
                        setattr(info, 'gwaendli_color', updates['gwaendli_color'])
                        updated = True
                        print(f"Successfully updated gwaendli_color for {mac}")
                    else:
                        print(f"Failed to send color command to {mac}")
                        return False

            # Handle other updates
            for key, value in updates.items():
                if key not in ['led_count', 'gwaendli_color']:  # Skip already handled updates
                    if key == 'position' and value:
                        info.position = Position(x=value['x'], y=value['y'])
                        updated = True
                    elif hasattr(info, key) and getattr(info, key) != value:
                        setattr(info, key, value)
                        updated = True

            if updated:
                self._save_nano_info()
                # Broadcast update via WebSocket
                await self.websocket_manager.broadcast_message({
                    "type": "nano_info_updated",
                    "mac": mac,
                    "updates": updates
                })

                # Send strobo effect confirmation only if the update was successful
                strobo_command = bytes([
                    36,           # Effect (Strobo)
                    0x03, 0xE8,  # Duration (1000ms = 1s)
                    0xFF,        # Intensity (max)
                    0xFF,        # Red (max)
                    0x00,        # Green (max)
                    0x00,        # Blue (max)
                    0x00,        # Rainbow (off)
                    0x00, 0x64,  # Speed (100ms)
                    0x00         # Length (0)
                ])
                await self.broadcast_command(strobo_command, target_mac=mac)

                # Wait for strobo to finish
                await asyncio.sleep(1.5)

                # Return to active standby
                active_standby_command = bytes([
                    0x66,        # Effect (Active Standby)
                    0x00, 0x00,  # Duration (0 = permanent)
                    0x40,        # Intensity (25%)
                    0xFF,        # Red (max)
                    0xFF,        # Green (max)
                    0xFF,        # Blue (max)
                    0x00,        # Rainbow (off)
                    0x00, 0x00,  # Speed (0)
                    0x00         # Length (0)
                ])
                await self.broadcast_command(active_standby_command, target_mac=mac)

                return True
            return True
        except Exception as e:
            print(f"Error updating nano info for {mac}: {e}")
            return False

    def get_nano_info(self, mac: str) -> Optional[NanoInfo]:
        """Get stored information for a specific nano"""
        return self.nano_info.get(mac)

    async def register_nano(self, mac: str, ip: str) -> bool:
        """Register a new nano without TCP connection (used by HTTP registration)"""
        try:
            print(f"DEBUG: Registering nano {mac} from {ip}")
            is_new = mac not in self.connected_nanos
            if is_new:
                self.connected_nanos[mac] = NanoConnection(
                    ip=ip,
                    last_seen=datetime.now(),
                    status='registered',
                    reader=None,
                    writer=None
                )
            else:
                # Update existing nano
                self.connected_nanos[mac].ip = ip
                self.connected_nanos[mac].last_seen = datetime.now()
                self.connected_nanos[mac].status = 'registered'
            
            # Add to nano_info if not exists
            if mac not in self.nano_info:
                self.nano_info[mac] = NanoInfo(mac=mac)
                self._save_nano_info()  # Persist the new nano info
            
            # Broadcast registration event via WebSocket
            await self.websocket_manager.broadcast_message({
                "type": "nano_registration",
                "mac": mac,
                "ip": ip,
                "status": "registered",
                "is_new": is_new,
                "timestamp": datetime.now().isoformat()
            })
            
            print(f"Nano registered via HTTP: {mac} from {ip}")
            return True
        except Exception as e:
            print(f"Error registering nano {mac}: {e}")
            return False

    async def handle_client_connected(self, mac: str, ip: str, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        """Handle new TCP client connection"""
        try:
            print(f"Setting up TCP connection for Nano {mac}")
            
            # Create or update nano connection with active TCP connection
            self.connected_nanos[mac] = NanoConnection(
                ip=ip,
                last_seen=datetime.now(),
                status='active',  # Change from 'registered' to 'active'
                reader=reader,
                writer=writer
            )
            print(f"Nano {mac} connected and active")
            
            # Keep connection alive and monitor
            while True:
                try:
                    # Periodically check connection
                    data = await reader.read(1024)

                    if not data:  # Connection closed by client
                        print(f"Client {mac} closed connection")
                        break

                    message = data.decode().strip()
                    print(f"DEBUG: Raw message received: '{message}'")

                    self.connected_nanos[mac].last_seen = datetime.now()
                except Exception as e:
                    print(f"Connection error for {mac}: {e}")
                    break
                await asyncio.sleep(1)  # Reduced polling frequency
                
        except Exception as e:
            print(f"Error handling client {mac}: {e}")
            
        finally:
            # Only clean up if this is still the active connection
            if mac in self.connected_nanos and self.connected_nanos[mac].writer == writer:
                try:
                    writer.close()
                    await writer.wait_closed()
                except:
                    pass
                # Don't remove the nano, just mark it as disconnected
                self.connected_nanos[mac].status = 'registered'
                self.connected_nanos[mac].writer = None
                self.connected_nanos[mac].reader = None
                print(f"Nano {mac} connection closed, marked as registered")

    async def _send_command(self, mac: str, command: bytes, max_retries: int = 3) -> bool:
        """Internal method to send a command to a specific nano with retry logic"""
        nano = self.connected_nanos.get(mac)
        if not nano:
            print(f"X Nano {mac} not found in connected_nanos")
            return False

        retry_count = 0
        while retry_count < max_retries:
            try:
                if not nano.writer:
                    try:
                        reader, writer = await asyncio.open_connection(nano.ip, 9000, timeout=2.0)
                        nano.reader = reader
                        nano.writer = writer
                        print(f"Established new TCP connection to {mac}")
                    except Exception as e:
                        print(f"Nano {mac} appears to be inactive: {e}")
                        # Mark nano as inactive
                        nano.status = 'inactive'
                        nano.writer = None
                        nano.reader = None
                        # Broadcast status change via WebSocket
                        await self.websocket_manager.broadcast_message({
                            "type": "nano_status_changed",
                            "mac": mac,
                            "status": "inactive"
                        })
                        return False

                # Ensure command is bytes
                if isinstance(command, list):
                    command = bytes(command)
                
                nano.writer.write(command)
                await nano.writer.drain()
                nano.last_seen = datetime.now()
                # Update status to active if command was successful
                if nano.status != 'active':
                    nano.status = 'active'
                    await self.websocket_manager.broadcast_message({
                        "type": "nano_status_changed",
                        "mac": mac,
                        "status": "active"
                    })
                print(f"Sent command to {mac}: {[hex(b)[2:].zfill(2) for b in command]}")
                return True

            except Exception as e:
                retry_count += 1
                print(f"Error sending to {mac} (attempt {retry_count}): {str(e)}")
                
                if nano.writer:
                    try:
                        nano.writer.close()
                        await nano.writer.wait_closed()
                    except:
                        pass
                    nano.writer = None
                    nano.reader = None
                
                if retry_count >= max_retries:
                    print(f"Max retries reached for {mac}, marking as inactive")
                    nano.status = 'inactive'
                    # Broadcast status change via WebSocket
                    await self.websocket_manager.broadcast_message({
                        "type": "nano_status_changed",
                        "mac": mac,
                        "status": "inactive"
                    })
                    return False
                
                await asyncio.sleep(0.1)

        return False

    async def broadcast_command(
        self, 
        command: bytes | list[int], 
        target_register: Optional[int] = None,
        target_mac: Optional[str] = None
    ) -> bool:
        """
        Broadcast a command to nanos based on specified criteria
        
        Args:
            command: Command bytes or list of integers to send
            target_register: Optional register to target specific nanos
            target_mac: Optional MAC address to target a specific nano
            
        Returns:
            bool: True if command was sent successfully to at least one nano
        """
        try:
            # Convert command to bytes if needed
            if isinstance(command, list):
                command = bytes(command)

            # Determine target nanos
            if target_mac:
                target_nanos = {target_mac: self.connected_nanos[target_mac]} if target_mac in self.connected_nanos else {}
            elif target_register is not None:
                target_nanos = self.get_nanos_by_register(target_register)
            else:
                target_nanos = self.connected_nanos

            if not target_nanos:
                print(f"No target nanos found for command. Register: {target_register}, MAC: {target_mac}")
                return False

            # Sende alle Kommandos parallel
            tasks = [
                self._send_command(mac_address, command)
                for mac_address in target_nanos
            ]
            results = await asyncio.gather(*tasks, return_exceptions=True)

            # Werte die Ergebnisse aus
            success_count = sum(1 for r in results if r is True)
            disconnected_nanos = [mac for mac, r in zip(target_nanos, results) if r is not True]

            # Entferne ggf. nicht erreichbare Nanos
            for mac_address in disconnected_nanos:
                print(f"Removing disconnected nano: {mac_address}")
                self.connected_nanos.pop(mac_address, None)

            target_type = (
                f"nano {target_mac}" if target_mac 
                else f"register {target_register}" if target_register is not None 
                else "all nanos"
            )
            print(f"Command broadcast to {target_type}: {success_count} successful, {len(disconnected_nanos)} failed")
            
            return success_count > 0

        except Exception as e:
            print(f"Error in broadcast_command: {e}")
            return False

    def get_nanos_by_register(self, register: int) -> Dict[str, NanoConnection]:
        """
        Get all connected nanos belonging to a specific register.
        If register is 1, returns all connected nanos.
        """
        if register == 1:
            return self.connected_nanos.copy()
            
        result = {}
        for mac_address, nano in self.connected_nanos.items():
            nano_info = self.get_nano_info(mac_address)
            # Convert register to int for comparison
            if nano_info and int(nano_info.register) == register:
                result[mac_address] = nano
        return result

    def get_nano_status(self, mac: str) -> str:
        """Get current status of a nano"""
        nano = self.connected_nanos.get(mac)
        return nano.status if nano else 'unknown'
            
    def remove_nano(self, mac: str):
        """Remove a nano from connected devices"""
        if mac in self.connected_nanos:
            del self.connected_nanos[mac]
            
    def get_all_nanos(self) -> Dict[str, dict]:
        """Get all nanos with their additional info, including offline ones"""
        result = {}
        
        # Durch alle bekannten Nanos aus nano_info iterieren
        for mac, info in self.nano_info.items():
            connection = self.connected_nanos.get(mac)

            # Wenn eine Verbindung existiert, dann nutze deren Status,
            # ansonsten ist der Nano "offline".
            if connection:
                status = connection.status
            else:
                status = "offline"

            result[mac] = {
                "ip": connection.ip if connection else None,
                "last_seen": connection.last_seen.isoformat() if connection else None,
                "status": status,
                "name": info.name,
                "color": info.color,
                "gwaendli_color": info.gwaendli_color,
                "instrument": info.instrument,
                "position": asdict(info.position) if info.position else None,
                "register": info.register,
                "led_count": info.led_count
            }

        return result

        
    def get_nano(self, mac: str) -> Optional[NanoConnection]:
        """Get specific nano connection"""
        return self.connected_nanos.get(mac) 

    def check_nano_status(self, mac: str) -> bool:
        """Check if nano is considered active based on last heartbeat"""
        if mac not in self.connected_nanos:
            return False
            
        nano = self.connected_nanos[mac]
        timeout = datetime.now() - timedelta(seconds=35)
        return nano.last_seen > timeout
