from enum import Enum


class EffectType(Enum):
	"""
	Enumeration of all available LED effects.
	IDs must match nano/include/constants.h and PROTOCOL.md
	"""
	SOLID = 0x20
	BLINK = 0x21
	RAINBOW = 0x23
	RAINBOW_CYCLE = 0x24
	CHASE = 0x25
	THEATER_CHASE = 0x26
	TWINKLE = 0x27
	FIRE = 0x29
	PULSE = 0x2A
	GRADIENT = 0x2C
	WAVE = 0x2D
	METEOR = 0x2E
	DNA = 0x30
	BOUNCE = 0x31
	COLOR_WIPE = 0x32
	SCANNER = 0x33
	CONFETTI = 0x34
	LIGHTNING = 0x35
	POLICE = 0x36
	STACKING = 0x37
	MARQUEE = 0x38
	RIPPLE = 0x39
	PLASMA = 0x3A

	@classmethod
	def from_id(cls, effect_id: int) -> 'EffectType':
		"""
		Get EffectType from effect ID.

		@param {int} effect_id - Effect ID
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
		return 0x20 <= effect_id <= 0x3A


# Effect ID constants for direct import
EFFECT_SOLID = 0x20
EFFECT_BLINK = 0x21
EFFECT_RAINBOW = 0x23
EFFECT_RAINBOW_CYCLE = 0x24
EFFECT_CHASE = 0x25
EFFECT_THEATER_CHASE = 0x26
EFFECT_TWINKLE = 0x27
EFFECT_FIRE = 0x29
EFFECT_PULSE = 0x2A
EFFECT_GRADIENT = 0x2C
EFFECT_WAVE = 0x2D
EFFECT_METEOR = 0x2E
EFFECT_DNA = 0x30
EFFECT_BOUNCE = 0x31
EFFECT_COLOR_WIPE = 0x32
EFFECT_SCANNER = 0x33
EFFECT_CONFETTI = 0x34
EFFECT_LIGHTNING = 0x35
EFFECT_POLICE = 0x36
EFFECT_STACKING = 0x37
EFFECT_MARQUEE = 0x38
EFFECT_RIPPLE = 0x39
EFFECT_PLASMA = 0x3A
