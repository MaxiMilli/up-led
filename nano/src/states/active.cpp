#include "command.h"
#include "hub_connection.h"
#include "led_handler.h"
#include "states.h"
#include "wifi_connection.h"

void HandleActiveState(State &current_state)
{
  if (!wifi_connected or !IsHubConnected())
  {
    current_state = kEmergencyStandby;
    return;
  }

  Command command;
  if (GetCommandFromHub(command))
  {
    if (!IsLedEffect(command))
    {
      HandleSetStateCommand(current_state, command);
      return;
    }

    SetLedEffect(command);
  }
  else
  {
    delay(20);
    UpdateLedEffect();
  }
}
