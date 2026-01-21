"""
Serial Gateway Module

Kommuniziert über USB-Serial mit einem Gateway-ESP32, der Commands
per ESP-NOW Broadcast an LED-Nanos weiterleitet.

Frame Format (18 Bytes):
┌────────┬──────────────────────────────────────────────────────────────┬──────────┐
│ START  │                    PAYLOAD (16 Bytes)                        │ CHECKSUM │
│  0xAA  │                                                              │  (XOR)   │
└────────┴──────────────────────────────────────────────────────────────┴──────────┘
"""

import asyncio
import serial
import os
from typing import Optional
from ..config import settings

START_BYTE = 0xAA
START_BYTE_UPSTREAM = 0xBB
FRAME_SIZE = 18
UPSTREAM_FRAME_SIZE_PAIRING = 9
UPSTREAM_FRAME_SIZE_CONFIG_ACK = 10
PAYLOAD_SIZE = 16

COMMAND_NOP = 0x00
COMMAND_HEARTBEAT = 0x01
COMMAND_PING = 0x02
COMMAND_IDENTIFY = 0x03
COMMAND_SET_LED_COUNT = 0x04
COMMAND_SET_GROUPS = 0x05
COMMAND_SAVE_CONFIG = 0x06
COMMAND_REBOOT = 0x07
COMMAND_FACTORY_RESET = 0x0A
COMMAND_SET_MESH_TTL = 0x0B

COMMAND_STATE_OFF = 0x10
COMMAND_STATE_STANDBY = 0x11
COMMAND_STATE_ACTIVE = 0x12
COMMAND_STATE_EMERGENCY = 0x13
COMMAND_STATE_BLACKOUT = 0x14

COMMAND_SOLID = 0x20
COMMAND_BLINK = 0x21
COMMAND_FADE = 0x22
COMMAND_RAINBOW = 0x23
COMMAND_RAINBOW_CYCLE = 0x24
COMMAND_CHASE = 0x25
COMMAND_THEATER_CHASE = 0x26
COMMAND_TWINKLE = 0x27
COMMAND_SPARKLE = 0x28
COMMAND_FIRE = 0x29
COMMAND_PULSE = 0x2A
COMMAND_STROBE = 0x2B
COMMAND_GRADIENT = 0x2C
COMMAND_WAVE = 0x2D
COMMAND_METEOR = 0x2E
COMMAND_BREATHING = 0x2F

COMMAND_DEBUG_ECHO = 0xF0
COMMAND_DEBUG_INFO = 0xF1
COMMAND_DEBUG_STRESS = 0xF2

COMMAND_PAIRING_REQUEST = 0xA0
COMMAND_PAIRING_ACK = 0x81
COMMAND_CONFIG_SET = 0x82
COMMAND_CONFIG_ACK = 0x83

MSG_TYPE_PAIRING = 0x01
MSG_TYPE_CONFIG_ACK = 0x02

GROUP_ALL = 0x0001
GROUP_BROADCAST = 0xFFFF

FLAG_PRIORITY = 0x01
FLAG_FORCE = 0x02
FLAG_SYNC = 0x04
FLAG_NO_REBROADCAST = 0x08

# TTL configuration (upper 4 bits of flags byte)
DEFAULT_TTL = 2  # 2 hops from hub
TTL_SHIFT = 4
TTL_MASK = 0xF0
FLAGS_MASK = 0x0F

def make_flags_byte(ttl: int, flags: int) -> int:
    """Combine TTL (upper 4 bits) and flags (lower 4 bits) into flags byte."""
    return ((ttl << TTL_SHIFT) & TTL_MASK) | (flags & FLAGS_MASK)

# CRC-8 lookup table (polynomial 0x07, init 0x00)
CRC8_TABLE = [
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, 0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
]

def calculate_crc8(data: bytes) -> int:
    """Calculate CRC-8 checksum."""
    crc = 0x00
    for byte in data:
        crc = CRC8_TABLE[crc ^ byte]
    return crc


