#include "button_handler.h"

#include "constants.h"
#include "logging.h"

namespace
{
   bool lastButtonState = HIGH;
   uint32_t buttonPressStart = 0;
   bool longPressTriggered = false;
}

void InitializeButton()
{
   pinMode(kOnboardButtonPin, INPUT_PULLUP);
   lastButtonState = digitalRead(kOnboardButtonPin);
   LOG("Button handler initialized");
}

bool ProcessButton()
{
   bool currentState = digitalRead(kOnboardButtonPin);

   if (currentState == LOW && lastButtonState == HIGH)
   {
      buttonPressStart = millis();
      longPressTriggered = false;
   }

   if (currentState == LOW && !longPressTriggered)
   {
      uint32_t pressDuration = millis() - buttonPressStart;
      if (pressDuration >= kButtonLongPressMs)
      {
         longPressTriggered = true;
         LOG("Long press detected - triggering pairing");
         lastButtonState = currentState;
         return true;
      }
   }

   if (currentState == HIGH && lastButtonState == LOW)
   {
      longPressTriggered = false;
   }

   lastButtonState = currentState;
   return false;
}

bool IsButtonPressed()
{
   return digitalRead(kOnboardButtonPin) == LOW;
}
