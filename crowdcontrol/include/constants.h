#pragma once

#include <Arduino.h>

// Button pins - same as applausmaschine
// GPIO 2 needs external pull-up resistor (10k to 3.3V) - onboard LED pulls it LOW
// Set to 255 to disable a button slot
constexpr uint8_t kButtonPins[] = {15, 255, 4, 23, 22, 19, 18, 5, 17, 16, 21};
constexpr uint8_t kNumButtons = sizeof(kButtonPins) / sizeof(kButtonPins[0]);

// Button index mapping:
// Index 0:  GPIO 15 -> (nicht verwendet)
// Index 1:  GPIO 255 -> DISABLED
// Index 2:  GPIO 4  -> Demo (wild effects while held)
// Index 3:  GPIO 23 -> Drums (slow green pulse 7s)
// Index 4:  GPIO 22 -> Pauken (medium orange pulse 7s)
// Index 5:  GPIO 19 -> Tschinellen (fast red pulse 7s + solid 30s)
// Index 6:  GPIO 18 -> Liras (White 100%)
// Index 7:  GPIO 5  -> Trompeten (Pink Ripple)
// Index 8:  GPIO 17 -> Posaunen (Rainbow)
// Index 9:  GPIO 16 -> Bass (Blackout)
// Index 10: GPIO 21 -> (frei)

// ESP-NOW configuration (must match gateway/nano settings)
constexpr uint8_t kWifiChannel = 11;
constexpr bool kLongRangeEnabled = true;
constexpr int8_t kTxPowerDbm = 20;

// Command frame configuration
constexpr size_t kFrameSize = 16;

// Timing
constexpr uint32_t kDebounceMs = 50;
constexpr uint32_t kCommandIntervalMs = 250;  // How often to resend effect while active

// Group bitmasks
namespace Group
{
    constexpr uint16_t kBroadcast = 0xFFFF;
}

// Command codes (same as nano constants.h)
namespace Cmd
{
    constexpr uint8_t kStateBlackout = 0x14;
    constexpr uint8_t kEffectSolid = 0x20;
    constexpr uint8_t kEffectBlink = 0x21;
    constexpr uint8_t kEffectRainbow = 0x23;
    constexpr uint8_t kEffectRainbowCycle = 0x24;
    constexpr uint8_t kEffectChase = 0x25;
    constexpr uint8_t kEffectTheaterChase = 0x26;
    constexpr uint8_t kEffectTwinkle = 0x27;
    constexpr uint8_t kEffectFire = 0x29;
    constexpr uint8_t kEffectPulse = 0x2A;
    constexpr uint8_t kEffectWave = 0x2D;
    constexpr uint8_t kEffectMeteor = 0x2E;
    constexpr uint8_t kEffectBounce = 0x31;
    constexpr uint8_t kEffectScanner = 0x33;
    constexpr uint8_t kEffectConfetti = 0x34;
    constexpr uint8_t kEffectLightning = 0x35;
    constexpr uint8_t kEffectPolice = 0x36;
    constexpr uint8_t kEffectStacking = 0x37;
    constexpr uint8_t kEffectRipple = 0x39;
    constexpr uint8_t kEffectPlasma = 0x3A;
}

// Demo mode effects array
constexpr uint8_t kDemoEffects[] = {
    Cmd::kEffectRainbowCycle,
    Cmd::kEffectFire,
    Cmd::kEffectPolice,
    Cmd::kEffectMeteor,
    Cmd::kEffectLightning,
    Cmd::kEffectPlasma,
    Cmd::kEffectChase,
    Cmd::kEffectConfetti,
    Cmd::kEffectTheaterChase,
    Cmd::kEffectWave,
    Cmd::kEffectTwinkle,
    Cmd::kEffectScanner,
    Cmd::kEffectBounce,
    Cmd::kEffectRipple,
    Cmd::kEffectStacking,
    Cmd::kEffectBlink,
};
constexpr uint8_t kDemoEffectCount = sizeof(kDemoEffects) / sizeof(kDemoEffects[0]);
constexpr uint32_t kDemoIntervalMs = 2000;  // 2 seconds per effect

