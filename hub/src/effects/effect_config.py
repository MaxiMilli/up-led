from dataclasses import dataclass
from typing import List, Optional


@dataclass
class BaseEffectConfig:
	"""Base configuration for all effects"""
	intensity: int = 255
	target_registers: Optional[List[int]] = None
	length: int = 4


@dataclass
class ColorEffectConfig(BaseEffectConfig):
	"""Configuration for color-based effects"""
	rgb: List[int] = None
	rainbow: bool = False
	effect_number: int = 0
	speed_ms: int = 100
	duration_ms: int = 0

	def __post_init__(self):
		self.rgb = self.rgb or [0, 0, 0]

