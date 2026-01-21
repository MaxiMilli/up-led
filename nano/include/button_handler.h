#pragma once

#include <Arduino.h>

/**
 * @brief Initialize button handler
 */
void InitializeButton();

/**
 * @brief Process button state - call in main loop
 * @returns true if long press detected (pairing trigger)
 */
bool ProcessButton();

/**
 * @brief Check if button is currently pressed
 * @returns true if pressed
 */
bool IsButtonPressed();
