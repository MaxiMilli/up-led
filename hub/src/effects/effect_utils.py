from typing import List, Optional
import random
import colorsys

def create_wave_command(
    original_command: List[int],
    speed_ms: int,
) -> List[int]:
    """Modify a command for wave effect"""
    command = original_command.copy()
    # Set speed for wave effect
    command[8] = (speed_ms >> 8) & 0xFF
    command[9] = speed_ms & 0xFF
    return command 

def create_command_bytes(effect_code: int, config: 'BaseEffectConfig', r: int = None, g: int = None, b: int = None) -> bytes:
    """Create command bytes from effect code and config"""
    command = [
        effect_code,
        0, 0,  # Duration (unused)
        config.intensity
    ]
    
    if hasattr(config, 'rgb'):
        # Use provided RGB values if available, otherwise use config.rgb
        if r is not None and g is not None and b is not None:
            command.extend([r, g, b])
        else:
            command.extend(config.rgb)
        command.append(1 if config.rainbow else 0)
    else:
        command.extend([0, 0, 0, 0])
        
    # Verwende speed statt speed_ms
    speed = getattr(config, 'speed_ms', getattr(config, 'speed', 0))
    command.extend([
        (speed >> 8) & 0xFF,
        speed & 0xFF
    ])
        
    if hasattr(config, 'count'):
        command.append(config.count)
    else:
        command.append(0)
        
    return bytes(command)

def generate_harmonious_color(base_color=None):
    """Generate harmonious colors utility"""
    # ... (existing color generation logic) ...
