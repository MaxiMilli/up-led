#pragma once

#include <Arduino.h>

// Check Hub for firmware update on boot
// Returns true if update was performed (device will reboot automatically)
// Returns false if no update needed or update failed
bool CheckAndPerformOta();

// Get current firmware version (embedded at compile time)
uint32_t GetFirmwareVersion();
