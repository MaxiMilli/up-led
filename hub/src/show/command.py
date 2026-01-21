from dataclasses import dataclass
from typing import Optional

@dataclass
class LEDCommand:
    effect: int
    duration: int
    intensity: int
    red: int
    green: int
    blue: int
    rainbow: int
    speed: int
    length: int

    def to_bytes(self) -> bytes:
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

class CommandManager:
    def __init__(self):
        self.tcp_server = None  # Will be set from main.py
    
    def create_command(self, effect: int, **kwargs) -> LEDCommand:
        return LEDCommand(
            effect=effect,
            duration=kwargs.get('duration', 0),
            intensity=kwargs.get('intensity', 255),
            red=kwargs.get('red', 0),
            green=kwargs.get('green', 0),
            blue=kwargs.get('blue', 0),
            rainbow=kwargs.get('rainbow', 0),
            speed=kwargs.get('speed', 0),
            length=kwargs.get('length', 0)
        )
