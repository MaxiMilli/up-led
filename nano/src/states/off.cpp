#include <Arduino.h>

#include "hub_connection.h"
#include "led_handler.h"
#include "states.h"
#include "wifi_connection.h"

void HandleOffState(State &current_state)
{
  // Turn off any components using power
  TurnOffLeds();

  // DisconnectFromHub();
  // DisconnectWifi();

  // Turn off ESP 32 almost completely
  // esp_deep_sleep_start();
}