// Flags (lower 4 bits of flags byte)
namespace Flag
{
    constexpr uint8_t kPriority = 0x01;
}

// TTL configuration (upper 4 bits of flags byte)
constexpr uint8_t kDefaultTTL = 2;
constexpr uint8_t kTTLShift = 4;

inline uint8_t MakeFlagsByte(uint8_t ttl, uint8_t flags) {
    return ((ttl << kTTLShift) & 0xF0) | (flags & 0x0F);
}

// =============================================================================
// EFFECT DEFINITIONS
// =============================================================================

// Effect phases
enum class EffectPhase : uint8_t
{
    IDLE = 0,
    PULSE,
    SOLID,
    BLACKOUT
};

// Effect configuration per button
struct ButtonEffect
{
    bool enabled;             // Whether this button has an effect
    bool isDemo;              // Demo mode: cycle through effects while held
    bool isInstant;           // Instant effect: send once, no timing (e.g., blackout)

    // Phase 1: Main effect
    uint8_t effectType;       // Effect command (e.g., kEffectPulse, kEffectRipple, kEffectSolid)
    uint8_t effectR;
    uint8_t effectG;
    uint8_t effectB;
    uint16_t effectSpeed;     // Lower = faster
    uint8_t effectLength;     // Length parameter for effects that use it
    uint8_t effectIntensity;
    uint32_t effectDurationMs;

    // Phase 2: Solid (optional, set duration to 0 to skip)
    uint8_t solidR;
    uint8_t solidG;
    uint8_t solidB;
    uint8_t solidIntensity;
    uint32_t solidDurationMs;
};

// Disabled effect (for unused buttons)
constexpr ButtonEffect kEffectDisabled = {
    .enabled = false,
    .isDemo = false,
    .isInstant = false,
    .effectType = 0, .effectR = 0, .effectG = 0, .effectB = 0,
    .effectSpeed = 0, .effectLength = 0, .effectIntensity = 0, .effectDurationMs = 0,
    .solidR = 0, .solidG = 0, .solidB = 0,
    .solidIntensity = 0, .solidDurationMs = 0
};

// Button 2 (GPIO 4): Demo - cycle through wild effects while held
constexpr ButtonEffect kEffectDemo = {
    .enabled = true,
    .isDemo = true,
    .isInstant = false,
    .effectType = 0, .effectR = 0, .effectG = 0, .effectB = 0,
    .effectSpeed = 0, .effectLength = 0, .effectIntensity = 0, .effectDurationMs = 0,
    .solidR = 0, .solidG = 0, .solidB = 0,
    .solidIntensity = 0, .solidDurationMs = 0
};

// Button 3 (GPIO 23): Drums - slow green pulse for 7s
constexpr ButtonEffect kEffectDrums = {
    .enabled = true,
    .isDemo = false,
    .isInstant = false,
    .effectType = Cmd::kEffectPulse,
    .effectR = 0,
    .effectG = 255,
    .effectB = 0,
    .effectSpeed = 180,        // Slow
    .effectLength = 0,
    .effectIntensity = 255,
    .effectDurationMs = 7000,
    .solidR = 0,
    .solidG = 0,
    .solidB = 0,
    .solidIntensity = 0,
    .solidDurationMs = 0      // No solid phase
};

// Button 4 (GPIO 22): Pauken - faster orange pulse for 7s
constexpr ButtonEffect kEffectPauken = {
    .enabled = true,
    .isDemo = false,
    .isInstant = false,
    .effectType = Cmd::kEffectPulse,
    .effectR = 255,
    .effectG = 100,
    .effectB = 0,
    .effectSpeed = 100,        // Medium-fast
    .effectLength = 0,
    .effectIntensity = 255,
    .effectDurationMs = 7000,
    .solidR = 0,
    .solidG = 0,
    .solidB = 0,
    .solidIntensity = 0,
    .solidDurationMs = 0      // No solid phase
};

