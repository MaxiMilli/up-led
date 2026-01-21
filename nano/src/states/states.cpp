#include "states.h"

#include "effects.h"
#include "logging.h"

State last_state = kInit;

void HandleState(State &current_state)
{
  if (current_state != last_state)
  {
    LOGF("Switching from state %d to %d\n", last_state, current_state);
    last_state = current_state;
  }

  switch (current_state)
  {
  case kInit:
    HandleInitState(current_state);
    break;
  case kActiveStandby:
    HandleActiveStandbyState(current_state);
    break;
  case kStandby:
    HandleStandbyState(current_state);
    break;
  case kActive:
    HandleActiveState(current_state);
    break;
  case kEmergencyStandby:
    HandleEmergencyStandbyState(current_state);
    break;
  case kOff:
    HandleOffState(current_state);
    break;
  default:
    // Handle unexpected state
    current_state = kInit; // Default to kInit if state is invalid
    break;
  }
}

void HandleSetStateCommand(State &current_state, Command command)
{
  switch (command.effect)
  {
  case Effect::kStateInit:
    current_state = kInit;
    break;
  case Effect::kStateActiveStandby:
    current_state = kActiveStandby;
    break;
  case Effect::kStateStandby:
    current_state = kStandby;
    break;
  default:
    // not one of the state commands -> do nothing
    break;
  }
}
