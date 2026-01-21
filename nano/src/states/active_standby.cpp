#include "command.h"
#include "hub_connection.h"
#include "led_handler.h"
#include "states.h"
#include "wifi_connection.h"

void ProcessCommand(Command command, State &current_state)
{
  if (IsLedEffect(command))
  {
    current_state = kActive;
    SetLedEffect(command);
    return;
  }

  HandleSetStateCommand(current_state, command);
}

void HandleActiveStandbyState(State &current_state)
{
  if (!wifi_connected or !IsHubConnected())
  {
    current_state = kInit;
    return;
  }

  // LEDs glow white
  SetLedColor(15, 15, 15);

  Command command;
  if (GetCommandFromHub(command))
  {
    ProcessCommand(command, current_state);
  }
  else
  {
    delay(100);
  }
}
