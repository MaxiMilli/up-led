from dataclasses import dataclass
from typing import List, Dict, Optional
import asyncio
import math
from ..nano_network.nano_manager import NanoManager
from ..nano_network.nano_info import NanoInfo
from .effect_config import ColorEffectConfig, RandomEffectConfig
from .effect_utils import create_command_bytes
import random

@dataclass
class WaveConfig:
    command: bytes
    speed_ms: int
    target_registers: Optional[List[int]] = None
    
@dataclass
class ColorRainConfig:
    speed_ms: int
    target_registers: Optional[List[int]] = None

@dataclass
class ColorPulseConfig:
    speed_ms: int
    target_registers: Optional[List[int]] = None

class EffectCoordinator:
    def __init__(self):
        self.nano_manager = NanoManager()
    
    def _get_sorted_nanos_by_position(self, target_registers: Optional[List[int]] = None) -> List[tuple[str, NanoInfo]]:
        """Get nanos sorted by x position, optionally filtered by registers"""
        print(f"\nDEBUG: Getting sorted nanos for registers: {target_registers}")
        nanos = []

        for mac, nano_info in self.nano_manager.nano_info.items():
            print(f"DEBUG: Checking nano {mac} with register {target_registers} and {nano_info.register}")

            if target_registers and 1 in target_registers:
                nanos.append((mac, nano_info))
                print("  - Added to list (all)")
                continue

            print(f"  - Position: {nano_info.position}")
            print(f"  - Register: {nano_info.register}")
            
            if not nano_info.position:
                print("  - Skipped: No position")
                continue

            # Debug-Ausgaben für Typen hinzufügen
            print(f"DEBUG: Types - register: {type(nano_info.register)}, target_registers: {type(target_registers)}")
            print(f"DEBUG: Values - register: {nano_info.register}, target_registers: {target_registers}")
            
            if target_registers is not None and int(nano_info.register) not in target_registers:
                print("  - Skipped: Wrong register")
                continue

            nanos.append((mac, nano_info))
            print("  - Added to list")
        
        sorted_nanos = sorted(nanos, key=lambda x: (x[1].position.x, x[1].position.y))
        print(f"\nDEBUG: Sorted nanos:")
        for mac, info in sorted_nanos:
            print(f"  - Nano {mac}: x={info.position.x}, y={info.position.y}")
        return sorted_nanos

    async def create_wave(self, config: WaveConfig):
        """Create a wave effect from left to right"""
        print("\n=== Starting Wave Effect ===")
        print(f"Wave config: speed_ms={config.speed_ms}, target_registers={config.target_registers}")
        print(f"WaveConfig: command={config.command}, speed_ms={config.speed_ms}, target_registers={config.target_registers}")
        print(f"Command bytes: {[hex(b) for b in config.command]}")
        
        sorted_nanos = self._get_sorted_nanos_by_position(config.target_registers)
        if not sorted_nanos:
            print("ERROR: No positioned nanos found for wave effect")
            return

        # Group nanos by x position
        position_groups: Dict[int, List[tuple[str, NanoInfo]]] = {}
        for mac, nano_info in sorted_nanos:
            x_pos = nano_info.position.x
            if x_pos not in position_groups:
                position_groups[x_pos] = []
            position_groups[x_pos].append((mac, nano_info))
        
        print("\nDEBUG: Position groups:")
        for x_pos, nanos in position_groups.items():
            print(f"  Position x={x_pos}:")
            for mac, info in nanos:
                print(f"    - Nano {mac}")

        # Calculate delays based on x positions
        min_x = min(position_groups.keys())
        max_x = max(position_groups.keys())
        total_distance = max_x - min_x
        print(f"\nDEBUG: Distance calculations:")
        print(f"  - Min x: {min_x}")
        print(f"  - Max x: {max_x}")
        print(f"  - Total distance: {total_distance}")
        
        # Send commands with calculated delays
        for x_pos, nanos in position_groups.items():
            # Calculate delay based on relative position
            relative_position = (x_pos - min_x) / total_distance if total_distance > 0 else 0
            delay = relative_position * config.speed_ms / 1000.0  # Convert to seconds
            
            print(f"\nDEBUG: Sending to position x={x_pos}:")
            print(f"  - Relative position: {relative_position:.3f}")
            print(f"  - Delay: {delay:.3f}s")
            
            # Wait for the calculated delay
            if delay > 0:
                await asyncio.sleep(delay)
            
            # Send commands to all nanos in this position group
            for mac, _ in nanos:
                print(f"  - Sending to nano {mac}")
                await self.nano_manager.broadcast_command(
                    config.command,
                    target_mac=mac
                )

    async def _delayed_command(self, delay: float, mac: str, command: bytes):
        """Send a command to a specific nano with delay"""
        print(f"DEBUG: Executing delayed command for {mac} with {delay:.3f}s delay")
        if delay > 0:
            await asyncio.sleep(delay)
        print(f"DEBUG: Sending command to {mac}: {[hex(b) for b in command]}")
        await self.nano_manager.broadcast_command(command, target_mac=mac)

    async def create_color_explosion(self, config: ColorEffectConfig):
        """Create a color explosion effect where each nano gets a harmonious random color"""
        # Get target nanos for all registers
        nanos = set()
        if config.target_registers:
            for register in config.target_registers:
                nanos.update(self.nano_manager.get_nanos_by_register(register))
        else:
            nanos = set(self.nano_manager.connected_nanos)

        if not nanos:
            print("No nanos found for color explosion effect")
            return

        # Create and send individual commands for each nano
        tasks = []
        for mac in nanos:
            # Generate random RGB values
            r = int(random.triangular(0, 255, 0))
            g = int(random.triangular(0, 255, 0))
            b = int(random.triangular(0, 255, 0))
            
            command = create_command_bytes(
                effect_code=0x67,  # Color explosion effect
                config=config,
                r=r, g=g, b=b  # Pass random RGB values
            )
            tasks.append(
                self.nano_manager.broadcast_command(command, target_mac=mac)
            )

        # Wait for all commands to complete
        if tasks:
            await asyncio.gather(*tasks)
            print(f"Color explosion sent to {len(tasks)} nanos")

    async def create_random_nano_effect(self, config: RandomEffectConfig):
        """Create an effect where only random nanos are active"""
        # Get all eligible nanos from all target registers
        available_nanos = set()
        if config.target_registers:
            for register in config.target_registers:
                available_nanos.update(self.nano_manager.get_nanos_by_register(register))
        else:
            available_nanos = set(self.nano_manager.connected_nanos)

        available_nanos = list(available_nanos)

        if not available_nanos:
            print("No nanos available for random effect")
            return

        # Select random nanos
        count = min(config.count, len(available_nanos))
        selected_nanos = set(random.sample(available_nanos, count))
        
        # Create command for selected and non-selected nanos
        on_command = create_command_bytes(0x67, config)  # RGB command for selected nanos
        off_command = bytes([0x67, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])  # RGB command with all values 0
        
        # Create all tasks at once
        tasks = []
        for mac in available_nanos:
            command = on_command if mac in selected_nanos else off_command
            tasks.append(self.nano_manager.broadcast_command(command, target_mac=mac))

        # Execute all commands in parallel
        if tasks:
            await asyncio.gather(*tasks)
            print(f"Random nano effect sent to {count} nanos")

    async def create_color_rain(self, config: ColorRainConfig):
        """Create a color rain effect where nanos get a random color and change it periodically"""
        # Get target nanos
        nanos = set()
        if config.target_registers:
            for register in config.target_registers:
                nanos.update(self.nano_manager.get_nanos_by_register(register))
        else:
            nanos = set(self.nano_manager.connected_nanos)

        if not nanos:
            print("No nanos found for color rain effect")
            return

        # Generate random RGB values like in color explosion
        r = int(random.triangular(0, 255, 0))
        g = int(random.triangular(0, 255, 0))
        b = int(random.triangular(0, 255, 0))

        # Create command with effect code 30 (running light)
        command = create_command_bytes(
            effect_code=0x30,
            config=config,
            r=r, g=g, b=b,
            speed=config.speed_ms
        )

        # Send command to all nanos
        tasks = []
        for mac in nanos:
            tasks.append(
                self.nano_manager.broadcast_command(command, target_mac=mac)
            )

        if tasks:
            await asyncio.gather(*tasks)
            print(f"Color rain effect sent to {len(tasks)} nanos")

    async def create_color_pulse(self, config: ColorPulseConfig):
        """Create a color pulse effect where nanos get a random color and change it periodically"""
        # Get target nanos
        nanos = set()
        if config.target_registers:
            for register in config.target_registers:
                nanos.update(self.nano_manager.get_nanos_by_register(register))
        else:
            nanos = set(self.nano_manager.connected_nanos)

        if not nanos:
            print("No nanos found for color pulse effect")
            return

        # Create command with effect code 33 (pulse)
        command = create_command_bytes(
            effect_code=0x33,
            config=config,
            speed=config.speed_ms
        )

        # Send command to all nanos
        tasks = []
        for mac in nanos:
            tasks.append(
                self.nano_manager.broadcast_command(command, target_mac=mac)
            )

        if tasks:
            await asyncio.gather(*tasks)
            print(f"Color pulse effect sent to {len(tasks)} nanos")

    
