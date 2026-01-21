#include <Arduino.h>

#include "wifi_connection.h"
// #include "hub_connection.h"
// #include "command.h"

#include "constants.h"
#include "led_handler.h"
#include "logging.h"
#include "states.h"
#include "eeprom_handler.h"

State current_state = kInit;

void setup()
{
  // configure on-board button and LED to be used
  pinMode(kOnboardButtonPin, INPUT_PULLUP);
  pinMode(kOnboardLedPin, OUTPUT);

  InitializeLogging();

  InitializeEEPROM();
  InitializeLeds();

  LOG("Starting");
  ConnectToWifi();
}

void loop()
{

  HandleState(current_state);

  // if (response_length == 11){
  //   DecodeCommand(response_buffer);
  // }
}