class SerialGateway:
	"""
	Manages serial communication with the Gateway-ESP32.

	Handles device discovery, frame building, and automatic heartbeat.
	"""

	_instance: Optional["SerialGateway"] = None

	def __new__(cls):
		if cls._instance is None:
			cls._instance = super().__new__(cls)
			cls._instance._initialized = False
		return cls._instance

	def __init__(self):
		if self._initialized:
			return

		self._initialized = True
		self._serial: Optional[serial.Serial] = None
		self._seq_counter = 0
		self._heartbeat_task: Optional[asyncio.Task] = None
		self._read_task: Optional[asyncio.Task] = None
		self._connected = False
		self._message_callbacks = []

	@property
	def is_connected(self) -> bool:
		return self._connected and self._serial is not None and self._serial.is_open

	def _discover_device(self) -> Optional[str]:
		"""
		Discover available serial device.

		@returns {str | None} Path to the first available serial device, or None
		"""
		for port in settings.SERIAL_PORTS:
			if os.path.exists(port):
				print(f"Serial device found: {port}")
				return port

		print(f"No serial device found. Checked: {settings.SERIAL_PORTS}")
		return None

	def connect(self) -> bool:
		"""
		Connect to the Gateway-ESP32 via serial.

		@returns {bool} True if connection successful
		"""
		if self.is_connected:
			return True

		device = self._discover_device()
		if not device:
			return False

		try:
			self._serial = serial.Serial(
				port=device,
				baudrate=settings.SERIAL_BAUD,
				bytesize=serial.EIGHTBITS,
				parity=serial.PARITY_NONE,
				stopbits=serial.STOPBITS_ONE,
				timeout=1.0
			)
			self._connected = True
			print(f"Connected to Gateway on {device} @ {settings.SERIAL_BAUD} baud")
			return True

		except serial.SerialException as e:
			print(f"Failed to connect to {device}: {e}")
			self._serial = None
			self._connected = False
			return False

	def disconnect(self):
		"""Close serial connection and stop tasks."""
		if self._heartbeat_task:
			self._heartbeat_task.cancel()
			self._heartbeat_task = None

		if self._read_task:
			self._read_task.cancel()
			self._read_task = None

		self._cleanup_connection()
		print("Serial gateway disconnected")

	def register_message_callback(self, callback):
		"""
		Register callback for incoming messages from Gateway.

		@param {callable} callback - async function(msg_type, mac, data)
		"""
		self._message_callbacks.append(callback)

	def _parse_mac(self, data: bytes) -> str:
		"""
		Parse 6-byte MAC address to string format.

		@param {bytes} data - 6 bytes MAC
		@returns {str} MAC in format "XX:XX:XX:XX:XX:XX"
		"""
		return ":".join(f"{b:02X}" for b in data)

	async def _process_incoming_message(self, frame: bytes):
		"""
		Process incoming frame from Gateway.

		Frame formats:
		- Pairing (0x01): [0xBB][TYPE][MAC 6 bytes][CHECKSUM] = 9 bytes
		- Config ACK (0x02): [0xBB][TYPE][MAC 6 bytes][STATUS][CHECKSUM] = 10 bytes

		@param {bytes} frame - Incoming frame (9 or 10 bytes)
		"""
		if len(frame) < UPSTREAM_FRAME_SIZE_PAIRING:
			return

		msg_type = frame[1]

		if msg_type == MSG_TYPE_CONFIG_ACK:
			mac = self._parse_mac(frame[2:8])
			status = frame[8]
			checksum = frame[9]
			checksum_data = frame[1:9]
		else:
			mac = self._parse_mac(frame[2:8])
			status = 0
			checksum = frame[8]
			checksum_data = frame[1:8]

		calculated_checksum = calculate_crc8(checksum_data)

		if checksum != calculated_checksum:
			print(f"Checksum mismatch: expected {calculated_checksum:02X}, got {checksum:02X}")
			return

		if settings.DEBUG:
			hex_frame = " ".join(f"{b:02X}" for b in frame)
			print(f"RX: {hex_frame} | Type: {msg_type:02X} | MAC: {mac}")

		for callback in self._message_callbacks:
			try:
				await callback(msg_type, mac, bytes([status]))
			except Exception as e:
				print(f"Callback error: {e}")

	async def _read_loop(self):
		"""Background task to read incoming messages from Gateway."""
		buffer = bytearray()

		while True:
			try:
				if not self.is_connected:
					await asyncio.sleep(1)
					continue

				if self._serial.in_waiting > 0:
					chunk = self._serial.read(self._serial.in_waiting)
					buffer.extend(chunk)

					if settings.DEBUG and len(chunk) > 0:
						hex_chunk = " ".join(f"{b:02X}" for b in chunk)
						print(f"Serial RX raw: {hex_chunk}")

					while len(buffer) >= UPSTREAM_FRAME_SIZE_PAIRING:
						if buffer[0] != START_BYTE_UPSTREAM:
							buffer.pop(0)
							continue

						msg_type = buffer[1] if len(buffer) > 1 else 0

						if msg_type == MSG_TYPE_CONFIG_ACK:
							frame_size = UPSTREAM_FRAME_SIZE_CONFIG_ACK
						else:
							frame_size = UPSTREAM_FRAME_SIZE_PAIRING

						if len(buffer) < frame_size:
							break

						frame = bytes(buffer[:frame_size])
						buffer = buffer[frame_size:]
						await self._process_incoming_message(frame)
				else:
					await asyncio.sleep(0.01)

			except asyncio.CancelledError:
				break
			except Exception as e:
				print(f"Read loop error: {e}")
				await asyncio.sleep(1)

	async def start_read_loop(self):
		"""Start async loop for reading incoming messages."""
		if self._read_task:
			return

		self._read_task = asyncio.create_task(self._read_loop())
		print("Serial read loop started")

	def _next_seq(self) -> int:
		"""
		Get next sequence number (0-65535 with wraparound).

		@returns {int} Next sequence number
		"""
		seq = self._seq_counter
		self._seq_counter = (self._seq_counter + 1) & 0xFFFF
		return seq

	def _calculate_checksum(self, payload: bytes) -> int:
		"""
		Calculate CRC-8 checksum of payload.

		@param {bytes} payload - 16-byte payload
		@returns {int} CRC-8 checksum
		"""
		return calculate_crc8(payload)

	def build_payload(
		self,
		seq: int,
		flags: int,
		effect: int,
		groups: int,
		duration: int = 0,
		length: int = 0,
		rainbow: int = 0,
		r: int = 0,
		g: int = 0,
		b: int = 0,
		speed: int = 0,
		intensity: int = 255,
		ttl: int = DEFAULT_TTL
	) -> bytes:
		"""
		Build 16-byte payload for serial transmission.

		@param {int} seq - Sequence number (0-65535)
		@param {int} flags - Command flags (lower 4 bits)
		@param {int} effect - Effect/command ID
		@param {int} groups - Target groups bitmask
		@param {int} duration - Effect duration in ms
		@param {int} length - Effect parameter (e.g., chase length)
		@param {int} rainbow - Rainbow mode (0=off, 1=on)
		@param {int} r - Red value (0-255)
		@param {int} g - Green value (0-255)
		@param {int} b - Blue value (0-255)
		@param {int} speed - Animation speed in ms
		@param {int} intensity - Brightness (0-255)
		@param {int} ttl - Time-to-live for mesh rebroadcast (upper 4 bits of flags byte)
		@returns {bytes} 16-byte payload
		"""
		payload = bytearray(PAYLOAD_SIZE)

		payload[0] = (seq >> 8) & 0xFF
		payload[1] = seq & 0xFF
		payload[2] = make_flags_byte(ttl, flags)
		payload[3] = effect & 0xFF
		payload[4] = (groups >> 8) & 0xFF
		payload[5] = groups & 0xFF
		payload[6] = (duration >> 8) & 0xFF
		payload[7] = duration & 0xFF
		payload[8] = length & 0xFF
		payload[9] = rainbow & 0xFF
		payload[10] = r & 0xFF
		payload[11] = g & 0xFF
		payload[12] = b & 0xFF
		payload[13] = (speed >> 8) & 0xFF
		payload[14] = speed & 0xFF
		payload[15] = intensity & 0xFF

		return bytes(payload)

	def build_frame(self, payload: bytes) -> bytes:
		"""
		Build complete 18-byte frame with START byte and checksum.

		@param {bytes} payload - 16-byte payload
		@returns {bytes} 18-byte frame ready for transmission
		"""
		if len(payload) != PAYLOAD_SIZE:
			raise ValueError(f"Payload must be {PAYLOAD_SIZE} bytes, got {len(payload)}")

		frame = bytearray(FRAME_SIZE)
		frame[0] = START_BYTE
		frame[1:17] = payload
		frame[17] = self._calculate_checksum(payload)

		return bytes(frame)

	def _cleanup_connection(self):
		"""Clean up serial connection after error."""
		if self._serial:
			try:
				self._serial.close()
			except Exception:
				pass
		self._serial = None
		self._connected = False

	def send_frame(self, frame: bytes) -> bool:
		"""
		Send frame over serial connection.

		@param {bytes} frame - 18-byte frame to send
		@returns {bool} True if sent successfully
		"""
		if not self.is_connected:
			if not self.connect():
				return False

		try:
			self._serial.write(frame)
			self._serial.flush()
			return True

		except (serial.SerialException, OSError) as e:
			print(f"Serial write error: {e}")
			self._cleanup_connection()
			if self.connect():
				try:
					self._serial.write(frame)
					self._serial.flush()
					print("Reconnected to serial device")
					return True
				except Exception:
					self._cleanup_connection()
			return False

	def send_command(
		self,
		effect: int,
		groups: int = GROUP_BROADCAST,
		flags: int = 0,
		duration: int = 0,
		length: int = 0,
		rainbow: int = 0,
		r: int = 0,
		g: int = 0,
		b: int = 0,
		speed: int = 0,
		intensity: int = 255,
		use_new_seq: bool = True
	) -> bool:
		"""
		Build and send a command to the Gateway.

		@param {int} effect - Effect/command ID
		@param {int} groups - Target groups bitmask
		@param {int} flags - Command flags
		@param {int} duration - Effect duration in ms
		@param {int} length - Effect parameter
		@param {int} rainbow - Rainbow mode
		@param {int} r - Red value
		@param {int} g - Green value
		@param {int} b - Blue value
		@param {int} speed - Animation speed in ms
		@param {int} intensity - Brightness
		@param {bool} use_new_seq - Whether to increment sequence counter
		@returns {bool} True if sent successfully
		"""
		seq = self._next_seq() if use_new_seq else self._seq_counter

		payload = self.build_payload(
			seq=seq,
			flags=flags,
			effect=effect,
			groups=groups,
			duration=duration,
			length=length,
			rainbow=rainbow,
			r=r,
			g=g,
			b=b,
			speed=speed,
			intensity=intensity
		)

		frame = self.build_frame(payload)
		success = self.send_frame(frame)

		if success and settings.DEBUG:
			hex_frame = " ".join(f"{b:02X}" for b in frame)
			print(f"TX: {hex_frame}")

		return success

	def send_heartbeat(self) -> bool:
		"""
		Send heartbeat command to all nanos.

		@returns {bool} True if sent successfully
		"""
		return self.send_command(
			effect=COMMAND_HEARTBEAT,
			groups=GROUP_BROADCAST,
			flags=0
		)

	async def start_heartbeat_loop(self):
		"""Start async heartbeat loop (every 5 seconds)."""
		if self._heartbeat_task:
			return

		async def heartbeat_loop():
			interval = settings.HEARTBEAT_INTERVAL_MS / 1000.0
			while True:
				try:
					self.send_heartbeat()
					await asyncio.sleep(interval)
				except asyncio.CancelledError:
					break
				except Exception as e:
					print(f"Heartbeat error: {e}")
					await asyncio.sleep(interval)

		self._heartbeat_task = asyncio.create_task(heartbeat_loop())
		print(f"Heartbeat loop started (interval: {settings.HEARTBEAT_INTERVAL_MS}ms)")

	def stop_heartbeat_loop(self):
		"""Stop the heartbeat loop."""
		if self._heartbeat_task:
			self._heartbeat_task.cancel()
			self._heartbeat_task = None
			print("Heartbeat loop stopped")

	def _mac_to_bytes(self, mac: str) -> bytes:
		"""
		Convert MAC string to 6 bytes.

		@param {str} mac - MAC in format "XX:XX:XX:XX:XX:XX"
		@returns {bytes} 6-byte MAC
		"""
		return bytes(int(b, 16) for b in mac.split(":"))

	def send_config_to_nano(
		self,
		mac: str,
		register: int,
		led_count: int,
		standby_r: int = 0,
		standby_g: int = 0,
		standby_b: int = 255
	) -> bool:
		"""
		Send configuration to a specific Nano using standard 18-byte frame.

		MAC is encoded in payload fields: r,g,b = MAC[0:3], speed = MAC[3:5], intensity = MAC[5]
		Register in 'length' field, LED count in 'duration' field.
		Standby color encoded in: flags = standby_b, groups = (standby_r << 8) | standby_g

		@param {str} mac - Target MAC address
		@param {int} register - Group register (2-15)
		@param {int} led_count - Number of LEDs
		@param {int} standby_r - Standby color red (0-255)
		@param {int} standby_g - Standby color green (0-255)
		@param {int} standby_b - Standby color blue (0-255)
		@returns {bool} True if sent successfully
		"""
		mac_bytes = self._mac_to_bytes(mac)

		return self.send_command(
			effect=COMMAND_CONFIG_SET,
			flags=standby_b & 0xFF,
			groups=((standby_r & 0xFF) << 8) | (standby_g & 0xFF),
			duration=led_count,
			length=register,
			r=mac_bytes[0],
			g=mac_bytes[1],
			b=mac_bytes[2],
			speed=(mac_bytes[3] << 8) | mac_bytes[4],
			intensity=mac_bytes[5]
		)

	def send_pairing_ack(self, mac: str) -> bool:
		"""
		Send pairing acknowledgment to a specific Nano using standard 18-byte frame.

		@param {str} mac - Target MAC address
		@returns {bool} True if sent successfully
		"""
		mac_bytes = self._mac_to_bytes(mac)

		return self.send_command(
			effect=COMMAND_PAIRING_ACK,
			groups=GROUP_BROADCAST,
			r=mac_bytes[0],
			g=mac_bytes[1],
			b=mac_bytes[2],
			speed=(mac_bytes[3] << 8) | mac_bytes[4],
			intensity=mac_bytes[5]
		)


