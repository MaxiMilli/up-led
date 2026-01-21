#pragma once

#include <Arduino.h>

constexpr size_t kCommandSize = 11;

struct Command
{
  uint8_t effect;    // Effect (30...49, 100+)
  uint16_t duration; // Duration in ms (1...65535, 0 = indefinite)
  uint8_t intensity; // Intensity (0...255)
  uint8_t red;       // Red color value (0...255)
  uint8_t green;     // Green color value (0...255)
  uint8_t blue;      // Blue color value (0...255)
  uint8_t rainbow;   // Rainbow mode (0-3)
  uint16_t speed;    // Speed in ms (1...65535, 0 = not applicable)
  uint8_t length;
};

Command DecodeCommand(uint8_t *messageBuffer);

bool IsLedEffect(Command command);
