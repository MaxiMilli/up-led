#pragma once

#include "constants.h"

// =============================================================================
// INSTRUMENT GROUP DEFINITIONS (see PROTOCOL.md)
// =============================================================================
//
// Unified instrument groups - clean 1-7 mapping:
// Register 1 = Drums, Register 2 = Pauken, etc.
//
// NOTE: These are now defined in constants.h under namespace Instrument.
// The values here are kept for backwards compatibility but should use
// the constants from Instrument:: namespace.

// =============================================================================
// BUTTON-TO-INSTRUMENT CONFIGURATION
// =============================================================================
//
// Button pin mapping (uses new unified register numbers 1-7):
//   Button 0  = GPIO 15 -> Random (picks random instrument)
//   Button 1  = GPIO 2  -> DISABLED (needs external pull-up)
//   Button 2  = GPIO 4  -> Broadcast (alle Nanos)
//   Button 3  = GPIO 16 -> Drums       (Register 1 = 0x0002)
//   Button 4  = GPIO 17 -> Pauken      (Register 2 = 0x0004)
//   Button 5  = GPIO 5  -> Tschinellen (Register 3 = 0x0008)
//   Button 6  = GPIO 18 -> Liras       (Register 4 = 0x0010)
//   Button 7  = GPIO 19 -> Trompeten   (Register 5 = 0x0020)
//   Button 8  = GPIO 21 -> Posaunen    (Register 6 = 0x0040)
//   Button 9  = GPIO 22 -> Baesse      (Register 7 = 0x0080)
//   Button 10 = GPIO 23 -> (frei)
//
// =============================================================================

// Special value for random group selection
constexpr uint16_t kRandomGroup = 0x0000;

// Available instrument groups for random selection (uses new Instrument:: from constants.h)
constexpr uint16_t kRandomGroupPool[] = {
    Instrument::kDrums,       // 0x0002 - Register 1
    Instrument::kPauken,      // 0x0004 - Register 2
    Instrument::kTschinellen, // 0x0008 - Register 3
    Instrument::kLiras,       // 0x0010 - Register 4
    Instrument::kTrompeten,   // 0x0020 - Register 5
    Instrument::kPosaunen,    // 0x0040 - Register 6
    Instrument::kBaesse,      // 0x0080 - Register 7
};
constexpr uint8_t kRandomGroupPoolSize = sizeof(kRandomGroupPool) / sizeof(kRandomGroupPool[0]);

constexpr uint16_t kButtonGroups[kNumButtons] = {
    kRandomGroup,             // Button 0  (GPIO 15) -> Random instrument
    Group::kAll,              // Button 1  (GPIO 2)  -> DISABLED
    Group::kBroadcast,        // Button 2  (GPIO 4)  -> Alle
    Instrument::kDrums,       // Button 3  (GPIO 16) -> Drums (Register 1)
    Instrument::kPauken,      // Button 4  (GPIO 17) -> Pauken (Register 2)
    Instrument::kTschinellen, // Button 5  (GPIO 5)  -> Tschinellen (Register 3)
    Instrument::kLiras,       // Button 6  (GPIO 18) -> Liras (Register 4)
    Instrument::kTrompeten,   // Button 7  (GPIO 19) -> Trompeten (Register 5)
    Instrument::kPosaunen,    // Button 8  (GPIO 21) -> Posaunen (Register 6)
    Instrument::kBaesse,      // Button 9  (GPIO 22) -> Baesse (Register 7)
    Group::kAll,              // Button 10 (GPIO 23) -> (frei, derzeit kAll)
};

// =============================================================================
// STROBE EFFECT CONFIGURATION
// =============================================================================

// Strobe color (RGB)
constexpr uint8_t kStrobeR = 255;
constexpr uint8_t kStrobeG = 0;
constexpr uint8_t kStrobeB = 0;

// Strobe speed (lower = faster, range ~50-500)
constexpr uint16_t kStrobeSpeed = 100;

// Strobe intensity (0-255)
constexpr uint8_t kStrobeIntensity = 255;
