#pragma once

#include <Arduino.h>

// Button pins - directly mapped to GPIO numbers
// GPIO 2 needs external pull-up resistor (10k to 3.3V) - onboard LED pulls it LOW
// Set to 255 to disable a button slot
// Physical order: Random, Broadcast, Drums, Pauken, Tschinellen, Liras, Trompeten, Posaunen, Bässe, (frei)
constexpr uint8_t kButtonPins[] = {15, 255, 4, 23, 22, 19, 18, 5, 17, 16, 21};
constexpr uint8_t kNumButtons = sizeof(kButtonPins) / sizeof(kButtonPins[0]);

// Onboard LED for status indication
constexpr uint8_t kOnboardLedPin = 2;

// ESP-NOW configuration (must match gateway/nano settings)
constexpr uint8_t kWifiChannel = 11;
constexpr bool kLongRangeEnabled = true;
constexpr int8_t kTxPowerDbm = 20;

// Command frame configuration
constexpr size_t kFrameSize = 16;

// Timing
constexpr uint32_t kDebounceMs = 50;
constexpr uint32_t kStrobeIntervalMs = 250;  // How often to resend strobe while held

// Group bitmasks (same as nano constants.h)
namespace Group
{
    constexpr uint16_t kAll = 0x0001;
    constexpr uint16_t kGroup1 = 0x0002;
    constexpr uint16_t kGroup2 = 0x0004;
    constexpr uint16_t kGroup3 = 0x0008;
    constexpr uint16_t kGroup4 = 0x0010;
    constexpr uint16_t kGroup5 = 0x0020;
    constexpr uint16_t kGroup6 = 0x0040;
    constexpr uint16_t kGroup7 = 0x0080;
    constexpr uint16_t kGroup8 = 0x0100;
    constexpr uint16_t kGroup9 = 0x0200;
    constexpr uint16_t kGroup10 = 0x0400;
    constexpr uint16_t kGroup11 = 0x0800;
    constexpr uint16_t kGroup12 = 0x1000;
    constexpr uint16_t kGroup13 = 0x2000;
    constexpr uint16_t kGroup14 = 0x4000;
    constexpr uint16_t kGroup15 = 0x8000;
    constexpr uint16_t kBroadcast = 0xFFFF;
}

// Instrument groups - clean 1-7 mapping (see PROTOCOL.md)
namespace Instrument
{
    constexpr uint16_t kDrums       = 0x0002;  // Register 1 - bit 1
    constexpr uint16_t kPauken      = 0x0004;  // Register 2 - bit 2
    constexpr uint16_t kTschinellen = 0x0008;  // Register 3 - bit 3
    constexpr uint16_t kLiras       = 0x0010;  // Register 4 - bit 4
    constexpr uint16_t kTrompeten   = 0x0020;  // Register 5 - bit 5
    constexpr uint16_t kPosaunen    = 0x0040;  // Register 6 - bit 6
    constexpr uint16_t kBaesse      = 0x0080;  // Register 7 - bit 7
}

// Command codes (same as nano constants.h)
namespace Cmd
{
    constexpr uint8_t kStateBlackout = 0x14;
    constexpr uint8_t kEffectStrobe = 0x2B;
    constexpr uint8_t kEffectSolid = 0x20;
}

// Flags (lower 4 bits of flags byte)
namespace Flag
{
    constexpr uint8_t kPriority = 0x01;
    constexpr uint8_t kForce = 0x02;
    constexpr uint8_t kNoRebroadcast = 0x08;
}

// TTL configuration (upper 4 bits of flags byte)
constexpr uint8_t kDefaultTTL = 2;  // 2 hops for applausmaschine
constexpr uint8_t kTTLShift = 4;

inline uint8_t MakeFlagsByte(uint8_t ttl, uint8_t flags) {
    return ((ttl << kTTLShift) & 0xF0) | (flags & 0x0F);
}
