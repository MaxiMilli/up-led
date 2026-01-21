#pragma once

#include <Arduino.h>

#include "command.h"

/**
 * @brief Initialize ESP-NOW communication
 * @returns true on success
 */
bool InitializeEspNow();

/**
 * @brief Check for and process incoming ESP-NOW messages
 * Should be called in main loop
 */
void ProcessEspNow();

/**
 * @brief Send broadcast message via ESP-NOW
 * @param data Pointer to data buffer
 * @param len Length of data (max 250 bytes)
 * @returns true on success
 */
bool SendBroadcast(const uint8_t *data, size_t len);

/**
 * @brief Check if a command with given SEQ was already processed
 * @param seq Sequence number to check
 * @returns true if SEQ is known (duplicate)
 */
bool IsKnownSeq(uint16_t seq);

/**
 * @brief Add SEQ to known sequences buffer
 * @param seq Sequence number to add
 */
void AddKnownSeq(uint16_t seq);

/**
 * @brief Clear all known sequences
 */
void ClearKnownSeqs();

/**
 * @brief Get last received command
 * @returns Pointer to last command, or nullptr if none pending
 */
Command *GetPendingCommand();

/**
 * @brief Clear pending command after processing
 */
void ClearPendingCommand();

/**
 * @brief Get timestamp of last received heartbeat
 * @returns millis() value of last heartbeat
 */
uint32_t GetLastHeartbeatTime();

/**
 * @brief Check if heartbeat timeout has occurred
 * @returns true if no heartbeat received within timeout period
 */
bool IsHeartbeatTimedOut();

/**
 * @brief Send pairing request broadcast
 */
void SendPairingRequest();

/**
 * @brief Send config ACK to gateway
 * @param success true if config was applied successfully
 */
void SendConfigAck(bool success);

/**
 * @brief Process pairing-related messages (called from OnDataReceived)
 * @param data Pointer to received data
 * @param len Length of received data
 * @returns true if message was a pairing message and was handled
 */
bool ProcessPairingMessage(const uint8_t *data, int len);
