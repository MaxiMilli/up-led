# Applausmaschine

ESP32-basierter Button-Controller für die LED-Show. Sendet bei Tasterdruck roten Strobe-Effekt direkt via ESP-NOW an die Nanos.

## Hardware

11 Taster an folgenden GPIO-Pins:

| Button | GPIO | Default Register |
|--------|------|-----------------|
| 0      | 15   | Broadcast (alle) |
| 1      | 2    | Register 1      |
| 2      | 4    | Register 2      |
| 3      | 16   | Register 3      |
| 4      | 17   | Register 4      |
| 5      | 5    | Register 5      |
| 6      | 18   | Register 6      |
| 7      | 19   | Register 7      |
| 8      | 21   | Register 8      |
| 9      | 22   | Register 9      |
| 10     | 23   | Register 10     |

**Anschluss:** Taster zwischen GPIO und GND (INPUT_PULLUP wird verwendet).

**Hinweis:** GPIO 2 und GPIO 15 sind Strapping Pins, funktionieren aber nach dem Boot.

## Konfiguration

Die Zuordnung Button → Register kann in `include/config.h` angepasst werden:

```cpp
constexpr uint16_t kButtonGroups[kNumButtons] = {
    Group::kBroadcast,  // Button 0 (GPIO 15) -> Alle Register
    Group::kGroup1,     // Button 1 (GPIO 2)  -> Register 1
    Group::kGroup2,     // Button 2 (GPIO 4)  -> Register 2
    // ...
};
```

Mehrere Register gleichzeitig ansprechen:
```cpp
Group::kGroup1 | Group::kGroup2  // Register 1 und 2
```

## Strobe-Einstellungen

In `include/config.h`:

```cpp
// Farbe (RGB)
constexpr uint8_t kStrobeR = 255;  // Rot
constexpr uint8_t kStrobeG = 0;    // Grün
constexpr uint8_t kStrobeB = 0;    // Blau

// Geschwindigkeit (niedriger = schneller)
constexpr uint16_t kStrobeSpeed = 100;

// Intensität (0-255)
constexpr uint8_t kStrobeIntensity = 255;
```

## Funktionsweise

1. **Taster gedrückt:** Sendet Strobe-Kommando an konfigurierte Register
2. **Taster gehalten:** Strobe wird alle 100ms erneut gesendet
3. **Taster losgelassen:** Blackout an alle Nanos

## Build & Upload

Mit PlatformIO in VS Code:

1. Ordner `applausmaschine` in PlatformIO öffnen
2. Build → Upload & Monitor

## Serial Output

```
[APPLAUS] Applausmaschine starting...
[APPLAUS] Button 0 on GPIO 15 -> groups=0xFFFF (init=released)
[APPLAUS] Button 1 on GPIO 2 -> groups=0x0002 (init=released)
...
[DEBUG] Pins: G15=1 G2=1 G4=1 G16=1 ...
[APPLAUS] Button 3 PRESSED (GPIO 16)
[APPLAUS] STROBE -> groups=0x0008
[APPLAUS] Button 3 RELEASED (GPIO 16)
[APPLAUS] BLACKOUT -> all (1/4)
[APPLAUS] BLACKOUT -> all (2/4)
[APPLAUS] BLACKOUT -> all (3/4)
[APPLAUS] BLACKOUT -> all (4/4)
```
