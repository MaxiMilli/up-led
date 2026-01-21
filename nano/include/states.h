#pragma once

#include "command.h"

enum State {
  kInit,
  kActiveStandby,
  kStandby,
  kActive,
  kEmergencyStandby,
  kOff
};

// Function prototypes for state handling
void HandleState(State &current_state);
void HandleInitState(State &current_state);
void HandleActiveStandbyState(State &current_state);
void HandleStandbyState(State &current_state);
void HandleActiveState(State &current_state);
void HandleEmergencyStandbyState(State &current_state);
void HandleOffState(State &current_state);

void HandleSetStateCommand(State &current_state, Command command);
