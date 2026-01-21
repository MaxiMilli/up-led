#include "command.h"
#include "eeprom_handler.h"
#include "hub_connection.h"
#include "led_handler.h"
#include "states.h"
#include "wifi_connection.h"

// Globale Variablen für die Animation
static float offset = 0.0f;
constexpr float MAX_BRIGHTNESS = 0.06f;  // Reduziert auf 2% maximale Helligkeit für sanfteren Effekt
constexpr float WAVE_LENGTH = 0.3f;  // Kürzere Wellenlänge für mehr Details
constexpr int NUM_WAVES = 4;  // Mehr überlagerte Wellen für komplexeres Muster
constexpr float WAVE_STEP = 0.002f;  // Deutlich kleinerer Step für langsamere Bewegung
constexpr int UPDATE_INTERVAL = 1000;  // 20ms (50 FPS) für flüssigere Animation

// Hilfsfunktion für sanfte Übergänge
float smoothstep2(float x) {
  x = constrain(x, 0.0f, 1.0f);
  return x * x * (3 - 2 * x);
}

void HandleStandbyState(State &current_state) {
  if (!wifi_connected) {
    current_state = kInit;
    return;
  }

  if (!IsHubConnected()) {
    RegisterOnHub();
  }

  Command command;
  if (GetCommandFromHub(command)) {
    if (!IsLedEffect(command)) {
      HandleSetStateCommand(current_state, command);
    } else {
      SetLedEffect(command);
    }

    return;
  }

  // Standby-Farbe laden
  uint8_t base_r, base_g, base_b;
  LoadStandbyColors(base_r, base_g, base_b);

  // Für jede LED die Position der Wellen berechnen
  for (int i = 0; i < NUM_LEDS; i++) {
    float brightness = 0.0f;
    float pos = (float)i / NUM_LEDS;

    // Mehrere Wellen überlagern mit unterschiedlichen Frequenzen
    for (int wave = 0; wave < NUM_WAVES; wave++) {
      float frequency = 1.0f + wave * 0.7f;  // Verschiedene Frequenzen für jede Welle
      float phase = wave * (2.0f * PI / NUM_WAVES);
      float wave_contrib = sin(2.0f * PI * (pos * WAVE_LENGTH * frequency + offset + phase));

      // Gewichtung der Wellen - spätere Wellen haben weniger Einfluss
      float weight = 1.0f / (wave + 1);
      brightness += (wave_contrib + 1.0f) * weight;
    }

    // Brightness normalisieren und mit smoothstep glätten
    brightness = brightness / (2.0f * NUM_WAVES);
    brightness = smoothstep2(brightness) * MAX_BRIGHTNESS;

    // Farbübergänge zusätzlich glätten
    uint8_t r = (uint8_t)(base_r * brightness);
    uint8_t g = (uint8_t)(base_g * brightness);
    uint8_t b = (uint8_t)(base_b * brightness);

    strip->setPixelColor(i, strip->Color(r, g, b));
  }

  // Wellen sehr langsam bewegen
  offset += WAVE_STEP;
  if (offset >= 1.0f) {
    offset -= 1.0f;
  }

  // LED-Strip aktualisieren und kürzere Verzögerung für flüssigere Animation
  strip->show();
  delay(UPDATE_INTERVAL);
}
