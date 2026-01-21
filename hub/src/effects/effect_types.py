from enum import Enum, auto

class EffectType(Enum):
    """Enumeration of all available effects"""
    NORMAL = auto()
    WAVE = auto()
    COLOR_EXPLOSION = auto()
    RANDOM_NANO = auto()
    COLOR_RAIN = auto()
    COLOR_PULSE = auto()
    
    @classmethod
    def from_note(cls, note: int) -> 'EffectType':
        """Convert MIDI note to effect type"""
        if 30 <= note <= 45 or 100 <= note <= 110:
            return cls.NORMAL
        elif note == 50:
            return cls.RANDOM_NANO
        elif note == 51:
            return cls.WAVE
        elif note == 53:
            return cls.COLOR_EXPLOSION
        elif note == 54:
            return cls.COLOR_RAIN
        elif note == 55:
            return cls.COLOR_PULSE
        else:
            raise ValueError(f"Unknown effect note: {note}") 
