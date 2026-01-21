#pragma once

#include <Arduino.h>

#include "constants.h"

struct Command
{
  uint16_t seq;
  uint8_t flags;
  uint8_t effect;
  uint16_t groups;
  uint16_t duration;
  uint8_t length;
  uint8_t rainbow;
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint16_t speed;
  uint8_t intensity;
};

/**
 * @brief Parse raw 16-byte frame into Command struct
 * @param buffer Pointer to 16-byte data buffer
 * @returns Parsed Command struct with Big-Endian values converted
 */
Command ParseCommand(const uint8_t *buffer);

/**
 * @brief Check if command targets this nano based on group membership
 * @param cmd Command to check
 * @param myGroups This nano's group bitmask
 * @returns true if at least one group matches
 */
bool MatchesGroup(const Command &cmd, uint16_t myGroups);

/**
 * @brief Check if PRIORITY flag is set
 */
inline bool HasPriorityFlag(const Command &cmd) { return cmd.flags & Flag::kPriority; }

/**
 * @brief Check if FORCE flag is set
 */
inline bool HasForceFlag(const Command &cmd) { return cmd.flags & Flag::kForce; }

/**
 * @brief Check if SYNC flag is set
 */
inline bool HasSyncFlag(const Command &cmd) { return cmd.flags & Flag::kSync; }

/**
 * @brief Check if NO_REBROADCAST flag is set
 */
inline bool HasNoRebroadcastFlag(const Command &cmd) { return cmd.flags & Flag::kNoRebroadcast; }

/**
 * @brief Check if command is an LED effect (0x20-0x3F)
 */
inline bool IsLedEffect(const Command &cmd) { return IsEffectCommand(cmd.effect); }
