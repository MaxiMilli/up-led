#include "logging.h"

#include "constants.h"

// Initialize the logging flag
bool logging_enabled = false;

void InitializeLogging() {
  // Logging is activated if the onboard button is pressed immediately after
  // startup In the case of a release build, this is similar to a debug mode
  // This is indicated by the onboard LED glowing
  delay(500);
  if (digitalRead(kOnboardButtonPin) == LOW) {  // Button is active LOW
    digitalWrite(kOnboardLedPin, true);
    logging_enabled = true;
  } else {
    digitalWrite(kOnboardLedPin, false);
  }

  if (ENABLE_LOGGING_DEFAULT || logging_enabled) {
    Serial.begin(115200);
    delay(500);
    LOGF("Logging enabled. Debug: %d\n", logging_enabled);
  }
}
