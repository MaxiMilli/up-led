#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <Preferences.h>

#include "constants.h"

constexpr size_t kEepromSize = 64;
constexpr uint32_t kConfigMagic = 0xCAFEBABE;
constexpr uint8_t kConfigVersion = 2;

constexpr char kNvsNamespace[] = "nano_config";
constexpr char kNvsKeyRegister[] = "register";
constexpr char kNvsKeyLedCount[] = "led_count";
constexpr char kNvsKeyConfigured[] = "configured";
constexpr char kNvsKeyStandbyR[] = "standby_r";
constexpr char kNvsKeyStandbyG[] = "standby_g";
constexpr char kNvsKeyStandbyB[] = "standby_b";

struct NanoConfig
{
   uint32_t magic;
   uint8_t version;
   uint16_t groups;
   uint8_t ledCount;
   uint8_t ledPin;
   uint8_t maxBrightness;
   uint8_t meshTTL;
   uint8_t channel;
   uint8_t standbyR;
   uint8_t standbyG;
   uint8_t standbyB;
   uint8_t deviceRegister;
   bool configured;
};

extern NanoConfig config;

/**
 * @brief Initialize EEPROM and load config
 */
void InitializeEEPROM();

/**
 * @brief Load config from EEPROM, or use defaults if invalid
 */
void LoadConfig();

/**
 * @brief Save current config to EEPROM
 */
void SaveConfig();

/**
 * @brief Reset config to factory defaults
 */
void FactoryReset();

/**
 * @brief Get default config values
 * @returns NanoConfig with default values
 */
NanoConfig GetDefaultConfig();

/**
 * @brief Check if device is configured (has received config from gateway)
 * @returns true if configured
 */
bool IsDeviceConfigured();

/**
 * @brief Save pairing config to NVS
 * @param deviceRegister The register assigned by gateway
 * @param ledCount Number of LEDs
 * @param standbyR Standby color red (0-255)
 * @param standbyG Standby color green (0-255)
 * @param standbyB Standby color blue (0-255)
 * @returns true on success
 */
bool SavePairingConfig(uint8_t deviceRegister, uint16_t ledCount, uint8_t standbyR, uint8_t standbyG, uint8_t standbyB);

/**
 * @brief Load pairing config from NVS
 * @returns true if config was loaded successfully
 */
bool LoadPairingConfig();

/**
 * @brief Clear pairing config from NVS
 */
void ClearPairingConfig();

/**
 * @brief Convert device register to group bitmask
 * @param deviceRegister Register number (1-15)
 * @returns Group bitmask (always includes kAll for broadcast support)
 */
uint16_t RegisterToGroupBitmask(uint8_t deviceRegister);
