"""
Nano Manager Module

Manages LED-Nano information and broadcasts commands via SerialGateway.
Includes pairing workflow for individual nano configuration.
"""

from datetime import datetime
from typing import Dict, List, Optional
from dataclasses import asdict
import asyncio
import json
import os

from ..websocket.websocket_manager import websocket_manager
from .nano_info import NanoInfo, Position, PairingStatus
from .serial_gateway import (
	SerialGateway,
	register_to_group_bitmask,
	GROUP_BROADCAST,
	GROUP_ALL,
	COMMAND_BLINK,
	COMMAND_STATE_STANDBY,
	COMMAND_SET_LED_COUNT,
	COMMAND_SOLID,
	MSG_TYPE_PAIRING,
	MSG_TYPE_CONFIG_ACK,
)


class NanoManager:
	"""
	Singleton manager for LED-Nano devices.

	Stores nano metadata (name, color, register, etc.) and broadcasts
	commands via SerialGateway to Gateway-ESP32.
	Handles pairing workflow for individual nano configuration.
	"""

	_instance = None
	STORAGE_FILE = "data/nano_info.json"

	def __new__(cls):
		if cls._instance is None:
			cls._instance = super(NanoManager, cls).__new__(cls)
			cls._instance.nano_info = {}
			cls._instance.websocket_manager = websocket_manager
			cls._instance.gateway = SerialGateway()
			cls._instance.pairing_mode = False
			cls._instance.pending_pairing_mac = None
			cls._instance._load_nano_info()
			cls._instance.gateway.register_message_callback(cls._instance._handle_gateway_message)
		return cls._instance

	async def _handle_gateway_message(self, msg_type: int, mac: str, data: bytes):
		"""
		Handle incoming messages from Gateway (from Nanos).

		@param {int} msg_type - Message type
		@param {str} mac - Source MAC address
		@param {bytes} data - Message data
		"""
		if msg_type == MSG_TYPE_PAIRING:
			await self._handle_pairing_request(mac)

		elif msg_type == MSG_TYPE_CONFIG_ACK:
			await self._handle_config_ack(mac)

	async def _handle_pairing_request(self, mac: str):
		"""
		Handle pairing request from a Nano.

		@param {str} mac - MAC address of the Nano requesting pairing
		"""
		print(f"Pairing request from: {mac}")

		if not self.pairing_mode:
			print(f"Ignoring pairing request - pairing mode not active")
			return

		self.gateway.send_pairing_ack(mac)

		is_new = mac not in self.nano_info
		if is_new:
			self.nano_info[mac] = NanoInfo(mac=mac, pairing_status=PairingStatus.PAIRING)
		else:
			self.nano_info[mac].pairing_status = PairingStatus.PAIRING

		self.pending_pairing_mac = mac

		await self.websocket_manager.broadcast_message({
			"type": "nano_pairing",
			"mac": mac,
			"is_new": is_new,
			"current_config": self.nano_info[mac].to_dict(),
			"timestamp": datetime.now().isoformat()
		})

	async def _handle_config_ack(self, mac: str):
		"""
		Handle config acknowledgment from a Nano.

		@param {str} mac - MAC address of the Nano
		"""
		print(f"Config ACK from: {mac}")

		if mac in self.nano_info:
			self.nano_info[mac].pairing_status = PairingStatus.CONFIGURED
			self._save_nano_info()

		await self.websocket_manager.broadcast_message({
			"type": "nano_config_ack",
			"mac": mac,
			"timestamp": datetime.now().isoformat()
		})

	def start_pairing_mode(self):
		"""Enable pairing mode to accept pairing requests."""
		self.pairing_mode = True
		self.pending_pairing_mac = None
		print("Pairing mode activated")

	def stop_pairing_mode(self):
		"""Disable pairing mode."""
		self.pairing_mode = False
		self.pending_pairing_mac = None
		print("Pairing mode deactivated")

	async def configure_nano(
		self,
		mac: str,
		register: int,
		led_count: int,
		name: Optional[str] = None,
		standby_r: int = 0,
		standby_g: int = 0,
		standby_b: int = 255
	) -> bool:
		"""
		Send configuration to a specific Nano and update local storage.

		@param {str} mac - Target MAC address
		@param {int} register - Group register (2-15)
		@param {int} led_count - Number of LEDs
		@param {str} name - Optional display name
		@param {int} standby_r - Standby color red (0-255)
		@param {int} standby_g - Standby color green (0-255)
		@param {int} standby_b - Standby color blue (0-255)
		@returns {bool} True if config sent successfully
		"""
		success = self.gateway.send_config_to_nano(
			mac, register, led_count, standby_r, standby_g, standby_b
		)

		if not success:
			print(f"Failed to send config to {mac}")
			return False

		if mac not in self.nano_info:
			self.nano_info[mac] = NanoInfo(mac=mac)

		self.nano_info[mac].register = register
		self.nano_info[mac].led_count = led_count
		if name:
			self.nano_info[mac].name = name

		self._save_nano_info()
		print(f"Config sent to {mac}: register={register}, led_count={led_count}, standby=({standby_r},{standby_g},{standby_b})")

		return True

	def _load_nano_info(self):
		"""Load nano information from storage file."""
		os.makedirs(os.path.dirname(self.STORAGE_FILE), exist_ok=True)
		try:
			if os.path.exists(self.STORAGE_FILE):
				with open(self.STORAGE_FILE, "r") as f:
					data = json.load(f)
					self.nano_info = {
						mac: NanoInfo.from_dict(info) for mac, info in data.items()
					}
		except Exception as e:
			print(f"Error loading nano info: {e}")
			self.nano_info = {}

	def _save_nano_info(self):
		"""Save nano information to storage file."""
		try:
			with open(self.STORAGE_FILE, "w") as f:
				data = {
					mac: info.to_dict() for mac, info in self.nano_info.items()
				}
				json.dump(data, f, indent=2)
		except Exception as e:
			print(f"Error saving nano info: {e}")

	async def update_nano_info(self, mac: str, **updates) -> bool:
		"""
		Update nano information and persist changes.

		@param {str} mac - MAC address of the nano
		@param {dict} updates - Key-value pairs to update
		@returns {bool} True if update successful
		"""
		try:
			if mac not in self.nano_info:
				self.nano_info[mac] = NanoInfo(mac=mac)

			info = self.nano_info[mac]
			updated = False

			if "led_count" in updates:
				led_count = updates["led_count"]
				groups = register_to_group_bitmask(info.register)

				self.gateway.send_command(
					effect=COMMAND_SET_LED_COUNT,
					groups=groups,
					length=led_count
				)
				setattr(info, "led_count", led_count)
				print(f"Updated led_count for {mac}: {led_count}")
				updated = True

			if "gwaendli_color" in updates:
				COLOR_RGB_MAP = {
					"cyan": (0, 255, 255),
					"magenta": (255, 0, 255),
					"yellow": (255, 255, 0)
				}
				rgb = COLOR_RGB_MAP.get(updates["gwaendli_color"])
				if rgb is not None:
					groups = register_to_group_bitmask(info.register)
					self.gateway.send_command(
						effect=COMMAND_SOLID,
						groups=groups,
						r=rgb[0],
						g=rgb[1],
						b=rgb[2],
						duration=1000
					)
					setattr(info, "gwaendli_color", updates["gwaendli_color"])
					print(f"Updated gwaendli_color for {mac}: {updates['gwaendli_color']}")
					updated = True

			for key, value in updates.items():
				if key not in ["led_count", "gwaendli_color"]:
					if key == "position" and value:
						info.position = Position(x=value["x"], y=value["y"])
						updated = True
					elif hasattr(info, key) and getattr(info, key) != value:
						setattr(info, key, value)
						updated = True

			if updated:
				self._save_nano_info()

				await self.websocket_manager.broadcast_message({
					"type": "nano_info_updated",
					"mac": mac,
					"updates": updates
				})

				groups = register_to_group_bitmask(info.register)
				self.gateway.send_command(
					effect=COMMAND_BLINK,
					groups=groups,
					duration=1000,
					r=255,
					g=0,
					b=0,
					speed=100,
					intensity=255
				)

				await asyncio.sleep(1.5)

				self.gateway.send_command(
					effect=COMMAND_STATE_STANDBY,
					groups=groups,
					r=255,
					g=255,
					b=255,
					intensity=64
				)

				return True
			return True

		except Exception as e:
			print(f"Error updating nano info for {mac}: {e}")
			return False

	def get_nano_info(self, mac: str) -> Optional[NanoInfo]:
		"""
		Get stored information for a specific nano.

		@param {str} mac - MAC address
		@returns {NanoInfo | None} Nano info or None if not found
		"""
		return self.nano_info.get(mac)

	async def register_nano(self, mac: str) -> bool:
		"""
		Register a nano in the system.

		Note: With ESP-NOW broadcast, nanos don't maintain persistent connections.
		This just ensures the nano is tracked in nano_info.

		@param {str} mac - MAC address of the nano
		@returns {bool} True if registration successful
		"""
		try:
			is_new = mac not in self.nano_info

			if is_new:
				self.nano_info[mac] = NanoInfo(mac=mac)
				self._save_nano_info()

			await self.websocket_manager.broadcast_message({
				"type": "nano_registration",
				"mac": mac,
				"status": "registered",
				"is_new": is_new,
				"timestamp": datetime.now().isoformat()
			})

			print(f"Nano registered: {mac} (new: {is_new})")
			return True

		except Exception as e:
			print(f"Error registering nano {mac}: {e}")
			return False

	async def broadcast_command(
		self,
		command,
		target_register: Optional[int] = None,
		target_registers: Optional[List[int]] = None,
		target_mac: Optional[str] = None,
		**kwargs
	) -> bool:
		"""
		Broadcast a command to nanos via SerialGateway.

		Supports both legacy bytes format and new keyword argument format.

		Legacy format (bytes or list):
			broadcast_command(bytes([effect, ...]), target_register=1)

		New format (keyword arguments):
			broadcast_command(effect=0x21, red=255, green=0, blue=0)

		Note: target_mac is accepted for backwards compatibility but ignored
		since ESP-NOW broadcasts to groups, not individual MACs. The nano's
		register determines which group it belongs to.

		@param {bytes | list | int} command - Command bytes/list or effect ID
		@param {int} target_register - Target register (1=all, 2-15=specific groups)
		@param {List[int]} target_registers - List of registers (combined into single bitmask)
		@param {str} target_mac - Ignored (kept for backwards compatibility)
		@param {dict} kwargs - Additional parameters for new format
		@returns {bool} True if sent successfully
		"""
		if isinstance(command, (bytes, list)):
			return self._broadcast_legacy_command(command, target_register, target_registers, target_mac)

		return self._broadcast_new_command(
			effect=command,
			target_register=target_register,
			target_registers=target_registers,
			**kwargs
		)

	def _broadcast_legacy_command(
		self,
		command,
		target_register: Optional[int] = None,
		target_registers: Optional[List[int]] = None,
		target_mac: Optional[str] = None
	) -> bool:
		"""
		Broadcast a legacy 11-byte command.

		@param {bytes | list} command - Legacy 11-byte command
		@param {int} target_register - Target register (single)
		@param {List[int]} target_registers - List of registers (combined into bitmask)
		@param {str} target_mac - MAC address (used to look up register)
		@returns {bool} True if sent successfully
		"""
		if isinstance(command, list):
			command = bytes(command)

		if len(command) < 11:
			print(f"Invalid command length: {len(command)}")
			return False

		if target_mac and not target_register:
			nano_info = self.get_nano_info(target_mac)
			if nano_info:
				target_register = nano_info.register

		effect = command[0]
		duration = (command[1] << 8) | command[2]
		intensity = command[3]
		red = command[4]
		green = command[5]
		blue = command[6]
		rainbow = command[7]
		speed = (command[8] << 8) | command[9]
		length = command[10]

		# Combine multiple registers into one bitmask
		if target_registers is not None and len(target_registers) > 0:
			groups = 0
			for reg in target_registers:
				groups |= register_to_group_bitmask(reg)
		elif target_register is not None:
			groups = register_to_group_bitmask(target_register)
		else:
			groups = GROUP_BROADCAST

		return self.gateway.send_command(
			effect=effect,
			groups=groups,
			duration=duration,
			length=length,
			rainbow=rainbow,
			r=red,
			g=green,
			b=blue,
			speed=speed,
			intensity=intensity
		)

	def _broadcast_new_command(
		self,
		effect: int,
		target_register: Optional[int] = None,
		target_registers: Optional[List[int]] = None,
		duration: int = 0,
		intensity: int = 255,
		red: int = 0,
		green: int = 0,
		blue: int = 0,
		rainbow: int = 0,
		speed: int = 0,
		length: int = 0,
		flags: int = 0
	) -> bool:
		"""
		Broadcast a command using new format.

		@param {int} effect - Effect/command ID
		@param {int} target_register - Target register (1=all, 2-15=specific groups)
		@param {List[int]} target_registers - List of registers (combined into bitmask)
		@param {int} duration - Effect duration in ms
		@param {int} intensity - Brightness (0-255)
		@param {int} red - Red value (0-255)
		@param {int} green - Green value (0-255)
		@param {int} blue - Blue value (0-255)
		@param {int} rainbow - Rainbow mode
		@param {int} speed - Animation speed in ms
		@param {int} length - Effect parameter
		@param {int} flags - Command flags
		@returns {bool} True if sent successfully
		"""
		# Combine multiple registers into one bitmask
		if target_registers is not None and len(target_registers) > 0:
			groups = 0
			for reg in target_registers:
				groups |= register_to_group_bitmask(reg)
		elif target_register is not None:
			groups = register_to_group_bitmask(target_register)
		else:
			groups = GROUP_BROADCAST

		return self.gateway.send_command(
			effect=effect,
			groups=groups,
			flags=flags,
			duration=duration,
			length=length,
			rainbow=rainbow,
			r=red,
			g=green,
			b=blue,
			speed=speed,
			intensity=intensity
		)

	def broadcast_command_bytes(
		self,
		command: bytes,
		target_register: Optional[int] = None
	) -> bool:
		"""
		Broadcast a legacy 11-byte command.

		Parses the legacy format and sends via SerialGateway.

		@param {bytes} command - Legacy 11-byte command
		@param {int} target_register - Target register
		@returns {bool} True if sent successfully
		"""
		return self._broadcast_legacy_command(command, target_register)

	def get_nanos_by_register(self, register: int) -> Dict[str, NanoInfo]:
		"""
		Get all nanos belonging to a specific register.

		If register is 1, returns all nanos.

		@param {int} register - Register number (1-15)
		@returns {dict} MAC -> NanoInfo mapping
		"""
		if register == 1:
			return self.nano_info.copy()

		result = {}
		for mac, info in self.nano_info.items():
			if int(info.register) == register:
				result[mac] = info
		return result

	def get_all_nanos(self) -> Dict[str, dict]:
		"""
		Get all known nanos with their info.

		Note: With ESP-NOW broadcast, we can't know actual connection status.
		All nanos are assumed to be reachable if within range.

		@returns {dict} MAC -> nano data mapping
		"""
		result = {}

		for mac, info in self.nano_info.items():
			result[mac] = {
				"mac": mac,
				"status": "broadcast",
				"name": info.name,
				"color": info.color,
				"gwaendli_color": info.gwaendli_color,
				"instrument": info.instrument,
				"position": asdict(info.position) if info.position else None,
				"register": info.register,
				"led_count": info.led_count
			}

		return result

	def remove_nano(self, mac: str):
		"""
		Remove a nano from the system.

		@param {str} mac - MAC address to remove
		"""
		if mac in self.nano_info:
			del self.nano_info[mac]
			self._save_nano_info()
			print(f"Nano removed: {mac}")