def register_to_group_bitmask(register: int) -> int:
	"""
	Convert register number to group bitmask.

	Unified instrument mapping (see PROTOCOL.md):
	Register 1 = Drums       (0x0002)
	Register 2 = Pauken      (0x0004)
	Register 3 = Tschinellen (0x0008)
	Register 4 = Liras       (0x0010)
	Register 5 = Trompeten   (0x0020)
	Register 6 = Posaunen    (0x0040)
	Register 7 = Baesse      (0x0080)

	@param {int} register - Register number (1-7)
	@returns {int} Group bitmask
	"""
	if register <= 0 or register > 7:
		return GROUP_BROADCAST

	# Register N maps to bit N: 1 << 1 = 0x0002, 1 << 2 = 0x0004, etc.
	return 1 << register


# Instrument group constants for reference (matching PROTOCOL.md)
INSTRUMENT_DRUMS       = 0x0002  # Register 1
INSTRUMENT_PAUKEN      = 0x0004  # Register 2
INSTRUMENT_TSCHINELLEN = 0x0008  # Register 3
INSTRUMENT_LIRAS       = 0x0010  # Register 4
INSTRUMENT_TROMPETEN   = 0x0020  # Register 5
INSTRUMENT_POSAUNEN    = 0x0040  # Register 6
INSTRUMENT_BAESSE      = 0x0080  # Register 7

INSTRUMENT_NAMES = {
	1: "Drums",
	2: "Pauken",
	3: "Tschinellen",
	4: "Liras",
	5: "Trompeten",
	6: "Posaunen",
	7: "Baesse",
}
