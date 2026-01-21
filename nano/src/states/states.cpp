#include "states.h"

#include "constants.h"
#include "eeprom_handler.h"
#include "espnow_handler.h"
#include "led_handler.h"
#include "logging.h"

namespace
{
	State lastState = kInit;
	Command currentEffectCmd;
	bool effectActive = false;
	uint32_t effectStartTime = 0;

	bool pairingActive = false;
	uint32_t pairingStartTime = 0;
	uint32_t lastPairingRequest = 0;
	bool pairingAckReceived = false;

	uint32_t lastHeartbeatTime = 0;
}

const char *GetStateName(State state)
{
	switch (state)
	{
	case kInit:
		return "INIT";
	case kUnconfigured:
		return "UNCONFIGURED";
	case kPairing:
		return "PAIRING";
	case kConnecting:
		return "CONNECTING";
	case kStandby:
		return "STANDBY";
	case kActive:
		return "ACTIVE";
	case kBlackout:
		return "BLACKOUT";
	case kDisconnected:
		return "DISCONNECTED";
	default:
		return "UNKNOWN";
	}
}

void HandleState(State &currentState)
{
	if (currentState != lastState)
	{
		LOGF("State: %s -> %s\n", GetStateName(lastState), GetStateName(currentState));
		lastState = currentState;
	}

	ProcessEspNow();

	Command *pending = GetPendingCommand();
	if (pending->effect != Cmd::kNop)
	{
		ProcessCommand(currentState, *pending);
		ClearPendingCommand();
	}

	switch (currentState)
	{
	case kInit:
		HandleInitState(currentState);
		break;

	case kUnconfigured:
		HandleUnconfiguredState(currentState);
		break;

	case kPairing:
		HandlePairingState(currentState);
		break;

	case kConnecting:
		HandleConnectingState(currentState);
		break;

	case kStandby:
		HandleStandbyState(currentState);
		break;

	case kActive:
		HandleActiveState(currentState);
		break;

	case kBlackout:
		HandleBlackoutState(currentState);
		break;

	case kDisconnected:
		HandleDisconnectedState(currentState);
		break;

	default:
		currentState = kInit;
		break;
	}
}

void ProcessCommand(State &currentState, const Command &cmd)
{
	if (IsSystemCommand(cmd.effect))
	{
		switch (cmd.effect)
		{
		case Cmd::kHeartbeat:
			lastHeartbeatTime = millis();
			if (currentState != kActive && currentState != kBlackout)
			{
				TriggerHeartbeatFlash();
			}
			if (currentState == kConnecting)
			{
				currentState = kStandby;
			}
			if (currentState == kDisconnected)
			{
				currentState = kStandby;
			}
			break;

		case Cmd::kPing:
			LOG("PING received");
			break;

		case Cmd::kIdentify:
			SetIdentifyEffect(cmd.duration);
			break;

		case Cmd::kSetLedCount:
			SetLedCount(cmd.length);
			break;

		case Cmd::kSetGroups:
			extern NanoConfig config;
			config.groups = cmd.duration;
			LOGF("Groups set to 0x%04X\n", config.groups);
			break;

		case Cmd::kSaveConfig:
			SaveConfig();
			break;

		case Cmd::kReboot:
			LOG("Rebooting...");
			delay(100);
			ESP.restart();
			break;

		case Cmd::kFactoryReset:
			FactoryReset();
			delay(100);
			ESP.restart();
			break;

		case Cmd::kSetMeshTTL:
			extern NanoConfig config;
			config.meshTTL = min(cmd.length, kMaxMeshTTL);
			LOGF("Mesh TTL set to %u\n", config.meshTTL);
			break;
		}
		return;
	}

	if (IsStateCommand(cmd.effect))
	{
		switch (cmd.effect)
		{
		case Cmd::kStateOff:
			TurnOffLeds();
			currentState = kStandby;
			effectActive = false;
			break;

		case Cmd::kStateStandby:
			currentState = kStandby;
			effectActive = false;
			break;

		case Cmd::kStateActive:
			currentState = kActive;
			break;

		case Cmd::kStateEmergency:
			SetEmergencyEffect();
			currentState = kActive;
			effectActive = true;
			break;

		case Cmd::kStateBlackout:
			TurnOffLedsImmediate();
			effectStartTime = millis();
			currentState = kBlackout;
			effectActive = false;
			break;
		}
		return;
	}

	if (IsEffectCommand(cmd.effect))
	{
		currentEffectCmd = cmd;
		effectActive = true;
		effectStartTime = millis();
		currentState = kActive;
		SetLedEffect(cmd);
		return;
	}

	if (IsDebugCommand(cmd.effect))
	{
		switch (cmd.effect)
		{
		case Cmd::kDebugInfo:
			extern NanoConfig config;
			LOGF("Debug: groups=0x%04X leds=%u ttl=%u\n",
				  config.groups, config.ledCount, config.meshTTL);
			break;
		}
	}
}

