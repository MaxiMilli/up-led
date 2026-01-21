#include "eeprom_handler.h"

#include "logging.h"

NanoConfig config;
Preferences preferences;

NanoConfig GetDefaultConfig()
{
	NanoConfig cfg;
	cfg.magic = kConfigMagic;
	cfg.version = kConfigVersion;
	cfg.groups = Group::kAll;
	cfg.ledCount = 30;
	cfg.ledPin = 4;
	cfg.maxBrightness = 255;
	cfg.meshTTL = kDefaultMeshTTL;
	cfg.channel = kWifiChannel;
	cfg.standbyR = 0;
	cfg.standbyG = 0;
	cfg.standbyB = 50;
	cfg.deviceRegister = 0;
	cfg.configured = false;
	return cfg;
}

void InitializeEEPROM()
{
	if (!EEPROM.begin(kEepromSize))
	{
		LOG("Failed to initialize EEPROM");
		return;
	}
	LOG("EEPROM initialized");
	LoadConfig();
	LoadPairingConfig();
}

void LoadConfig()
{
	EEPROM.get(0, config);

	if (config.magic != kConfigMagic || config.version != kConfigVersion)
	{
		LOG("Invalid config, loading defaults");
		config = GetDefaultConfig();
		SaveConfig();
		return;
	}

	if (config.meshTTL > kMaxMeshTTL)
	{
		config.meshTTL = kDefaultMeshTTL;
	}

	LOGF("Config loaded: groups=0x%04X leds=%u ttl=%u\n",
		  config.groups, config.ledCount, config.meshTTL);
}

void SaveConfig()
{
	config.magic = kConfigMagic;
	config.version = kConfigVersion;
	EEPROM.put(0, config);
	EEPROM.commit();
	LOG("Config saved");
}

void FactoryReset()
{
	config = GetDefaultConfig();
	SaveConfig();
	ClearPairingConfig();
	LOG("Factory reset complete");
}

bool IsDeviceConfigured()
{
	return config.configured;
}

bool SavePairingConfig(uint8_t deviceRegister, uint16_t ledCount, uint8_t standbyR, uint8_t standbyG, uint8_t standbyB)
{
	if (!preferences.begin(kNvsNamespace, false))
	{
		LOG("Failed to open NVS for writing");
		return false;
	}

	preferences.putUChar(kNvsKeyRegister, deviceRegister);
	preferences.putUShort(kNvsKeyLedCount, ledCount);
	preferences.putUChar(kNvsKeyStandbyR, standbyR);
	preferences.putUChar(kNvsKeyStandbyG, standbyG);
	preferences.putUChar(kNvsKeyStandbyB, standbyB);
	preferences.putBool(kNvsKeyConfigured, true);
	preferences.end();

	config.deviceRegister = deviceRegister;
	config.ledCount = ledCount;
	config.standbyR = standbyR;
	config.standbyG = standbyG;
	config.standbyB = standbyB;
	config.configured = true;
	config.groups = RegisterToGroupBitmask(deviceRegister);

	LOGF("Pairing config saved: register=%u ledCount=%u groups=0x%04X standby=(%u,%u,%u)\n",
		  deviceRegister, ledCount, config.groups, standbyR, standbyG, standbyB);
	return true;
}

bool LoadPairingConfig()
{
	if (!preferences.begin(kNvsNamespace, true))
	{
		LOG("NVS namespace not found, device unconfigured");
		config.configured = false;
		return false;
	}

	bool configured = preferences.getBool(kNvsKeyConfigured, false);
	if (!configured)
	{
		preferences.end();
		config.configured = false;
		LOG("Device not configured");
		return false;
	}

	config.deviceRegister = preferences.getUChar(kNvsKeyRegister, 0);
	config.ledCount = preferences.getUShort(kNvsKeyLedCount, 30);
	config.standbyR = preferences.getUChar(kNvsKeyStandbyR, 0);
	config.standbyG = preferences.getUChar(kNvsKeyStandbyG, 0);
	config.standbyB = preferences.getUChar(kNvsKeyStandbyB, 255);
	config.configured = true;
	config.groups = RegisterToGroupBitmask(config.deviceRegister);
	preferences.end();

	LOGF("Pairing config loaded: register=%u ledCount=%u groups=0x%04X standby=(%u,%u,%u)\n",
		  config.deviceRegister, config.ledCount, config.groups,
		  config.standbyR, config.standbyG, config.standbyB);
	return true;
}

void ClearPairingConfig()
{
	if (!preferences.begin(kNvsNamespace, false))
	{
		LOG("Failed to open NVS for clearing");
		return;
	}

	preferences.clear();
	preferences.end();

	config.deviceRegister = 0;
	config.configured = false;
	LOG("Pairing config cleared");
}

uint16_t RegisterToGroupBitmask(uint8_t deviceRegister)
{
	// Register 0 means unconfigured - only respond to kAll (broadcast)
	if (deviceRegister == 0)
	{
		return Group::kAll;
	}

	// Map register to group bitmask:
	// Register 1 -> kGroup1 (0x0002) | kAll
	// Register 2 -> kGroup2 (0x0004) | kAll
	// etc.
	// kAll is always included for broadcast support
	return Group::kAll | (1 << deviceRegister);
}
