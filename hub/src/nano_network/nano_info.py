from dataclasses import dataclass
from typing import Optional

@dataclass
class Position:
    x: int
    y: int

@dataclass

class NanoInfo:
    mac: str
    name: Optional[str] = None
    color: Optional[str] = None
    instrument: Optional[str] = None
    position: Optional[Position] = None
    gwaendli_color: Optional[str] = None
    register: Optional[int] = 0
    led_count: Optional[int] = 0
    
    def to_dict(self):
        result = {
            "mac": self.mac,
            "name": self.name,
            "color": self.color,
            "instrument": self.instrument,
            "gwaendli_color": self.gwaendli_color,
            "register": self.register,
            "led_count": self.led_count
        }
        if self.position:
            result["position"] = {"x": self.position.x, "y": self.position.y}
        return result

    @classmethod
    def from_dict(cls, data: dict):
        position_data = data.get('position')
        position = Position(**position_data) if position_data else None
        return cls(
            mac=data['mac'],
            name=data.get('name'),
            color=data.get('color'),
            instrument=data.get('instrument'),
            position=position,
            gwaendli_color=data.get('gwaendli_color'),
            register=data.get('register'),
            led_count=data.get('led_count')
        ) 
