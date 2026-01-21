from enum import Enum


class EffectType(Enum):
	"""
	Enumeration of all available LED effects.
	IDs must match nano/include/constants.h and PROTOCOL.md
	"""
	SOLID = 0x20
	BLINK = 0x21
	FADE = 0x22
	RAINBOW = 0x23
	RAINBOW_CYCLE = 0x24
	CHASE = 0x25
	THEATER_CHASE = 0x26
	TWINKLE = 0x27
	SPARKLE = 0x28
	FIRE = 0x29
	PULSE = 0x2A
	STROBE = 0x2B
	GRADIENT = 0x2C
	WAVE = 0x2D
	METEOR = 0x2E
	BREATHING = 0x2F

	@classmethod
	def from_id(cls, effect_id: int) -> 'EffectType':
		"""
		Get EffectType from effect ID.

		@param {int} effect_id - Effect ID (0x20-0x2F)
		@returns {EffectType} Effect type
		@raises {ValueError} If effect ID is unknown
		"""
		for effect in cls:
			if effect.value == effect_id:
				return effect
		raise ValueError(f"Unknown effect ID: 0x{effect_id:02X}")

	@classmethod
	def from_note(cls, note: int) -> 'EffectType':
		"""
		Convert MIDI note to effect type.
		Maps note ranges to specific effects.

		@param {int} note - MIDI note number
		@returns {EffectType} Effect type for the note
		"""
		# Default mapping: notes 30-45 and 100-110 map to SOLID
		# This can be extended for more complex mappings
		if 30 <= note <= 45 or 100 <= note <= 110:
			return cls.SOLID
		else:
			raise ValueError(f"Unknown effect note: {note}")

	@classmethod
	def is_valid_effect_id(cls, effect_id: int) -> bool:
		"""
		Check if an effect ID is valid.

		@param {int} effect_id - Effect ID to check
		@returns {bool} True if valid effect ID
		"""
		return 0x20 <= effect_id <= 0x2F


# Effect ID constants for direct import
EFFECT_SOLID = 0x20
EFFECT_BLINK = 0x21
EFFECT_FADE = 0x22
EFFECT_RAINBOW = 0x23
EFFECT_RAINBOW_CYCLE = 0x24
EFFECT_CHASE = 0x25
EFFECT_THEATER_CHASE = 0x26
EFFECT_TWINKLE = 0x27
EFFECT_SPARKLE = 0x28
EFFECT_FIRE = 0x29
EFFECT_PULSE = 0x2A
EFFECT_STROBE = 0x2B
EFFECT_GRADIENT = 0x2C
EFFECT_WAVE = 0x2D
EFFECT_METEOR = 0x2E
EFFECT_BREATHING = 0x2F
