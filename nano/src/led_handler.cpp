// led_handler.cpp
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

#include "led_handler.h"
#include "effects.h"
#include "logging.h"
#include "eeprom_handler.h"

uint16_t NUM_LEDS = 0; // Initialize with default value
Adafruit_NeoPixel *strip = nullptr;
bool initialized = false;

Command active_command;
uint32_t step = 0;

static unsigned long lastEffectUpdate = 0;
int fireworkCenter = NUM_LEDS / 2;

uint32_t wheelColor(byte wheelPos)
{
  wheelPos = 255 - wheelPos;
  if (wheelPos < 85)
  {
    return strip->Color(255 - wheelPos * 3, 0, wheelPos * 3);
  }
  else if (wheelPos < 170)
  {
    wheelPos -= 85;
    return strip->Color(0, wheelPos * 3, 255 - wheelPos * 3);
  }
  else
  {
    wheelPos -= 170;
    return strip->Color(wheelPos * 3, 255 - wheelPos * 3, 0);
  }
}

void InitializeLeds()
{
  LOG("Initializing LEDs");

  // Load LED count from EEPROM
  NUM_LEDS = LoadLedCount();
  LOGF("Loaded LED count from EEPROM: %d\n", NUM_LEDS);

  // Delete existing strip if it exists
  if (strip != nullptr)
  {
    delete strip;
  }

  // Create new strip with loaded LED count
  strip = new Adafruit_NeoPixel(NUM_LEDS, kLedPin, kLedType);
  strip->begin();
  strip->clear();
  strip->show();

  initialized = true;
  step = 0;
  lastEffectUpdate = millis();
}

void SetLedColor(uint8_t red, uint8_t green, uint8_t blue)
{
  if (!initialized)
  {
    return;
  }
  uint32_t color = strip->Color(red, green, blue);
  strip->fill(color, 0, NUM_LEDS);
  strip->show();
}

void TurnOffLeds()
{
  if (!initialized)
  {
    return;
  }
  strip->clear();
  strip->show();
}

void SetLedEffect(Command command)
{
  if (!initialized)
  {
    return;
  }
  LOGF("Setting effect %d\n", command.effect);

  if (command.effect == Effect::kSetLedCount)
  {
    LOGF("Setting LED count to %d\n", command.length);
    SaveLedCount(command.length);

    for (int i = 0; i < 100; i++)
    {
      strip->setPixelColor(i, strip->Color(0, 0, 0));
    }

    strip->clear();
    strip->show();
    // Reinitialize LEDs with new count
    InitializeLeds();

    // Flash red for visual feedback
    for (int i = 0; i < NUM_LEDS; i++)
    {
      strip->setPixelColor(i, strip->Color(255, 0, 0));
    }
    strip->show();
    delay(2000);
    strip->clear();
    strip->show();
    return;
  }

  // Für Effekte, die ihren Zustand beibehalten sollen (hier: WalkingLight und Pulsing),
  // wird das Zurücksetzen nur bei einem Wechsel vorgenommen.
  // Für alle anderen Effekte soll der interne Zustand immer zurückgesetzt werden.
  if (command.effect != active_command.effect ||
      (command.effect != Effect::kWalkingLight && command.effect != Effect::kPulsing && command.effect != Effect::kAlternateRunningLight && command.effect != Effect::kRainbow && command.effect != Effect::kMeteor && command.effect != Effect::kDoppler && command.effect != Effect::kDNAHelix && command.effect != Effect::kDNAHelixColorWhite))
  {
    step = 0;
    lastEffectUpdate = millis();

    // Falls der neue Effekt der Feuerwerk-Effekt (kFirework) ist,
    // setze den globalen Firework-Center zurück (siehe unten).
    if (command.effect == Effect::kFirework)
    {
      fireworkCenter = NUM_LEDS / 2;
    }
  }

  active_command = command;
  UpdateLedEffect();

  switch (command.effect)
  {
  case Effect::kRGB:
    SetLedColor(command.red, command.green, command.blue);
    break;
  // Die folgenden Effekte werden in UpdateLedEffect behandelt.
  case Effect::kWalkingLight:
  case Effect::kGlitter:
  case Effect::kAlternateRunningLight:
  case Effect::kPulsing:
  case Effect::kStarlightDrift:
  case Effect::kStrobo:
  case Effect::kRainbow:
  case Effect::kColorPulseRipple:
  case Effect::kMeteor:
  case Effect::kFlicker:
  case Effect::kNeonComet:
  case Effect::kDoppler:
  case Effect::kFirework:
  case Effect::kDNAHelix:
  case Effect::kDNAHelixColorWhite:
  case Effect::kBlink:
  case Effect::kSingle:
    // Zuerst den LED-Streifen leeren:
    // strip->clear();
    // strip->show();
    break;

  case Effect::kSetStandbyColor:
    LOGF("Setting standby color to %d, %d, %d\n", active_command.red, active_command.green, active_command.blue);
    SaveStandbyColors(active_command.red, active_command.green, active_command.blue);
    for (int i = 0; i < NUM_LEDS; i++)
    {
      strip->setPixelColor(i, strip->Color(active_command.red, active_command.green, active_command.blue)); // Set all LEDs to red
    }
    strip->show();
    delay(2000);    // Wait for 2 seconds
    strip->clear(); // Turn off all LEDs
    strip->show();

    TurnOffLeds();
    break;
  case Effect::kStateDark:
    TurnOffLeds();
    break;
  case Effect::kStateInit:
    InitializeLeds();
    break;
  default:
    TurnOffLeds();
    break;
  }
}

