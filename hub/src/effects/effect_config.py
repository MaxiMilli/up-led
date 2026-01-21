from dataclasses import dataclass
from typing import List, Optional, Tuple

@dataclass
class BaseEffectConfig:
    """Base configuration for all effects"""
    intensity: int = 255
    target_registers: Optional[List[int]] = None
    length: int = 4

@dataclass
class TimedEffectConfig(BaseEffectConfig):
    """Configuration for time-based effects"""
    speed_ms: int = 100
    duration_ms: Optional[int] = None

@dataclass
class ColorEffectConfig(TimedEffectConfig, BaseEffectConfig):
    """Configuration for color-based effects"""
    rgb: List[int] = None
    rainbow: bool = False
    effect_number: int = 0 
    
    def __post_init__(self):
        self.rgb = self.rgb or [0, 0, 0]

@dataclass
class WaveEffectConfig(ColorEffectConfig):
    """Configuration for wave effects"""
    duration: int = 100

@dataclass
class RandomEffectConfig(ColorEffectConfig):
    """Configuration for random nano effects"""
    count: int = 1

@dataclass
class ExplosionEffectConfig(ColorEffectConfig):
    """Configuration for explosion effects"""
    base_color: Optional[Tuple[int, int, int]] = None 

@dataclass
class ColorRainConfig(ColorEffectConfig):
    """Configuration for color rain effects"""
    speed: int = 100
    count: int = 1

@dataclass
class ColorPulseConfig(ColorEffectConfig):
    """Configuration for color pulse effects"""
    speed: int = 100
    count: int = 1

