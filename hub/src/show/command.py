from dataclasses import dataclass
from typing import Optional
from ..nano_network.serial_gateway import (
	SerialGateway,
	register_to_group_bitmask,
	GROUP_BROADCAST,
	COMMAND_SOLID,
	COMMAND_BLINK,
	COMMAND_STATE_STANDBY,
)


@dataclass
class LEDCommand:
	"""
	LED command with all effect parameters.

	Can be sent via SerialGateway to LED-Nanos.
	"""

	effect: int
	duration: int
	intensity: int
	red: int
	green: int
	blue: int
	rainbow: int
	speed: int
	length: int
	groups: int = GROUP_BROADCAST
	flags: int = 0

	def to_bytes_legacy(self) -> bytes:
		"""
		Legacy 11-byte format for backwards compatibility.

		@returns {bytes} 11-byte command
		"""
		return bytes([
			self.effect,
			(self.duration >> 8) & 0xFF,
			self.duration & 0xFF,
			self.intensity,
			self.red,
			self.green,
			self.blue,
			self.rainbow,
			(self.speed >> 8) & 0xFF,
			self.speed & 0xFF,
			self.length
		])

	def send(self, gateway: Optional[SerialGateway] = None) -> bool:
		"""
		Send this command via the SerialGateway.

		@param {SerialGateway} gateway - Gateway instance (uses singleton if None)
		@returns {bool} True if sent successfully
		"""
		if gateway is None:
			gateway = SerialGateway()

		return gateway.send_command(
			effect=self.effect,
			groups=self.groups,
			flags=self.flags,
			duration=self.duration,
			length=self.length,
			rainbow=self.rainbow,
			r=self.red,
			g=self.green,
			b=self.blue,
			speed=self.speed,
			intensity=self.intensity
		)


class CommandManager:
	"""Creates and sends LED commands."""

	def __init__(self):
		self.gateway = SerialGateway()

	def create_command(self, effect: int, **kwargs) -> LEDCommand:
		"""
		Create a new LED command.

		@param {int} effect - Effect ID
		@param {dict} kwargs - Effect parameters
		@returns {LEDCommand} New command instance
		"""
		register = kwargs.get("register", 1)
		groups = register_to_group_bitmask(register)

		return LEDCommand(
			effect=effect,
			duration=kwargs.get("duration", 0),
			intensity=kwargs.get("intensity", 255),
			red=kwargs.get("red", 0),
			green=kwargs.get("green", 0),
			blue=kwargs.get("blue", 0),
			rainbow=kwargs.get("rainbow", 0),
			speed=kwargs.get("speed", 0),
			length=kwargs.get("length", 0),
			groups=groups,
			flags=kwargs.get("flags", 0)
		)

	def send_command(self, command: LEDCommand) -> bool:
		"""
		Send a command via the gateway.

		@param {LEDCommand} command - Command to send
		@returns {bool} True if sent successfully
		"""
		return command.send(self.gateway)
