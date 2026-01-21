#pragma once

#include <Arduino.h>
#include <EEPROM.h>

// EEPROM size needed (in bytes)
constexpr size_t EEPROM_SIZE = 16;

// EEPROM addresses
constexpr uint8_t LED_COUNT_MAGIC_ADDR = 0;
constexpr uint8_t LED_COUNT_ADDR = 1;
constexpr uint8_t COLOR_MAGIC_ADDR = 2;
constexpr uint8_t COLOR_R_ADDR = 3;
constexpr uint8_t COLOR_G_ADDR = 4;
constexpr uint8_t COLOR_B_ADDR = 5;

// Magic numbers for validation
constexpr uint8_t LED_COUNT_MAGIC_NUMBER = 0xAA;
constexpr uint8_t COLOR_MAGIC_NUMBER = 0xBB;

// Default values
constexpr uint8_t DEFAULT_STANDBY_R = 255;
constexpr uint8_t DEFAULT_STANDBY_G = 255;
constexpr uint8_t DEFAULT_STANDBY_B = 255;

void InitializeEEPROM();
void SaveLedCount(uint8_t count);
uint8_t LoadLedCount();
void SaveStandbyColors(uint8_t r, uint8_t g, uint8_t b);
void LoadStandbyColors(uint8_t &r, uint8_t &g, uint8_t &b);
