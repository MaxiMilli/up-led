#pragma once

#include <Adafruit_NeoPixel.h>

#include "command.h"

extern Adafruit_NeoPixel *strip;

extern uint16_t NUM_LEDS;

constexpr const int kLedPin = 2;
constexpr const int kLedType = NEO_GRB + NEO_KHZ800;

void InitializeLeds();

void SetLedColor(uint8_t red, uint8_t green, uint8_t blue);

void TurnOffLeds();

void SetLedEffect(Command command);

void UpdateLedEffect();

uint32_t GetBaseColor(Command command);
uint32_t ApplyIntensity(uint32_t color, uint8_t intensity);
