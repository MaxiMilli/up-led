#include <Arduino.h>

#include "button_handler.h"
#include "constants.h"
#include "eeprom_handler.h"
#include "espnow_handler.h"
#include "led_handler.h"
#include "logging.h"
#include "ota_handler.h"
#include "states.h"

State currentState = kInit;

void setup()
{
  pinMode(kOnboardLedPin, OUTPUT);

  InitializeLogging();
  LOG("Nano starting...");

  InitializeEEPROM();
  InitializeLeds();
  InitializeButton();

  // OTA deaktiviert - bei Bedarf manuell flashen
  // CheckAndPerformOta();

  if (!InitializeEspNow())
  {
    LOG("ESP-NOW init failed!");
  }

  LOG("Setup complete");
}

void loop()
{
  if (ProcessButton())
  {
    StartPairing();
    currentState = kPairing;
  }

  HandleState(currentState);
}
