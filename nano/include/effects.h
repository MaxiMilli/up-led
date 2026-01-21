#pragma once

#include <Arduino.h>

namespace Effect
{
    constexpr uint8_t kWalkingLight = 30;
    constexpr uint8_t kGlitter = 31;
    constexpr uint8_t kAlternateRunningLight = 32;
    constexpr uint8_t kPulsing = 33;
    constexpr uint8_t kStarlightDrift = 34;
    constexpr uint8_t kStrobo = 35;
    constexpr uint8_t kRainbow = 36;
    constexpr uint8_t kColorPulseRipple = 37;
    constexpr uint8_t kMeteor = 38;
    constexpr uint8_t kFlicker = 39;
    constexpr uint8_t kNeonComet = 40;
    constexpr uint8_t kDoppler = 41;
    constexpr uint8_t kFirework = 42;
    constexpr uint8_t kDNAHelix = 43;
    constexpr uint8_t kDNAHelixColorWhite = 44;

    constexpr uint8_t kStateDark = 100;
    constexpr uint8_t kStateStandby = 101;
    constexpr uint8_t kStateActiveStandby = 102;
    constexpr uint8_t kStateInit = 110;

    constexpr uint8_t kRGB = 103;
    constexpr uint8_t kBlink = 105;
    constexpr uint8_t kSingle = 106;
    constexpr uint8_t kAdditionalSingle = 107;
    constexpr uint8_t kSetLedCount = 108;
    constexpr uint8_t kSetStandbyColor = 109;
}
