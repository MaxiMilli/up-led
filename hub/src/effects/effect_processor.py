from dataclasses import dataclass
from typing import List
from ..nano_network.nano_manager import NanoManager
from .effect_config import ColorEffectConfig
from .effect_types import EffectType


EFFECT_NOTE_TO_COMMAND = {
	30: 0x25,  # Running Light -> CHASE
	31: 0x27,  # Glitter -> TWINKLE
	32: 0x2D,  # Wave -> WAVE
	33: 0x2A,  # Pulsate -> PULSE
	34: 0x2A,  # Fade -> PULSE
	35: 0x21,  # Strobo -> BLINK
	36: 0x27,  # Twinkle -> TWINKLE
	37: 0x2A,  # Heartbeat -> PULSE
	38: 0x2E,  # Meteor -> METEOR
	39: 0x29,  # Flicker -> FIRE
	40: 0x2E,  # Comet -> METEOR
	41: 0x2D,  # Doppler -> WAVE
	42: 0x27,  # Firework -> TWINKLE
	43: 0x30,  # DNA Helix -> DNA
	44: 0x2C,  # Gradient -> GRADIENT
	45: 0x24,  # Rainbow Cycle -> RAINBOW_CYCLE

	100: 0x10,  # Ausschalten -> STATE_OFF
	101: 0x11,  # Standby -> STATE_STANDBY
	102: 0x12,  # Active Standby -> STATE_ACTIVE
	103: 0x20,  # RGB -> SOLID
	104: 0x23,  # Rainbow -> RAINBOW
	105: 0x21,  # Blink -> BLINK
	106: 0x20,  # Single LED -> SOLID
	107: 0x20,  # Additional LED -> SOLID
	108: 0x13,  # Emergency -> STATE_EMERGENCY
	109: 0x14,  # Blackout -> STATE_BLACKOUT
	110: 0x12,  # Initialisieren -> STATE_ACTIVE
}


@dataclass
class EffectSettings:
	register: List[int] = None
	rgb: List[int] = None
	rainbow: int = 0
	speed: int = 0
	length: int = 0
	intensity: int = 255
	duration: int = 0  # Dauer in ms

	def __post_init__(self):
		if self.rgb is None:
			self.rgb = [0, 0, 0]
		if self.register is None:
			self.register = [1]


class EffectProcessor:
	"""Processes effects and sends commands to nanos"""

	def __init__(self):
		self.nano_manager = NanoManager._instance or NanoManager()

	async def process_effect(self, effect_number: int, settings: EffectSettings) -> None:
		"""
		Process an effect with the given note and settings

		@param {int} effect_number - MIDI note number / effect ID
		@param {EffectSettings} settings - Effect configuration
		"""
		try:
			effect_type = EffectType.from_note(effect_number)
			print(f"Debug: Effect type for {effect_number}: {effect_type}")

			if not settings.register or not isinstance(settings.register, list):
				settings.register = [1]

			print(f"Debug: Settings for effect {effect_number}: {settings}")
			config = self._create_config(effect_number, settings)
			print(f"Debug: Created config for effect {effect_number}: {config}")
			await self._create_normal_effect(config)

		except ValueError as e:
			print(f"Warning: {e}")

	def _create_config(self, effect_number: int, settings: EffectSettings) -> ColorEffectConfig:
		"""
		Create config object from settings

		@param {int} effect_number - Effect ID
		@param {EffectSettings} settings - Effect configuration
		@returns {ColorEffectConfig} Configuration object
		"""
		return ColorEffectConfig(
			intensity=settings.intensity,
			target_registers=settings.register,
			length=settings.length,
			rgb=settings.rgb,
			rainbow=bool(settings.rainbow),
			effect_number=effect_number,
			speed_ms=settings.speed or 100,
			duration_ms=settings.duration
		)

	async def _create_normal_effect(self, config: ColorEffectConfig) -> None:
		"""
		Create and broadcast a normal effect

		@param {ColorEffectConfig} config - Effect configuration
		"""
		command_code = EFFECT_NOTE_TO_COMMAND.get(config.effect_number, config.effect_number)
		print(f"Debug: Creating effect - Note {config.effect_number} -> Command 0x{command_code:02X} for registers {config.target_registers}")

		command = [
			command_code,
			(config.duration_ms >> 8) & 0xFF,  # Duration high byte
			config.duration_ms & 0xFF,          # Duration low byte
			config.intensity,
			config.rgb[0],
			config.rgb[1],
			config.rgb[2],
			config.rainbow,
			(config.speed_ms >> 8) & 0xFF,
			config.speed_ms & 0xFF,
			config.length
		]

		# Send all registers in ONE command (combined bitmask)
		if config.target_registers:
			await self.nano_manager.broadcast_command(command, target_registers=config.target_registers)
		else:
			await self.nano_manager.broadcast_command(command)


effect_processor = EffectProcessor()
