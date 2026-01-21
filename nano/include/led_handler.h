#pragma once

#include <Adafruit_NeoPixel.h>

#include "command.h"
#include "eeprom_handler.h"

extern Adafruit_NeoPixel *strip;
extern uint16_t numLeds;

constexpr int kLedType = NEO_GRB + NEO_KHZ800;

/**
 * @brief Initialize LED strip from config
 */
void InitializeLeds();

/**
 * @brief Set all LEDs to a single color
 */
void SetLedColor(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Turn off all LEDs (with current fade behavior)
 */
void TurnOffLeds();

/**
 * @brief Turn off all LEDs immediately (no fade)
 */
void TurnOffLedsImmediate();

/**
 * @brief Set LED count and reinitialize strip
 */
void SetLedCount(uint8_t count);

/**
 * @brief Start an LED effect from command
 */
void SetLedEffect(const Command &cmd);

/**
 * @brief Update current running effect (call in loop)
 */
void UpdateLedEffect();

/**
 * @brief Update standby animation
 */
void UpdateStandbyAnimation();

/**
 * @brief Set identify blink effect
 * @param durationMs Duration in milliseconds
 */
void SetIdentifyEffect(uint16_t durationMs);

/**
 * @brief Set emergency red blink effect
 */
void SetEmergencyEffect();

/**
 * @brief Apply intensity to a color
 */
uint32_t ApplyIntensity(uint32_t color, uint8_t intensity);

/**
 * @brief Get color wheel value (0-255)
 */
uint32_t WheelColor(uint8_t pos);

/**
 * @brief Update unconfigured animation (slow red pulse)
 */
void UpdateUnconfiguredAnimation();

/**
 * @brief Update pairing animation (blue blink)
 */
void UpdatePairingAnimation();

/**
 * @brief Show pairing success feedback (green flash)
 */
void SetPairingSuccessFeedback();

/**
 * @brief Show pairing failed feedback (red flash)
 */
void SetPairingFailedFeedback();

/**
 * @brief Show config success feedback (green flash)
 */
void SetConfigSuccessFeedback();

/**
 * @brief Show config failed feedback (red flash)
 */
void SetConfigFailedFeedback();

/**
 * @brief Trigger a brief white flash for heartbeat
 */
void TriggerHeartbeatFlash();

/**
 * @brief Update heartbeat flash animation (call in loop)
 * @returns true if flash is active
 */
bool UpdateHeartbeatFlash();

/**
 * @brief Show dim white standby color
 */
void ShowDimWhiteStandby();
