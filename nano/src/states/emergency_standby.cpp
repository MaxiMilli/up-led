#include "hub_connection.h"
#include "led_handler.h"
#include "states.h"
#include "wifi_connection.h"

void HandleEmergencyStandbyState(State &current_state) {
  TurnOffLeds();  // Turn off all LEDs, so show is not disturbed

  if (wifi_connected) {  // Check WiFi using utility
    RegisterOnHub();
    if (IsHubConnected()) {
      current_state = kActive;
      return;
    }
  }

  delay(50);
}