// Button 5 (GPIO 19): Tschinellen - fast red pulse for 7s, then red solid 50% for 30s
constexpr ButtonEffect kEffectTschinellen = {
    .enabled = true,
    .isDemo = false,
    .isInstant = false,
    .effectType = Cmd::kEffectPulse,
    .effectR = 255,
    .effectG = 0,
    .effectB = 0,
    .effectSpeed = 50,         // Fast
    .effectLength = 0,
    .effectIntensity = 255,
    .effectDurationMs = 7000,
    .solidR = 255,
    .solidG = 0,
    .solidB = 0,
    .solidIntensity = 128,    // 50%
    .solidDurationMs = 30000
};

// Button 6 (GPIO 18): Liras - White 100% solid (instant, stays until other button)
constexpr ButtonEffect kEffectLiras = {
    .enabled = true,
    .isDemo = false,
    .isInstant = true,
    .effectType = Cmd::kEffectSolid,
    .effectR = 255,
    .effectG = 255,
    .effectB = 255,
    .effectSpeed = 0,
    .effectLength = 0,
    .effectIntensity = 255,    // 100%
    .effectDurationMs = 0,
    .solidR = 0, .solidG = 0, .solidB = 0,
    .solidIntensity = 0, .solidDurationMs = 0
};

// Button 7 (GPIO 5): Trompeten - Pink Ripple (from launchpad calm section)
constexpr ButtonEffect kEffectTrompeten = {
    .enabled = true,
    .isDemo = false,
    .isInstant = true,
    .effectType = Cmd::kEffectRipple,
    .effectR = 255,
    .effectG = 50,
    .effectB = 150,
    .effectSpeed = 300,
    .effectLength = 6,
    .effectIntensity = 255,
    .effectDurationMs = 0,
    .solidR = 0, .solidG = 0, .solidB = 0,
    .solidIntensity = 0, .solidDurationMs = 0
};

// Button 8 (GPIO 17): Posaunen - Rainbow
constexpr ButtonEffect kEffectPosaunen = {
    .enabled = true,
    .isDemo = false,
    .isInstant = true,
    .effectType = Cmd::kEffectRainbowCycle,
    .effectR = 255,
    .effectG = 0,
    .effectB = 0,
    .effectSpeed = 80,
    .effectLength = 0,
    .effectIntensity = 255,
    .effectDurationMs = 0,
    .solidR = 0, .solidG = 0, .solidB = 0,
    .solidIntensity = 0, .solidDurationMs = 0
};

// Button 9 (GPIO 16): Bass - Blackout (instant)
constexpr ButtonEffect kEffectBass = {
    .enabled = true,
    .isDemo = false,
    .isInstant = true,
    .effectType = Cmd::kStateBlackout,
    .effectR = 0,
    .effectG = 0,
    .effectB = 0,
    .effectSpeed = 0,
    .effectLength = 0,
    .effectIntensity = 0,
    .effectDurationMs = 0,
    .solidR = 0, .solidG = 0, .solidB = 0,
    .solidIntensity = 0, .solidDurationMs = 0
};

// Array of all button effects (must match kButtonPins order)
constexpr ButtonEffect kButtonEffects[kNumButtons] = {
    kEffectDisabled,      // Index 0:  GPIO 15 -> (nicht verwendet)
    kEffectDisabled,      // Index 1:  DISABLED
    kEffectDemo,          // Index 2:  GPIO 4  -> Demo (wild effects while held)
    kEffectDrums,         // Index 3:  GPIO 23 -> Drums
    kEffectPauken,        // Index 4:  GPIO 22 -> Pauken
    kEffectTschinellen,   // Index 5:  GPIO 19 -> Tschinellen
    kEffectLiras,         // Index 6:  GPIO 18 -> Liras (White 100%)
    kEffectTrompeten,     // Index 7:  GPIO 5  -> Trompeten (Pink Ripple)
    kEffectPosaunen,      // Index 8:  GPIO 17 -> Posaunen (Rainbow)
    kEffectBass,          // Index 9:  GPIO 16 -> Bass (Blackout)
    kEffectDisabled,      // Index 10: GPIO 21 -> (nicht verwendet)
};
