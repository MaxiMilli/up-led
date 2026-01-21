from dataclasses import dataclass
from typing import List, Optional, Tuple, Dict, Type
from ..nano_network.nano_manager import NanoManager
from .coordinator_instance import coordinator
from .effect_config import ColorEffectConfig, WaveEffectConfig, RandomEffectConfig, ExplosionEffectConfig, ColorRainConfig, ColorPulseConfig
from .effect_types import EffectType
from .effect_coordinator import WaveConfig

@dataclass
class EffectSettings:
    register: List[int] = None
    rgb: List[int] = None
    rainbow: int = 0
    speed: int = 0
    length: int = 0
    intensity: int = 255

    def __post_init__(self):
        if self.rgb is None:
            self.rgb = [0, 0, 0]
        if self.register is None:
            self.register = [1]  # Default to whole band (register 1)

class EffectProcessor:
    def __init__(self):
        self.nano_manager = NanoManager._instance or NanoManager()
        self._effect_handlers = {
            EffectType.NORMAL: self._create_normal_effect,
            EffectType.RANDOM_NANO: self._create_random_nano_effect,
            EffectType.COLOR_EXPLOSION: self._create_color_explosion,
            EffectType.WAVE: self._create_wave_effect,
            EffectType.COLOR_RAIN: self._create_color_rain_effect,
            EffectType.COLOR_PULSE: self._create_color_pulse_effect
        }

    async def process_effect(self, effect_number: int, settings: EffectSettings) -> None:
        """Process an effect with the given note and settings"""
        try:
            effect_type = EffectType.from_note(effect_number)
            print(f"Debug: Effect type for {effect_number}: {effect_type}")
            
            # Ensure register is always a list
            if not settings.register or not isinstance(settings.register, list):
                settings.register = [1]  # Default to whole band (register 1)
            
            print(f"Debug: Settings for effect {effect_number}: {settings}")
            config = self._create_config(effect_type, effect_number, settings)
            print(f"Debug: Created config for effect {effect_number}: {config}")
            await self._effect_handlers[effect_type](config)
        except ValueError as e:
            print(f"Warning: {e}")

    def _create_config(self, effect_type: EffectType, effect_number: int, settings: EffectSettings):
        """Create appropriate config object based on effect type"""
        base_config = {
            'intensity': settings.intensity,
            'target_registers': settings.register,
            'length': settings.length
        }
        
        color_config = {
            'rgb': settings.rgb,
            'rainbow': bool(settings.rainbow),
            'effect_number': effect_number
        }
        
        timed_config = {
            'speed_ms': settings.speed or 100
        }
        
        if effect_type == EffectType.NORMAL:
            return ColorEffectConfig(**base_config, **color_config, **timed_config)
        elif effect_type == EffectType.RANDOM_NANO:
            return RandomEffectConfig(**base_config, **color_config, count=settings.length or 1)
        elif effect_type == EffectType.COLOR_EXPLOSION:
            return ExplosionEffectConfig(**base_config, **color_config, **timed_config)
        elif effect_type == EffectType.WAVE:
            return WaveEffectConfig(**base_config, **color_config, **timed_config)
        elif effect_type == EffectType.COLOR_RAIN:
            return ColorRainConfig(**base_config, **color_config, **timed_config)
        elif effect_type == EffectType.COLOR_PULSE:
            return ColorPulseConfig(**base_config, **color_config, **timed_config)

    async def _create_random_nano_effect(self, config: RandomEffectConfig) -> None:
        command = [
            0x67,  # RGB command
            0, 0,  # Duration
            config.intensity,
            config.rgb[0],
            config.rgb[1],
            config.rgb[2],
            config.rainbow,
            (config.speed_ms >> 8) & 0xFF,
            config.speed_ms & 0xFF,
            config.count
        ]
        
        await coordinator.create_random_nano_effect(config)

    async def _create_color_explosion(self, config: ExplosionEffectConfig) -> None:
        await coordinator.create_color_explosion(config)

    async def _create_color_rain_effect(self, config: ColorRainConfig) -> None:
        await coordinator.create_color_rain(config)

    async def _create_color_pulse_effect(self, config: ColorPulseConfig) -> None:
        await coordinator.create_color_pulse(config)

    async def _create_normal_effect(self, config: ColorEffectConfig) -> None:
        print(f"Debug: Creating normal effect with config: {config}")
        command = [
            config.effect_number,
            0, 0,
            config.intensity,
            config.rgb[0],
            config.rgb[1],
            config.rgb[2],
            config.rainbow,
            (config.speed_ms >> 8) & 0xFF,
            config.speed_ms & 0xFF,
            config.length
        ]

        if config.target_registers:
            for register in config.target_registers:
                await self.nano_manager.broadcast_command(command, target_register=register)
        else:
            await self.nano_manager.broadcast_command(command)

    async def _create_wave_effect(self, config: WaveEffectConfig) -> None:
        """Create a wave effect moving from left to right across multiple nanos"""
        command = bytes([
            33,  # Wellenüberlauf effect (33)
            0x00, 0x00,  # Duration
            config.intensity,
            config.rgb[0],
            config.rgb[1],
            config.rgb[2],
            1 if config.rainbow else 0,
            (config.speed_ms >> 8) & 0xFF,
            config.speed_ms & 0xFF,
            0  # Length not used for wave
        ])
        
        wave_config = WaveConfig(
            command=command,
            speed_ms=config.duration,
            target_registers=config.target_registers
        )
        
        await coordinator.create_wave(wave_config)

# Create a singleton instance
effect_processor = EffectProcessor() 
