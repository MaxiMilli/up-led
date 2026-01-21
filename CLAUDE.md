# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Uzepatscher LED Gwaendli is a distributed LED light show control system for a Guggenmusik (carnival brass band). The system manages LED-equipped Nanos (ESP32 microcontrollers) via a central Hub running on a Raspberry Pi.

**Architecture:**
- **Hub** (`hub/`): Python FastAPI backend + Vue.js/Tailwind frontend running on Raspberry Pi
- **Nano** (`nano/`): C++ firmware for ESP32 microcontrollers with NeoPixel LEDs
- **Gateway** (`gateway/`): ESP32 bridge using ESP-NOW protocol for Hub-Nano communication
- **Simulation** (`simulation/`): Browser-based LED simulation for testing

**Communication Flow:**
```
Hub (Python) → Serial → Gateway (ESP32) → ESP-NOW → Nanos (ESP32)
```

## Build & Run Commands

### Hub (Python)

```bash
cd hub
python -m venv venv
source venv/bin/activate  # Mac/Linux
pip install -r requirements.txt
cp .env.example .env
python -m src.main        # Starts server on http://127.0.0.1:8000
```

API documentation available at http://127.0.0.1:8000/docs (Swagger UI)

### Running Tests

```bash
cd hub
pytest tests/ -v
pytest tests/test_api/test_songs.py -v  # Run specific test file
```

### Nano (C++/PlatformIO)

Uses PlatformIO IDE extension for VS Code. Open `nano/` folder, then use the PlatformIO toolbar to Build → Upload & Monitor. Serial monitor runs at 115200 baud.

## Key Architecture Patterns

### Command Protocol

Commands are 18-byte frames: `[START_BYTE 0xAA][PAYLOAD 16 bytes][CHECKSUM XOR]`

Command categories:
- System (0x00-0x0F): Heartbeat, Ping, Config, Reboot
- State (0x10-0x1F): Off, Standby, Active, Emergency, Blackout
- Effect (0x20-0x3F): Solid, Blink, Chase, Wave, Pulse, Meteor
- Pairing (0x80-0x8F): Pairing requests/acknowledgments

### Nano State Machine

```
Init → ActiveStandby (connected)
ActiveStandby → Standby | Active
Active → Standby | EmergencyStandby (connection lost)
Any → Off (shutdown)
```

### Hub Singletons

Core services use singleton pattern:
- `SerialGateway` - Serial communication to Gateway
- `NanoManager` - Nano registry and pairing
- `SongPlayer` (via player_instance) - Song playback engine
- `WebSocketManager` - Real-time client updates

### Song Playback

TSN files (XML format) are parsed into MIDI timelines. `SongPlayer` processes events tick-by-tick, converting MIDI notes to LED effect commands via `EffectProcessor`.

## Code Style

### Python (Hub)
- Async/await for all I/O operations
- Use `asyncio.Lock()` for thread-safe access to shared resources
- Pydantic models for data validation

### C++ (Nano)
Follows Google C++ Style Guide:
- File names: `snake_case` (e.g., `led_handler.cpp`)
- Type names: `PascalCase` (e.g., `LedHandler`)
- Functions: `PascalCase` (e.g., `HandleActiveStandby`)
- Constants: `kCamelCase` (e.g., `kActiveStandby`)
- Variables: `snake_case` (e.g., `current_state`)

## Deployment

### Raspberry Pi (Hub)
```bash
rsync -avz --exclude '__pycache__' --exclude '.git' --exclude 'venv' \
  ./ pi@192.168.1.195:~/uzepatscher-led-gwaendli/hub/
```

SSH: `hub.local` / user: `uzi` / password: `nanohub`

WiFi: `uzepatscher_lichtshow` / password: `kWalkingLight`
