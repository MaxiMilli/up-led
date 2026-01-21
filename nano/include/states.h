#pragma once

#include "command.h"

enum State
{
  kInit,
  kUnconfigured,
  kPairing,
  kConnecting,
  kStandby,
  kActive,
  kBlackout,
  kDisconnected
};

extern State currentState;

/**
 * @brief Get string name of state for logging
 */
const char *GetStateName(State state);

/**
 * @brief Main state handler - call in loop
 */
void HandleState(State &currentState);

/**
 * @brief Process a command and potentially change state
 * @param currentState Reference to current state
 * @param cmd Command to process
 */
void ProcessCommand(State &currentState, const Command &cmd);

void HandleInitState(State &currentState);
void HandleUnconfiguredState(State &currentState);
void HandlePairingState(State &currentState);
void HandleConnectingState(State &currentState);
void HandleStandbyState(State &currentState);
void HandleActiveState(State &currentState);
void HandleBlackoutState(State &currentState);
void HandleDisconnectedState(State &currentState);

/**
 * @brief Start pairing mode
 */
void StartPairing();

/**
 * @brief Check if pairing mode is active
 * @returns true if in pairing mode
 */
bool IsPairingActive();

/**
 * @brief Handle received pairing ACK
 */
void OnPairingAckReceived();

/**
 * @brief Handle received config set command
 * @param deviceRegister Register assigned by gateway
 * @param ledCount Number of LEDs
 * @param standbyR Standby color red (0-255)
 * @param standbyG Standby color green (0-255)
 * @param standbyB Standby color blue (0-255)
 * @returns true if config was applied successfully
 */
bool OnConfigSetReceived(uint8_t deviceRegister, uint16_t ledCount, uint8_t standbyR, uint8_t standbyG, uint8_t standbyB);