// --- Hauptupdate-Funktion: Wird regelmäßig (z. B. in loop()) aufgerufen ---
void UpdateLedEffect()
{
  if (!initialized)
  {
    return;
  }

  unsigned long currentTime = millis();

  if (currentTime - lastEffectUpdate < active_command.speed)
  {
    return;
  }
  lastEffectUpdate = currentTime;
  step++;

  switch (active_command.effect)
  {

  case Effect::kRGB:
  {
    uint8_t intensity = active_command.intensity;
    uint8_t r = (active_command.red * intensity) / 255;
    uint8_t g = (active_command.green * intensity) / 255;
    uint8_t b = (active_command.blue * intensity) / 255;
    uint32_t color = strip->Color(r, g, b);
    strip->fill(color, 0, NUM_LEDS);
    strip->show();
    break;
  }

  case Effect::kWalkingLight:
  { //  effect 30
    strip->clear();
    int groupSize = active_command.length > 0 ? active_command.length : 4;
    int totalPattern = groupSize * 2;
    int pos = step % totalPattern;
    for (int i = 0; i < NUM_LEDS; i++)
    {
      int patternPos = (i + pos) % totalPattern;
      if (patternPos < groupSize)
      {
        float center = (groupSize - 1) / 2.0;
        float distance = fabs(patternPos - center);
        float maxDistance = center;
        float brightnessFactor = 1.0 - (distance / maxDistance) * 0.5;
        uint32_t color = strip->Color((active_command.red * active_command.intensity * brightnessFactor) / 255,
                                      (active_command.green * active_command.intensity * brightnessFactor) / 255,
                                      (active_command.blue * active_command.intensity * brightnessFactor) / 255);
        strip->setPixelColor(i, color);
      }
    }
    strip->show();
    break;
  }

  case Effect::kGlitter:
  { // Effekt 31
    if (step % (active_command.speed > 0 ? active_command.speed : 1) == 0)
    {
      for (int i = 0; i < NUM_LEDS; i++)
      {
        // Verwende 'length' als Prozentwert: Bei z. B. length==30 besteht 30% Wahrscheinlichkeit
        if (random(100) < (active_command.length > 0 ? active_command.length : 10))
        {
          float randInt = 0.6 + (random(40) / 100.0);
          uint32_t color = strip->Color((active_command.red * active_command.intensity * randInt) / 255,
                                        (active_command.green * active_command.intensity * randInt) / 255,
                                        (active_command.blue * active_command.intensity * randInt) / 255);
          strip->setPixelColor(i, color);
        }
        else
        {
          strip->setPixelColor(i, 0);
        }
      }
      strip->show();
    }
    break;
  }

  case Effect::kAlternateRunningLight:
  { // Effekt 32
    int groupSize = active_command.length > 0 ? active_command.length : 4;
    int totalPattern = groupSize * 2;
    int pos = step % totalPattern;
    strip->clear();
    for (int i = 0; i < NUM_LEDS; i++)
    {
      int patternPos = (i + pos) % totalPattern;
      if (patternPos < groupSize)
      {
        if ((patternPos % 2) == 0)
        {
          uint32_t whiteColor = strip->Color(255 * active_command.intensity / 255,
                                             255 * active_command.intensity / 255,
                                             255 * active_command.intensity / 255);
          strip->setPixelColor(i, whiteColor);
        }
        else
        {
          uint32_t color = strip->Color((active_command.red * active_command.intensity) / 255,
                                        (active_command.green * active_command.intensity) / 255,
                                        (active_command.blue * active_command.intensity) / 255);
          strip->setPixelColor(i, color);
        }
      }
    }
    strip->show();
    break;
  }

  case Effect::kPulsing:
  { // Effekt 33
    float phase = ((float)step / 12.75) * 2.0 * PI;
    // Bestimme den minimalen Helligkeitsfaktor:
    // Ist active_command.length > 0, so wird dieser als Prozentwert interpretiert (z. B. 50 => 50% = 0.5).
    // Andernfalls (oder bei 0) nutze 0.4 (d. h. 40%).
    float minBrightness = (active_command.length > 0 ? active_command.length / 100.0 : 0.4);
    // Der Puls bewegt sich von minBrightness bis 1.0
    float pulse = minBrightness + (1.0 - minBrightness) * ((sin(phase) + 1.0) / 2.0);
    uint8_t pulseIntensity = (uint8_t)(pulse * active_command.intensity);
    uint32_t color = strip->Color((active_command.red * pulseIntensity) / 255,
                                  (active_command.green * pulseIntensity) / 255,
                                  (active_command.blue * pulseIntensity) / 255);
    for (int i = 0; i < NUM_LEDS; i++)
    {
      strip->setPixelColor(i, color);
    }
    strip->show();
    break;
  }

  case Effect::kStarlightDrift:
  { // Kanal 34
    // Parameter
    const int maxStars = 30;                 // Maximale Anzahl gleichzeitig aktiver Sterne
    const unsigned long starLifetime = 2000; // Lebensdauer eines Sterns in ms (z. B. 3 Sekunden)
    const float fadeGlobal = 0.90;           // Globaler Fade‑Faktor (0…1) für sanftes Ausblenden

    // Struktur zur Verwaltung einzelner Sterne
    struct Star
    {
      bool active;
      float position;          // Position als Gleitkommazahl (damit der Stern sanft wandert)
      float speed;             // Geschwindigkeit in LED pro Sekunde
      unsigned long spawnTime; // Zeitpunkt des Spawnens (millis)
    };
    // Statisches Array für Sterne – Initialisierung beim ersten Aufruf
    static Star stars[maxStars];
    static bool starsInitialized = false;
    if (!starsInitialized)
    {
      for (int i = 0; i < maxStars; i++)
      {
        stars[i].active = false;
      }
      starsInitialized = true;
    }

    // 1. Globalen Fade auf den gesamten Streifen anwenden
    for (int i = 0; i < NUM_LEDS; i++)
    {
      uint32_t curr = strip->getPixelColor(i);
      uint8_t r = (uint8_t)(((curr >> 16) & 0xFF) * fadeGlobal);
      uint8_t g = (uint8_t)(((curr >> 8) & 0xFF) * fadeGlobal);
      uint8_t b = (uint8_t)((curr & 0xFF) * fadeGlobal);
      strip->setPixelColor(i, strip->Color(r, g, b));
    }

    unsigned long now = millis();

    // 2. Alle aktiven Sterne aktualisieren
    for (int i = 0; i < maxStars; i++)
    {
      if (stars[i].active)
      {
        unsigned long elapsed = now - stars[i].spawnTime;
        if (elapsed > starLifetime)
        {
          stars[i].active = false;
        }
        else
        {
          // Update: Die Position wird anhand der Geschwindigkeit (LED/s) und einer angenommenen Frame-Dauer (ca. 20ms) verschoben
          // (Alternativ könnte der Delta-Zeitwert exakt ermittelt werden.)
          const float deltaTime = 0.02; // ca. 20ms
          stars[i].position += stars[i].speed * deltaTime;
          // Falls der Stern den Streifenende erreicht, einfach im Streifen (zyklisch) weitermachen
          while (stars[i].position >= NUM_LEDS)
            stars[i].position -= NUM_LEDS;
          while (stars[i].position < 0)
            stars[i].position += NUM_LEDS;

          // Berechne eine Helligkeit, die zuerst ansteigt und dann wieder abfällt (Sinuskurve)
          float brightness = sin(PI * (float)elapsed / starLifetime);
          // Nutze die übergebene Farbe (active_command.red/green/blue) und skaliere mit brightness und Intensität
          uint8_t baseR = active_command.red;
          uint8_t baseG = active_command.green;
          uint8_t baseB = active_command.blue;
          uint8_t outR = (uint8_t)(baseR * brightness * active_command.intensity / 255.0);
          uint8_t outG = (uint8_t)(baseG * brightness * active_command.intensity / 255.0);
          uint8_t outB = (uint8_t)(baseB * brightness * active_command.intensity / 255.0);

          // Zeichne den Stern an der gerundeten Position – additiv, falls dort schon etwas leuchtet
          int pos = (int)round(stars[i].position) % NUM_LEDS;
          uint32_t currColor = strip->getPixelColor(pos);
          uint8_t currR = (currColor >> 16) & 0xFF;
          uint8_t currG = (currColor >> 8) & 0xFF;
          uint8_t currB = currColor & 0xFF;
          outR = min(255, currR + outR);
          outG = min(255, currG + outG);
          outB = min(255, currB + outB);
          strip->setPixelColor(pos, strip->Color(outR, outG, outB));
        }
      }
    }

    // 3. Mit geringer Wahrscheinlichkeit einen neuen Stern spawnen (z. B. 2% Chance pro Update)
    if (random(1000) < 200)
    {
      for (int i = 0; i < maxStars; i++)
      {
        if (!stars[i].active)
        {
          stars[i].active = true;
          stars[i].spawnTime = now;
          stars[i].position = random(NUM_LEDS);
          // Zufällige Geschwindigkeit zwischen 0.05 und 0.20 LED/s
          stars[i].speed = 0.05 + ((float)random(1000) / 1000.0f) * (0.20 - 0.05);
          break;
        }
      }
    }

    strip->show();
    break;
  }

  case Effect::kStrobo:
  { // effect 35
    bool isOn = (step % 2 == 0);
    for (int i = 0; i < NUM_LEDS; i++)
    {
      if (isOn)
      {
        strip->setPixelColor(i, strip->Color((active_command.red * active_command.intensity) / 255,
                                             (active_command.green * active_command.intensity) / 255,
                                             (active_command.blue * active_command.intensity) / 255));
      }
      else
      {
        strip->setPixelColor(i, 0);
      }
    }
    strip->show();
    break;
  }

  case Effect::kRainbow:
  { // Effekt 36
    strip->clear();
    for (int i = 0; i < NUM_LEDS; i++)
    {
      int wheelPos = (i * 256 / NUM_LEDS + step) % 256;
      uint32_t col = wheelColor(wheelPos);
      uint8_t r = (((col >> 16) & 0xFF) * active_command.intensity) / 255;
      uint8_t g = (((col >> 8) & 0xFF) * active_command.intensity) / 255;
      uint8_t b = ((col & 0xFF) * active_command.intensity) / 255;
      strip->setPixelColor(i, strip->Color(r, g, b));
    }
    strip->show();
    break;
  }

  case Effect::kColorPulseRipple: // Kanal 37
  {
    // Parameter
    int center = NUM_LEDS / 2;
    // Die Wellenfront breitet sich mit konstanter Geschwindigkeit aus;
    // Der aktuelle Radius errechnet sich aus dem globalen Schrittzähler.
    float waveSpeed = 0.5; // Erweitert um (LEDs pro Update)
    float waveRadius = fmod(step * waveSpeed, center);
    float waveWidth = 3.0; // Breite der Wellenfront in LED-Einheiten

    // Optional: Zuerst den Streifen komplett schwarz machen, falls kein Überbleibsel erwünscht ist:
    // strip->clear();

    for (int i = 0; i < NUM_LEDS; i++)
    {
      // Abstand vom Zentrum
      float dist = fabs(i - center);
      // Prüfe, ob der LED nahe der aktuellen Wellenfront liegt
      float delta = fabs(dist - waveRadius);
      if (delta < waveWidth)
      {
        // Helligkeit sinkt, je weiter weg von der Wellenfront
        float brightness = (1.0 - (delta / waveWidth)) * (active_command.intensity / 255.0);
        uint8_t r = (uint8_t)(active_command.red * brightness);
        uint8_t g = (uint8_t)(active_command.green * brightness);
        uint8_t b = (uint8_t)(active_command.blue * brightness);
        strip->setPixelColor(i, strip->Color(r, g, b));
      }
      else
      {
        // LED bleibt aus – alternativ: kleine Resthelligkeit oder sanftes Ausblenden
        // strip->setPixelColor(i, 0);
      }
    }
    strip->show();
    break;
  }

  case Effect::kMeteor:
  { // effect 38
    strip->clear();
    int meteorLength = active_command.length > 0 ? active_command.length : 4;
    int gapLength = meteorLength;
    float fadeRate = 0.8;
    for (int j = 0; j < NUM_LEDS; j += (meteorLength + gapLength))
    {
      for (int i = 0; i < meteorLength; i++)
      {
        int pos = (step - i + j + NUM_LEDS) % NUM_LEDS;
        float fade = pow(fadeRate, i);
        uint32_t color = strip->Color((active_command.red * active_command.intensity * fade) / 255,
                                      (active_command.green * active_command.intensity * fade) / 255,
                                      (active_command.blue * active_command.intensity * fade) / 255);
        strip->setPixelColor(pos, color);
      }
    }
    // Zufälliges Funkeln im Schweif:
    for (int i = 0; i < NUM_LEDS; i++)
    {
      if (random(10) == 0)
      {
        uint32_t curCol = strip->getPixelColor(i);
        uint8_t r = ((curCol >> 16) & 0xFF) * 0.7;
        uint8_t g = ((curCol >> 8) & 0xFF) * 0.7;
        uint8_t b = (curCol & 0xFF) * 0.7;
        strip->setPixelColor(i, strip->Color(r, g, b));
      }
    }
    strip->show();
    break;
  }

  case Effect::kFlicker:
  { // effect 39 (Flackern, Kerzenlicht)
    float baseInt = 0.7 + (random(30) / 100.0);
    float flickerInt = baseInt + ((random(40) - 20) / 100.0);
    if (flickerInt < 0)
      flickerInt = 0;
    if (flickerInt > 1)
      flickerInt = 1;
    uint32_t color = strip->Color((active_command.red * active_command.intensity * flickerInt) / 255,
                                  (active_command.green * active_command.intensity * flickerInt * 0.85) / 255,
                                  (active_command.blue * active_command.intensity * flickerInt * 0.7) / 255);
    for (int i = 0; i < NUM_LEDS; i++)
    {
      strip->setPixelColor(i, color);
    }
    strip->show();
    break;
  }

  case Effect::kNeonComet:
  { // Kanal 40
    // Parameter
    int cometLength = active_command.length > 0 ? active_command.length : 10; // Länge des Kometenschweifs
    float tailFadeFactor = 0.8;                                               // Bestimmt, wie schnell der Schweif ausfadet

    // Bestimme die aktuelle Kopfposition anhand des globalen Schrittzählers (zyklisch über den Streifen)
    int headPos = step % NUM_LEDS;
    // Zuerst den Streifen leeren
    strip->clear();

    // Zeichne den Kometenschweif
    for (int i = 0; i < cometLength; i++)
    {
      // Berechne die Position entlang des Schweifs (rückwärts vom Kopf)
      int pos = (headPos - i + NUM_LEDS) % NUM_LEDS;
      // Helligkeit nimmt exponentiell ab
      float brightness = pow(tailFadeFactor, i) * (active_command.intensity / 255.0);

      // Jedes zweite LED im Schweif erhält zusätzlich einen Neon-White-Overlay
      if (i % 2 == 0)
      {
        // Mischung: Durchschnitt zwischen der angegebenen Farbe und Weiß
        uint8_t r = (uint8_t)(((active_command.red + 255) / 2.0) * brightness);
        uint8_t g = (uint8_t)(((active_command.green + 255) / 2.0) * brightness);
        uint8_t b = (uint8_t)(((active_command.blue + 255) / 2.0) * brightness);
        strip->setPixelColor(pos, strip->Color(r, g, b));
      }
      else
      {
        uint8_t r = (uint8_t)(active_command.red * brightness);
        uint8_t g = (uint8_t)(active_command.green * brightness);
        uint8_t b = (uint8_t)(active_command.blue * brightness);
        strip->setPixelColor(pos, strip->Color(r, g, b));
      }
    }

    strip->show();
    break;
  }

  case Effect::kDoppler:
  { // Effekt 41: Doppler-Effekt
    strip->clear();
    int pulseWidth = active_command.length > 0 ? active_command.length : 5;
    float normStep = (float)step / 255.0;
    float pos = NUM_LEDS * (1.0 - cos(2 * PI * normStep)) / 2.0;
    float relSpeed = sin(2 * PI * normStep);
    float colorShift = (relSpeed + 1.0) / 2.0;
    for (int i = 0; i < NUM_LEDS; i++)
    {
      float distance = fabs(i - pos);
      if (distance < pulseWidth)
      {
        float intens = (1.0 - distance / pulseWidth) * active_command.intensity / 255.0;
        uint32_t color = strip->Color(
            (uint8_t)(active_command.red * intens * (1.0 - colorShift)),
            (uint8_t)(active_command.green * intens),
            (uint8_t)(active_command.blue * intens * colorShift));
        strip->setPixelColor(i, color);
      }
    }
    strip->show();
    break;
  }

  case Effect::kFirework:
  { // Effekt 42
    // Parameter des Effekts
    const float fadeFactor = 0.85;                // Wie stark der gesamte Streifen bei jedem Update ausgeblendet wird
    const unsigned long explosionInterval = 1000; // Alle 1000 ms (1 Sekunde) wird ein neues Feuerwerk gestartet
    const unsigned long lifetime = 800;           // Lebensdauer eines Feuerwerks (in ms)
    const unsigned long flashDuration = 100;      // Dauer des weißen Blitzes am Zentrum (in ms)
    const int sparkRadius = 5;                    // Reichweite (in LED-Abständen) der Funken vom Explosionszentrum

    unsigned long currentTime = millis();

    // 1. Gesamten LED-Streifen leicht abdunkeln (Fade)
    for (int i = 0; i < NUM_LEDS; i++)
    {
      uint32_t currColor = strip->getPixelColor(i);
      uint8_t r = (uint8_t)(((currColor >> 16) & 0xFF) * fadeFactor);
      uint8_t g = (uint8_t)(((currColor >> 8) & 0xFF) * fadeFactor);
      uint8_t b = (uint8_t)((currColor & 0xFF) * fadeFactor);
      strip->setPixelColor(i, strip->Color(r, g, b));
    }

    // 2. Verwaltung mehrerer gleichzeitiger Feuerwerke
    // Definiere dazu eine einfache Struktur und ein statisches Array
    const int MAX_FIREWORKS = 5; // Maximale Anzahl gleichzeitiger Feuerwerke
    struct Firework
    {
      bool active;
      int center;              // LED-Index des Explosionszentrums
      unsigned long startTime; // Zeitpunkt der Explosion (in ms)
    };
    static Firework fireworks[MAX_FIREWORKS];
    static bool initializedFireworks = false;
    if (!initializedFireworks)
    {
      for (int i = 0; i < MAX_FIREWORKS; i++)
      {
        fireworks[i].active = false;
      }
      initializedFireworks = true;
    }

    // 3. Alle explosionInterval ms ein neues Feuerwerk starten
    static unsigned long lastExplosionTime = 0;
    if (currentTime - lastExplosionTime >= explosionInterval)
    {
      lastExplosionTime = currentTime;
      // Finde einen freien Slot oder, falls keiner frei ist, überschreibe den ältesten
      int slot = -1;
      for (int i = 0; i < MAX_FIREWORKS; i++)
      {
        if (!fireworks[i].active)
        {
          slot = i;
          break;
        }
      }
      if (slot == -1)
      {
        slot = 0; // alternativ: den mit dem ältesten startTime wählen
      }
      fireworks[slot].active = true;
      fireworks[slot].center = random(NUM_LEDS);
      fireworks[slot].startTime = currentTime;
    }

    // 4. Alle aktiven Feuerwerke abarbeiten
    for (int i = 0; i < MAX_FIREWORKS; i++)
    {
      if (fireworks[i].active)
      {
        unsigned long elapsed = currentTime - fireworks[i].startTime;
        if (elapsed > lifetime)
        {
          // Dieses Feuerwerk ist abgelaufen
          fireworks[i].active = false;
          continue;
        }
        // Für alle LEDs im Bereich [center - sparkRadius, center + sparkRadius]
        int startLED = max(0, fireworks[i].center - sparkRadius);
        int endLED = min(NUM_LEDS - 1, fireworks[i].center + sparkRadius);
        for (int j = startLED; j <= endLED; j++)
        {
          int distance = abs(j - fireworks[i].center);
          // Basisfaktor: Je weiter weg und je länger vergangen, desto geringer die Intensität
          float factor = (1.0f - (float)distance / sparkRadius) * (1.0f - (float)elapsed / lifetime);
          // Füge einen zufälligen "Flacker"-Faktor hinzu (zwischen 0.8 und 1.2)
          float flicker = (random(80, 121)) / 100.0f;
          float brightness = factor * flicker;
          if (brightness > 1.0f)
            brightness = 1.0f;

          // Am Zentrum: Weißer Blitz (nur während flashDuration)
          if (j == fireworks[i].center && elapsed < flashDuration)
          {
            float whiteIntensity = 1.0f - (float)elapsed / flashDuration; // Lässt den Blitz schnell ausklingen
            uint8_t addR = (uint8_t)(255 * whiteIntensity);
            uint8_t addG = (uint8_t)(255 * whiteIntensity);
            uint8_t addB = (uint8_t)(255 * whiteIntensity);
            // Addiere diesen Wert zum bereits vorhandenen LED-Farbwert (additive Blendung)
            uint32_t curr = strip->getPixelColor(j);
            uint8_t currR = (curr >> 16) & 0xFF;
            uint8_t currG = (curr >> 8) & 0xFF;
            uint8_t currB = curr & 0xFF;
            uint8_t newR = min(255, currR + addR);
            uint8_t newG = min(255, currG + addG);
            uint8_t newB = min(255, currB + addB);
            strip->setPixelColor(j, strip->Color(newR, newG, newB));
          }
          else
          {
            // Für Funken: Abwechselnd rot und blau (abhängig von der Position relativ zum Zentrum)
            uint8_t addR = 0, addG = 0, addB = 0;
            if ((j - fireworks[i].center) % 2 == 0)
            {
              // Roter Funke
              addR = (uint8_t)(255 * brightness);
            }
            else
            {
              // Blauer Funke
              addB = (uint8_t)(255 * brightness);
            }
            // Addiere diese Werte zu der aktuellen LED-Farbe
            uint32_t curr = strip->getPixelColor(j);
            uint8_t currR = (curr >> 16) & 0xFF;
            uint8_t currG = (curr >> 8) & 0xFF;
            uint8_t currB = curr & 0xFF;
            uint8_t newR = min(255, currR + addR);
            uint8_t newG = min(255, currG + addG);
            uint8_t newB = min(255, currB + addB);
            strip->setPixelColor(j, strip->Color(newR, newG, newB));
          }
        }
      }
    }

    strip->show();
    step++; // optional, falls der globale Schrittzähler benötigt wird
    break;
  }

  case Effect::kDNAHelix:
  { // Effekt 43: DNA-Helix
    float wavelength = active_command.length > 0 ? active_command.length : (float)NUM_LEDS / 2;
    // Hier verwenden wir eine moderate Phasenverschiebung für einen sanften Effekt:
    float phaseVal = 2.0 * PI * (step / 255.0);
    for (int i = 0; i < NUM_LEDS; i++)
    {
      float wave1 = sin(2.0 * PI * i / wavelength + phaseVal);
      float wave2 = sin(2.0 * PI * i / wavelength + phaseVal + PI);
      float amp1 = (wave1 + 1.0) * 0.5;
      float amp2 = (wave2 + 1.0) * 0.5;
      uint32_t color = strip->Color(
          (uint8_t)((active_command.red * active_command.intensity * amp1) / 255),
          (uint8_t)((active_command.green * active_command.intensity) / 255),
          (uint8_t)((active_command.blue * active_command.intensity * amp2) / 255));
      strip->setPixelColor(i, color);
    }
    strip->show();
    break;
  }

  case Effect::kDNAHelixColorWhite:
  { // Effekt 44
    // Bestimme die Wellenlänge (Standard: NUM_LEDS/2, falls keine Länge angegeben)
    float wavelength = active_command.length > 0 ? active_command.length : (float)NUM_LEDS / 2;
    // Bestimme eine Phasenverschiebung basierend auf step (0..255)
    float phaseVal = 2.0 * PI * ((float)step / 255.0);
    for (int i = 0; i < NUM_LEDS; i++)
    {
      // Berechne einen Mischfaktor zwischen 0 und 1 für die Interpolation
      float t = (sin(2.0 * PI * i / wavelength + phaseVal) + 1.0) / 2.0;
      // Interpoliere zwischen Weiss (bei t==0) und der übergebenen Farbe (bei t==1)
      uint8_t r = (uint8_t)(((1.0 - t) * 255 + t * active_command.red) * active_command.intensity / 255.0);
      uint8_t g = (uint8_t)(((1.0 - t) * 255 + t * active_command.green) * active_command.intensity / 255.0);
      uint8_t b = (uint8_t)(((1.0 - t) * 255 + t * active_command.blue) * active_command.intensity / 255.0);
      strip->setPixelColor(i, strip->Color(r, g, b));
    }
    strip->show();
    break;
  }

  case Effect::kBlink:
  { // effect 0x69
    if (active_command.length >= NUM_LEDS)
      break;
    bool isOn = (step % 2 == 0);
    if (isOn)
    {
      strip->setPixelColor(active_command.length, strip->Color((active_command.red * active_command.intensity) / 255,
                                                               (active_command.green * active_command.intensity) / 255,
                                                               (active_command.blue * active_command.intensity) / 255));
    }
    else
    {
      strip->setPixelColor(active_command.length, 0);
    }
    strip->show();
    break;
  }

  case Effect::kSingle:
  { // effect 0x70
    if (active_command.length >= NUM_LEDS)
      break;
    strip->clear();
    uint32_t color = strip->Color((active_command.red * active_command.intensity) / 255,
                                  (active_command.green * active_command.intensity) / 255,
                                  (active_command.blue * active_command.intensity) / 255);
    strip->setPixelColor(active_command.length, color);
    strip->show();
    break;
  }

  default:
    TurnOffLeds();
    break;
  } // end switch
}
