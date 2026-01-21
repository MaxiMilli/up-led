#include "hub_connection.h"
#include "led_handler.h"
#include "states.h"
#include "wifi_connection.h"
#include "eeprom_handler.h"

// Structure to hold RGB color values
struct Color
{
  uint8_t r, g, b;
  float brightness;
};

// Global variables for animation
static Color *ledColors = nullptr;
constexpr float MAX_BRIGHTNESS = 0.05f; // 5% maximum brightness
constexpr float MIN_BRIGHTNESS = 0.01f; // 1% minimum brightness
constexpr float MAX_CHANGE = 0.03f;     // 3% maximum change per update
constexpr int UPDATE_INTERVAL = 1000;   // 1 second between updates

// Helper function to interpolate between two colors
Color interpolateColor(const Color &start, const Color &end, float t)
{
  Color result;
  result.r = start.r + (end.r - start.r) * t;
  result.g = start.g + (end.g - start.g) * t;
  result.b = start.b + (end.b - start.b) * t;
  return result;
}

// Helper function to get random float between min and max
float randomFloat(float min, float max)
{
  return min + (static_cast<float>(random(1000)) / 1000.0f) * (max - min);
}

void HandleInitState(State &current_state)
{
  if (wifi_connected)
  {
    RegisterOnHub();
    if (IsHubConnected())
    {
      current_state = kActiveStandby;
      return;
    }
  }

  // Initialize LED colors array if not already done
  if (ledColors == nullptr)
  {
    ledColors = new Color[NUM_LEDS];

    // Load base colors
    uint8_t base_r, base_g, base_b;
    LoadStandbyColors(base_r, base_g, base_b);

    // Define color ranges based on base color
    Color startColor = {base_r, base_g, base_b, 0};
    Color endColor;

    // Determine end color based on start color
    if (base_r == 255 && base_g == 255 && base_b == 0)
    {                              // Yellow
      endColor = {255, 0, 255, 0}; // to Magenta
    }
    else if (base_r == 255 && base_g == 0 && base_b == 255)
    {                              // Magenta
      endColor = {0, 255, 255, 0}; // to Cyan
    }
    else
    {                              // Cyan
      endColor = {255, 255, 0, 0}; // to Yellow
    }

    // Initialize each LED with random color in range and brightness
    for (int i = 0; i < NUM_LEDS; i++)
    {
      float t = randomFloat(0.0f, 1.0f);
      ledColors[i] = interpolateColor(startColor, endColor, t);
      ledColors[i].brightness = randomFloat(MIN_BRIGHTNESS, MAX_BRIGHTNESS);
    }
  }

  // Update each LED
  for (int i = 0; i < NUM_LEDS; i++)
  {
    // Randomly adjust either color or brightness
    if (random(2) == 0)
    {
      // Adjust brightness
      float change = randomFloat(-MAX_CHANGE, MAX_CHANGE);
      ledColors[i].brightness = constrain(ledColors[i].brightness + change, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
    }

    // Apply brightness to color
    uint8_t r = ledColors[i].r * ledColors[i].brightness;
    uint8_t g = ledColors[i].g * ledColors[i].brightness;
    uint8_t b = ledColors[i].b * ledColors[i].brightness;

    strip->setPixelColor(i, strip->Color(r, g, b));
  }

  strip->show();
  delay(UPDATE_INTERVAL);
}