void HandleInitState(State &currentState)
{
	if (IsDeviceConfigured())
	{
		currentState = kConnecting;
	}
	else
	{
		currentState = kUnconfigured;
	}
}

void HandleUnconfiguredState(State &currentState)
{
	UpdateStandbyAnimation();
}

void HandlePairingState(State &currentState)
{
	uint32_t now = millis();

	if (now - pairingStartTime >= kPairingTimeoutMs)
	{
		LOG("Pairing timeout");
		pairingActive = false;
		SetPairingFailedFeedback();

		if (IsDeviceConfigured())
		{
			currentState = kConnecting;
		}
		else
		{
			currentState = kUnconfigured;
		}
		return;
	}

	if (pairingAckReceived)
	{
		LOG("Pairing successful, waiting for config...");
		return;
	}

	if (now - lastPairingRequest >= kPairingRequestIntervalMs)
	{
		SendPairingRequest();
		lastPairingRequest = now;
	}

	UpdatePairingAnimation();
}

void HandleConnectingState(State &currentState)
{
	UpdateStandbyAnimation();
}

void HandleStandbyState(State &currentState)
{
	// Check heartbeat timeout
	if (lastHeartbeatTime > 0 && millis() - lastHeartbeatTime > kHeartbeatTimeout)
	{
		LOG("Heartbeat timeout - disconnected");
		currentState = kDisconnected;
		return;
	}

	// If heartbeat flash is active, show it
	if (UpdateHeartbeatFlash())
	{
		return;
	}

	// Behavior based on heartbeat presence
	if (lastHeartbeatTime == 0)
	{
		// Kein Heartbeat = vor Auftritt → farbige Verläufe
		UpdateStandbyAnimation();
	}
	else
	{
		// Heartbeat aktiv = während Auftritt → gedimmtes Weiss
		ShowDimWhiteStandby();
	}
}

void HandleActiveState(State &currentState)
{
	constexpr uint32_t kDefaultEffectDuration = 2000;

	if (effectActive)
	{
		uint32_t effectDuration = currentEffectCmd.duration > 0
			? currentEffectCmd.duration
			: kDefaultEffectDuration;

		if (millis() - effectStartTime >= effectDuration)
		{
			effectActive = false;
			currentState = kStandby;
			return;
		}

		UpdateLedEffect();
	}
	else
	{
		currentState = kStandby;
	}
}

void HandleBlackoutState(State &currentState)
{
	constexpr uint32_t kBlackoutDefaultDuration = 2000;   // Ohne Heartbeat: 2s
	constexpr uint32_t kBlackoutActiveDuration = 60000;   // Mit Heartbeat: 60s

	// Determine timeout based on heartbeat status
	uint32_t blackoutDuration;
	if (currentEffectCmd.duration > 0)
	{
		// Explicit duration from command
		blackoutDuration = currentEffectCmd.duration;
	}
	else if (lastHeartbeatTime > 0)
	{
		// Heartbeat active = während Auftritt → lange warten
		blackoutDuration = kBlackoutActiveDuration;
	}
	else
	{
		// No heartbeat = vor Auftritt → kurz warten
		blackoutDuration = kBlackoutDefaultDuration;
	}

	if (millis() - effectStartTime >= blackoutDuration)
	{
		currentState = kStandby;
		return;
	}
}

void HandleDisconnectedState(State &currentState)
{
	// Show wave animation when disconnected (no heartbeat)
	UpdateStandbyAnimation();
}

void StartPairing()
{
	LOG("Starting pairing mode...");
	pairingActive = true;
	pairingAckReceived = false;
	pairingStartTime = millis();
	lastPairingRequest = 0;
}

bool IsPairingActive()
{
	return pairingActive;
}

void OnPairingAckReceived()
{
	if (!pairingActive)
		return;

	LOG("Pairing ACK received");
	pairingAckReceived = true;
	SetPairingSuccessFeedback();
}

bool OnConfigSetReceived(uint8_t deviceRegister, uint16_t ledCount, uint8_t standbyR, uint8_t standbyG, uint8_t standbyB)
{
	LOG("Config set received");
	LOGF("Register: %u, LED Count: %u, Standby: (%u,%u,%u)\n",
		  deviceRegister, ledCount, standbyR, standbyG, standbyB);

	if (!SavePairingConfig(deviceRegister, ledCount, standbyR, standbyG, standbyB))
	{
		LOG("Failed to save pairing config");
		SetConfigFailedFeedback();
		return false;
	}

	pairingActive = false;
	InitializeLeds();
	SetConfigSuccessFeedback();
	currentState = kConnecting;
	return true;
}
