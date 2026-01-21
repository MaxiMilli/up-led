#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

// Network Configuration
const char *WIFI_SSID = "uzepatscher_lichtshow";
const char *WIFI_PASSWORD = "nanohub";
const char *SERVER_IP = "hub.local";
const int SERVER_PORT = 8000;
const int TCP_SERVER_PORT = 9000;

// LED Configuration
int NUM_LEDS = 70;
const int LED_PIN = 2;
const int LED_TYPE = NEO_GRB + NEO_KHZ800;
const int BUILTIN_LED = 5; // Neue Zeile: GPIO 5 für die Status LED

// Timing Constants
const unsigned long WIFI_RETRY_DELAY = 5000;      // Initial retry delay (5s)
const unsigned long MAX_WIFI_RETRY_DELAY = 20000; // Maximum retry delay (20s)
const unsigned long COMMAND_TIMEOUT = 5000;       // Command timeout (5s)
const unsigned long TCP_RETRY_DELAY = 5000;       // 5 seconds between TCP connection attempts

String deviceColor = "";

// Device States
enum DeviceState
{
    OFF,
    STANDBY,
    ACTIVE_STANDBY,
    ACTIVE
};

// Command Structure (11 bytes)
struct Command
{
    uint8_t effect;
    uint16_t duration;
    uint8_t intensity;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t rainbow;
    uint16_t speed;
    uint8_t length;
} __attribute__((packed));

// Global Variables
DeviceState currentState = OFF;
WiFiClient client;
Adafruit_NeoPixel *strip = NULL; // Pointer statt direktem Objekt
unsigned long lastCommandTime = 0;
unsigned long currentRetryDelay = WIFI_RETRY_DELAY;
unsigned long lastTCPConnectionAttempt = 0;
int wifiRetryCount = 0;
Command currentCommand = {0};
bool isEffectRunning = false;

unsigned long lastEffectUpdate = 0; // Für Effect-Timing
int effectStep = 0;                 // Für Effect-Sequenzierung

// Konstanten für EEPROM
const int EEPROM_SIZE = 512;        // Größe des EEPROM
const int LED_NUMBER_ADDR = 0;      // Adresse für LED-Nummer
const int MAGIC_NUMBER = 0xAB;      // Magic number zur Validierung
const int MAGIC_ADDR = sizeof(int); // Adresse für magic number

// Neue Konstanten für Farben
const uint8_t COLOR_CYAN = 1;
const uint8_t COLOR_MAGENTA = 2;
const uint8_t COLOR_YELLOW = 3;

// EEPROM Adressen erweitern
const int COLOR_ADDR = sizeof(int) + 1;      // Nach LED_NUMBER_ADDR und MAGIC_ADDR
const int COLOR_MAGIC_ADDR = COLOR_ADDR + 1; // Separate Magic Number für Farbe
const uint8_t COLOR_MAGIC_NUMBER = 0xCD;     // Andere Magic Number als für LEDs

uint8_t currentStandbyColor = 0; // 0 = default (weiss)

// Function Declarations
bool connectToWiFi();
bool connectToServer();
void handleCommand();
void updateLEDs();
void setStandbyMode();
void setActiveStandbyMode();
void processCommand(const Command &cmd);
void runEffect(const Command &cmd);
void debugPrintCommand(const Command &cmd);
bool connectToTCPServer();
void indicateConnectionProblem();
bool readCommand(Command &cmd);
int loadLedNumber();
void saveLedNumber(int ledNumber);
uint8_t loadStandbyColor();
void saveStandbyColors(uint8_t color);

// Setup function
void setup()
{
    Serial.begin(115200);
    Serial.println("Initializing...");

    pinMode(BUILTIN_LED, OUTPUT);

    EEPROM.begin(EEPROM_SIZE);

    // LED-Nummer aus EEPROM laden
    int storedLeds = loadLedNumber();
    if (storedLeds > 0)
    {
        NUM_LEDS = storedLeds;
    }

    currentStandbyColor = loadStandbyColor();

    // Sicheres Initialisieren des Strips
    if (strip != NULL)
    {
        delete strip; // Alten Strip freigeben falls vorhanden
    }
    strip = new Adafruit_NeoPixel(NUM_LEDS, LED_PIN, LED_TYPE);
    strip->begin();
    strip->clear(); // Alle LEDs ausschalten
    strip->show();

    WiFi.mode(WIFI_STA);
    currentState = STANDBY;
    Serial.println("Initial state set to STANDBY");
}

void loop()
{
    unsigned long currentTime = millis();

    // Überprüfe WIFI-Verbindung
    if (WiFi.status() != WL_CONNECTED)
    {
        if (!connectToWiFi())
        {
            if (currentState != STANDBY)
            {
                setStandbyMode();
            }
            // Warte bis zum nächsten Verbindungsversuch, aber führe updateLEDs weiter aus
            delay(currentRetryDelay);
        }
    }
    else
    {
        // WIFI verbunden, überprüfe TCP
        if (!client.connected())
        {
            if (currentState == ACTIVE || currentState == ACTIVE_STANDBY)
            {
                strip->clear();
                strip->show();
                setStandbyMode();
            }

            // Versuche TCP-Verbindung in regelmäßigen Abständen
            if (currentTime - lastTCPConnectionAttempt >= TCP_RETRY_DELAY)
            {
                lastTCPConnectionAttempt = currentTime;
                if (connectToTCPServer())
                {
                    setActiveStandbyMode();
                }
            }
        }
        else
        {
            // Prüfe auf neue Kommandos
            if (client.available() >= sizeof(Command))
            {
                Command cmd;
                if (readCommand(cmd))
                {
                    Serial.println("Command received successfully!");
                    processCommand(cmd);
                    lastCommandTime = millis();
                    debugPrintCommand(cmd);
                }
            }

            // Prüfe auf Verbindungsprobleme
            if (!client.connected())
            {
                Serial.println("TCP connection lost");
                client.stop();
                setStandbyMode();
            }
        }
    }

    // Update LEDs mit Rate-Limiting
    static unsigned long lastLoopUpdate = 0;

    // Nur alle 20ms updaten (50fps)
    if (currentTime - lastLoopUpdate >= 20)
    {
        updateLEDs();
        lastLoopUpdate = currentTime;
    }
}

// WiFi connection function
bool connectToWiFi()
{
    Serial.print("Connecting to WiFi... (Attempt ");
    Serial.print(wifiRetryCount + 1);
    Serial.println(")");

    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        unsigned long startAttemptTime = millis();

        while (WiFi.status() != WL_CONNECTED)
        {
            if (millis() - startAttemptTime > currentRetryDelay)
            {
                Serial.println("Failed!");
                wifiRetryCount++;
                if (wifiRetryCount >= 20)
                {
                    currentRetryDelay = MAX_WIFI_RETRY_DELAY;
                }
                else
                {
                    currentRetryDelay = WIFI_RETRY_DELAY;
                }
                indicateConnectionProblem();
                return false;
            }
            delay(100);
            Serial.print(".");
        }

        wifiRetryCount = 0;
        currentRetryDelay = WIFI_RETRY_DELAY;
        Serial.println("Connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    }

    return true;
}

bool registerDevice()
{
    // WiFiClient httpClient;  // Separate Client für HTTP
    // Serial.print("Connecting to HTTP server for registration...");

    // if (!httpClient.connect(SERVER_IP, SERVER_PORT)) {  // PORT 8000
    //     Serial.println("Failed!");
    //     return false;
    // }
    // Serial.println("Connected!");

    // String macAddress = WiFi.macAddress();
    // String registerMsg = "{\"mac\":\"" + macAddress + "\"}";
    // Serial.println("Registration message: " + registerMsg);

    // String request = "POST /nano/register HTTP/1.1\r\n";
    // request += "Host: " + String(SERVER_IP) + ":" + String(SERVER_PORT) + "\r\n";
    // request += "Content-Type: application/json\r\n";
    // request += "Content-Length: " + String(registerMsg.length()) + "\r\n";
    // request += "\r\n" + registerMsg;

    // httpClient.print(request);
    // httpClient.flush();

    // Wait for response
    // unsigned long timeout = millis();
    // while (httpClient.available() == 0) {
    //     if (millis() - timeout > 5000) {
    //         Serial.println("Response Timeout!");
    //         httpClient.stop();
    //         return false;
    //     }
    // }

    // // Skip headers
    // while (httpClient.available()) {
    //     String line = httpClient.readStringUntil('\n');
    //     if (line == "\r") break;
    // }

    // String response = httpClient.readString();
    // httpClient.stop();  // Close HTTP connection
    // Serial.println("Response: " + response);

    // if (response.indexOf("\"status\":\"success\"") != -1) {
    //     if (response.indexOf("\"color\":\"red\"") != -1) {
    //         deviceColor = "RED";
    //     } else if (response.indexOf("\"color\":\"green\"") != -1) {
    //         deviceColor = "GREEN";
    //     } else if (response.indexOf("\"color\":\"blue\"") != -1) {
    //         deviceColor = "BLUE";
    //     } else {
    //         Serial.println("Unknown color in response!");
    //         return false;
    //     }
    //     Serial.println("Registration successful! Color: " + deviceColor);
    //     return true;
    // }

    // Serial.println("Registration failed!");
    return true;
}

bool connectToTCPServer()
{
    if (!client.connected())
    {
        Serial.print("Connecting to TCP server...");
        client.setTimeout(5000); // 5 Sekunden Timeout
        if (!client.connect(SERVER_IP, TCP_SERVER_PORT))
        {
            Serial.println("Failed!");
            return false;
        }
        String mac = WiFi.macAddress();
        // Sende MAC-Adresse mit korrektem Format
        client.print("register--" + mac);

        // // Warte auf Bestätigung (optional)
        // unsigned long timeout = millis();
        // while (client.available() == 0) {
        //     if (millis() - timeout > 5000) {
        //         Serial.println("Registration timeout!");
        //         client.stop();
        //         return false;
        //     }
        // }

        Serial.println("Connected and registered!");
    }
    return true;
}

// Debug print function for commands
void debugPrintCommand(const Command &cmd)
{
    Serial.println("\nReceived Command:");
    Serial.printf("Effect: %d\n", cmd.effect);
    Serial.printf("Duration: %d\n", cmd.duration);
    Serial.printf("Intensity: %d\n", cmd.intensity);
    Serial.printf("RGB: (%d,%d,%d)\n", cmd.red, cmd.green, cmd.blue);
    Serial.printf("Rainbow: %d\n", cmd.rainbow);
    Serial.printf("Speed: %d\n", cmd.speed);
    Serial.printf("Length: %d\n", cmd.length);
}

// Process received command
void processCommand(const Command &cmd)
{
    Serial.printf("Processing command with effect: %d\n", cmd.effect);
    switch (cmd.effect)
    {
    case 0x64: // OFF (100)
        Serial.println("Processing OFF command");
        currentState = OFF;
        strip->clear();
        strip->show();
        isEffectRunning = false;
        break;

    case 0x65: // STANDBY (101)
        Serial.println("Processing STANDBY command");
        setStandbyMode();
        isEffectRunning = false;
        break;

    case 0x66: // ACTIVE_STANDBY (102)
        Serial.println("Processing ACTIVE_STANDBY command");
        setActiveStandbyMode();
        isEffectRunning = false;
        break;

    case 0x67: // RGB (103)
        Serial.println("Processing RGB command");
        currentState = ACTIVE;
        currentCommand = cmd;
        isEffectRunning = true;
        lastEffectUpdate = millis();
        effectStep = 0;
        break;

    case 0x68: // RAINBOW (104)
        Serial.println("Processing RAINBOW command");
        currentState = ACTIVE;
        currentCommand = cmd;
        isEffectRunning = true;
        lastEffectUpdate = millis();
        effectStep = 0;
        break;

    case 0x69: // BLINK (105)
        Serial.println("Processing BLINK command");
        currentState = ACTIVE;
        currentCommand = cmd;
        isEffectRunning = true;
        lastEffectUpdate = millis();
        effectStep = 0;
        break;

    case 0x70: // SINGLE (106)
        Serial.println("Processing SINGLE command");
        currentState = ACTIVE;
        currentCommand = cmd;
        isEffectRunning = true;
        lastEffectUpdate = millis();
        effectStep = 0;
        break;

    case 0x71: // ADD_SINGLE (107)
        Serial.println("Processing ADD_SINGLE command");
        // Hier prüfen wir, ob das vorherige Kommando SINGLE oder ADD_SINGLE war
        if (currentCommand.effect != 0x70 && currentCommand.effect != 0x71)
        {
            Serial.println("Error: Previous command was not SINGLE or ADD_SINGLE");
            return;
        }
        currentState = ACTIVE;
        currentCommand = cmd;
        isEffectRunning = true;
        lastEffectUpdate = millis();
        break;

    default:
        // Reguläre Effekte (30-45)
        if (cmd.effect >= 30 && cmd.effect <= 45)
        {
            Serial.printf("Processing effect command: %d\n", cmd.effect);
            currentState = ACTIVE;
            currentCommand = cmd;
            isEffectRunning = true;
            lastEffectUpdate = millis();
            effectStep = 0;
        }
        else
        {
            Serial.printf("Unknown command effect: %d\n", cmd.effect);
            isEffectRunning = false;       // Stop any running effect
            currentState = ACTIVE_STANDBY; // Return to safe state
        }
        break;
    }
}

// Standby Animation Variables
const uint8_t STANDBY_BRIGHTNESS = 10;  // 10% of 255
const unsigned long FADE_INTERVAL = 30; // Update every 30ms for smooth animation
const float FADE_SPEED = 0.02;          // Speed of the fade animation
unsigned long lastFadeUpdate = 0;
float fadeValue = 0;
bool fadeDirection = true;
uint8_t standbyColorMode = 0; // 0=R, 1=G, 2=B
unsigned long lastColorChange = 0;
const unsigned long COLOR_CHANGE_INTERVAL = 30000; // Change color every 30 seconds

// Helper function to get current standby color
void getStandbyColor(uint8_t mode, uint8_t brightness, uint8_t &r, uint8_t &g, uint8_t &b)
{
    switch (mode)
    {
    case 0: // Red theme
        r = brightness;
        g = brightness / 8;
        b = brightness / 8;
        break;
    case 1: // Green theme
        r = brightness / 8;
        g = brightness;
        b = brightness / 8;
        break;
    case 2: // Blue theme
        r = brightness / 8;
        g = brightness / 8;
        b = brightness;
        break;
    }
}

// Set device to standby mode with energy-saving animation
void setStandbyMode()
{
    currentState = STANDBY;
    fadeValue = 0;
    fadeDirection = true;
    lastFadeUpdate = millis();
    lastColorChange = millis();
    standbyColorMode = random(3); // Start with random color

    Serial.println("Entering STANDBY mode");
}

// Set device to active standby mode with static dim white light
void setActiveStandbyMode()
{
    currentState = ACTIVE_STANDBY;
    // Set all LEDs to 10% white
    for (int i = 0; i < NUM_LEDS; i++)
    {
        strip->setPixelColor(i, strip->Color(STANDBY_BRIGHTNESS, STANDBY_BRIGHTNESS, STANDBY_BRIGHTNESS));
    }
    strip->show();

    Serial.println("Entering ACTIVE_STANDBY mode");
}

// Update LED strip based on current state and effect
void updateLEDs()
{
    unsigned long currentTime = millis();

    switch (currentState)
    {
    case OFF:
        strip->clear();
        strip->show();
        break;

    case STANDBY:
        // Nur update animation bei FADE_INTERVAL
        if (currentTime - lastFadeUpdate >= FADE_INTERVAL)
        {
            // Farbe basierend auf gespeichertem Wert
            uint8_t r, g, b;
            switch (currentStandbyColor)
            {
            case COLOR_CYAN:
                r = 0;
                g = 255;
                b = 255;
                break;
            case COLOR_MAGENTA:
                r = 255;
                g = 0;
                b = 255;
                break;
            case COLOR_YELLOW:
                r = 255;
                g = 255;
                b = 0;
                break;
            default: // Weiss
                r = 255;
                g = 255;
                b = 255;
                break;
            }

            // Lauflicht-Muster mit 3 LEDs
            int basePos = (currentTime / 1000) % 8; // 8 Positionen

            for (int i = 0; i < NUM_LEDS; i++)
            {
                int position = (i - basePos + 8) % 8;

                uint32_t color;
                switch (position)
                {
                case 0: // 20% Helligkeit
                    color = strip->Color(
                        max((r * 20) / 100, (r * 2) / 100),
                        max((g * 20) / 100, (g * 2) / 100),
                        max((b * 20) / 100, (b * 2) / 100));
                    break;
                case 1: // 10% Helligkeit
                    color = strip->Color(
                        max((r * 10) / 100, (r * 2) / 100),
                        max((g * 10) / 100, (g * 2) / 100),
                        max((b * 10) / 100, (b * 2) / 100));
                    break;
                case 2: // 5% Helligkeit
                    color = strip->Color(
                        max((r * 5) / 100, (r * 2) / 100),
                        max((g * 5) / 100, (g * 2) / 100),
                        max((b * 5) / 100, (b * 2) / 100));
                    break;
                default: // Minimale Helligkeit (2%)
                    color = strip->Color(
                        (r * 2) / 100,
                        (g * 2) / 100,
                        (b * 2) / 100);
                    break;
                }
                strip->setPixelColor(i, color);
            }

            strip->show();
            lastFadeUpdate = currentTime;
        }
        break;

    case ACTIVE_STANDBY:
        // Active standby is static, no need to update
        break;

    case ACTIVE:
        if (isEffectRunning)
        {
            // Fügen Sie Debug-Information hinzu
            static unsigned long lastDebugTime = 0;
            unsigned long currentTime = millis();
            if (currentTime - lastDebugTime >= 1000)
            { // Nur einmal pro Sekunde
                Serial.printf("Active state with effect: %d\n", currentCommand.effect);
                lastDebugTime = currentTime;
            }
            runEffect(currentCommand);
        }
        else
        {
            Serial.println("No effect running in active state");
        }
        break;
    }
}

// Helper function to create rainbow color
uint32_t wheelColor(byte wheelPos)
{
    wheelPos = 255 - wheelPos;
    if (wheelPos < 85)
    {
        return strip->Color(255 - wheelPos * 3, 0, wheelPos * 3);
    }
    if (wheelPos < 170)
    {
        wheelPos -= 85;
        return strip->Color(0, wheelPos * 3, 255 - wheelPos * 3);
    }
    wheelPos -= 170;
    return strip->Color(wheelPos * 3, 255 - wheelPos * 3, 0);
}

// Helper function to apply intensity to a color
uint32_t applyIntensity(uint32_t color, uint8_t intensity)
{
    uint8_t r = (uint8_t)(((color >> 16) & 0xFF) * intensity / 255);
    uint8_t g = (uint8_t)(((color >> 8) & 0xFF) * intensity / 255);
    uint8_t b = (uint8_t)((color & 0xFF) * intensity / 255);
    return strip->Color(r, g, b);
}

// Helper function to get base color from command
uint32_t getBaseColor(const Command &cmd)
{
    if (cmd.rainbow > 0)
    {
        return wheelColor((millis() / 20) & 255);
    }
    return strip->Color(cmd.red, cmd.green, cmd.blue);
}

// Run the current effect
void runEffect(const Command &cmd)
{
    unsigned long currentTime = millis();
    Serial.printf("Effect: %d, Time: %lu, Last Update: %lu, Diff: %lu, Speed: %d\n",
                  cmd.effect, currentTime, lastEffectUpdate,
                  currentTime - lastEffectUpdate, cmd.speed);

    uint32_t baseColor = getBaseColor(cmd);
    uint32_t colorWithIntensity = applyIntensity(baseColor, cmd.intensity);

    // Unified speed usage:
    if (currentTime - lastEffectUpdate < cmd.speed)
    {
        return;
    }
    lastEffectUpdate = currentTime;

    Serial.println("Updating effect...");

    switch (cmd.effect)
    {
    case 0x67:
    { // Simple RGB
        for (int i = 0; i < NUM_LEDS; i++)
        {
            strip->setPixelColor(i, colorWithIntensity);
        }
        break;
    }

    case 30:
    { // Lauflicht
        strip->clear();
        int groupSize = cmd.length > 0 ? cmd.length : 4;
        int totalPattern = groupSize * 2; // LEDs + gap

        // Calculate position based on speed (slower = more updates per pixel move)
        int position = effectStep % totalPattern;

        for (int i = 0; i < NUM_LEDS; i++)
        {
            int patternPos = (i + position) % totalPattern;
            if (patternPos < groupSize)
            {
                float center = (groupSize - 1) / 2.0;
                float distance = fabs(patternPos - center);
                float maxDistance = center;

                float brightnessFactor = 1.0 - (distance / maxDistance) * 0.5; // mind. 50% Helligkeit am Rand

                uint32_t color = strip->Color(
                    (cmd.red * cmd.intensity * brightnessFactor) / 255,
                    (cmd.green * cmd.intensity * brightnessFactor) / 255,
                    (cmd.blue * cmd.intensity * brightnessFactor) / 255);
                strip->setPixelColor(i, color);
            }
        }
        break;
    }

    case 31:
    { // Glitter with speed control
        Serial.println("Running Glitter effect");
        // Only update based on speed
        if ((effectStep % (cmd.speed > 0 ? cmd.speed : 1)) == 0)
        {
            for (int i = 0; i < NUM_LEDS; i++)
            {
                if (random(10) == 0)
                { // 10% chance for each LED to glitter
                    float randomIntensity = (0.6 + (float)random(40) / 100.0);
                    uint32_t color = strip->Color(
                        (cmd.red * cmd.intensity * randomIntensity) / 255,
                        (cmd.green * cmd.intensity * randomIntensity) / 255,
                        (cmd.blue * cmd.intensity * randomIntensity) / 255);
                    strip->setPixelColor(i, color);
                }
                else
                {
                    strip->setPixelColor(i, 0);
                }
            }
        }
        break;
    }

    case 32:
    { // Wave with edge dimming
        strip->clear();
        int groupSize = cmd.length > 0 ? cmd.length : 4;
        int totalPattern = groupSize * 2; // LEDs + gap

        // Calculate position based on speed
        int position = effectStep % totalPattern;

        for (int i = 0; i < NUM_LEDS; i++)
        {
            int patternPos = (i + position) % totalPattern;
            if (patternPos < groupSize)
            {
                // Calculate dimming factor for edge LEDs
                float dimFactor = 1.0;
                if (patternPos == 0 || patternPos == groupSize - 1)
                {
                    dimFactor = 0.5; // 50% brightness at edges
                }

                uint32_t color = strip->Color(
                    (cmd.red * cmd.intensity * dimFactor) / 255,
                    (cmd.green * cmd.intensity * dimFactor) / 255,
                    (cmd.blue * cmd.intensity * dimFactor) / 255);
                strip->setPixelColor(i, color);
            }
        }
        break;
    }

    case 33:
    { // Pulsate
        // Berechne einen Puls-Wert zwischen 0 und 1
        float phase = (float)effectStep / 12.75 * 2.0 * PI;
        float pulse = 0.4 + 0.6 * ((sin(phase) + 1.0) / 2.0);

        // Debug-Ausgabe für die Puls-Werte
        Serial.printf("Pulse value: %.2f, Step: %d\n", pulse, effectStep);

        // Berechne die Intensität basierend auf dem Puls
        uint8_t pulseIntensity = (uint8_t)(pulse * cmd.intensity);
        Serial.printf("Pulse intensity: %d\n", pulseIntensity);

        // Setze alle LEDs auf die gepulste Farbe
        uint32_t color = strip->Color(
            (cmd.red * pulseIntensity) / 255,
            (cmd.green * pulseIntensity) / 255,
            (cmd.blue * pulseIntensity) / 255);

        for (int i = 0; i < NUM_LEDS; i++)
        {
            strip->setPixelColor(i, color);
        }

        break;
    }

    case 35:
    { // Strobo
        // Bestimme den Zustand (an/aus) basierend auf der Zeit
        bool isOn = (effectStep % 2 == 0);

        // Setze alle LEDs entweder auf volle Helligkeit oder aus
        for (int i = 0; i < NUM_LEDS; i++)
        {
            if (isOn)
            {
                strip->setPixelColor(i, strip->Color(
                                            (cmd.red * cmd.intensity) / 255,
                                            (cmd.green * cmd.intensity) / 255,
                                            (cmd.blue * cmd.intensity) / 255));
            }
            else
            {
                strip->setPixelColor(i, 0);
            }
        }
        break;
    }

    case 36:
    { // RAINBOW
        // Clear previous state
        strip->clear();

        // For each LED
        for (int i = 0; i < NUM_LEDS; i++)
        {
            // Calculate color wheel position for this LED
            // Add effectStep to create movement and offset each LED position
            int wheelPos = ((effectStep * (cmd.speed * 10)) + (i * 256 / NUM_LEDS)) % 256;

            // Get color from wheel and apply intensity
            uint32_t color = wheelColor(wheelPos);
            uint8_t r = ((color >> 16) & 0xFF);
            uint8_t g = ((color >> 8) & 0xFF);
            uint8_t b = (color & 0xFF);

            // Set the LED color
            strip->setPixelColor(i, strip->Color(r, g, b));
        }

        break;
    }

    case 37:
    { // Herzschlag
        // Teile den Effekt-Zyklus in 255 Schritte
        float normalizedStep = (float)(effectStep % 255) / 255.0;
        float intensity;

        // Erstelle zwei Pulse pro Zyklus für einen realistischen Herzschlag
        if (normalizedStep < 0.15)
        { // Erster Puls (schnell hoch)
            intensity = normalizedStep / 0.15;
        }
        else if (normalizedStep < 0.3)
        { // Erster Puls (schnell runter)
            intensity = 1.0 - ((normalizedStep - 0.15) / 0.15);
        }
        else if (normalizedStep < 0.35)
        { // Kurze Pause
            intensity = 0;
        }
        else if (normalizedStep < 0.45)
        { // Zweiter Puls (schnell hoch)
            intensity = (normalizedStep - 0.35) / 0.1;
        }
        else if (normalizedStep < 0.55)
        { // Zweiter Puls (schnell runter)
            intensity = 1.0 - ((normalizedStep - 0.45) / 0.1);
        }
        else
        { // Lange Pause
            intensity = 0;
        }

        // Setze die Farbe für alle LEDs
        uint32_t color = strip->Color(
            (cmd.red * cmd.intensity * intensity) / 255,
            (cmd.green * cmd.intensity * intensity) / 255,
            (cmd.blue * cmd.intensity * intensity) / 255);

        for (int i = 0; i < NUM_LEDS; i++)
        {
            strip->setPixelColor(i, color);
        }

        break;
    }

    case 38:
    {                   // Meteor
        strip->clear(); // Lösche vorherige Anzeige

        int meteorLength = cmd.length > 0 ? cmd.length : 4; // Länge des Meteors, default 4
        int gapLength = meteorLength;                       // Länge der Lücke zwischen Meteoren
        float fadeRate = 0.8;                               // Faktor für den Schweif-Fade

        // Zeichne mehrere Meteoren mit Lücken
        for (int j = 0; j < NUM_LEDS; j += (meteorLength + gapLength))
        {
            for (int i = 0; i < meteorLength; i++)
            {
                int pos = (effectStep - i + j + NUM_LEDS) % NUM_LEDS; // Position mit Überlauf
                float fade = pow(fadeRate, i);                        // Exponentieller Fade für den Schweif

                uint32_t color = strip->Color(
                    (cmd.red * cmd.intensity * fade) / 255,
                    (cmd.green * cmd.intensity * fade) / 255,
                    (cmd.blue * cmd.intensity * fade) / 255);
                strip->setPixelColor(pos, color);
            }
        }

        // Zufälliges Funkeln im Schweif für realistischeren Effekt
        for (int i = 0; i < NUM_LEDS; i++)
        {
            if (random(10) == 0)
            { // 10% Chance für Funkeln
                uint32_t currentColor = strip->getPixelColor(i);
                uint8_t r = ((currentColor >> 16) & 0xFF) * 0.7; // 70% der aktuellen Helligkeit
                uint8_t g = ((currentColor >> 8) & 0xFF) * 0.7;
                uint8_t b = (currentColor & 0xFF) * 0.7;
                strip->setPixelColor(i, strip->Color(r, g, b));
            }
        }

        break;
    }

    case 39:
    { // Flackern (Kerzenlicht)
        // Basis-Intensität zwischen 70% und 100%
        float baseIntensity = 0.7 + (random(30) / 100.0);

        // Addiere zufällige Schwankung (-20% bis +20%)
        float flickerIntensity = baseIntensity + ((random(40) - 20) / 100.0);

        // Begrenze die Intensität auf 0-100%
        flickerIntensity = max(0.0f, min(1.0f, flickerIntensity));

        // Füge warmen Farbton hinzu (mehr Rot, weniger Blau für Kerzeneffekt)
        uint32_t color = strip->Color(
            (cmd.red * cmd.intensity * flickerIntensity) / 255,
            (cmd.green * cmd.intensity * flickerIntensity * 0.85) / 255, // Leicht reduziertes Grün
            (cmd.blue * cmd.intensity * flickerIntensity * 0.7) / 255    // Stärker reduziertes Blau
        );

        // Setze alle LEDs auf die berechnete Farbe
        for (int i = 0; i < NUM_LEDS; i++)
        {
            strip->setPixelColor(i, color);
        }
        break;
    }

    case 40:
    { // Komet
        strip->clear();

        int cometLength = cmd.length > 0 ? cmd.length : 6; // Länge des Kometen, default 6
        float brightnessFactor = 1.5;                      // Hellerer Kopf

        // Zeichne den Kometen mit hellerem Kopf
        for (int i = 0; i < cometLength; i++)
        {
            int pos = (effectStep + i + NUM_LEDS) % NUM_LEDS; // Position mit Überlauf
            float brightness = i == 0 ? brightnessFactor : 1.0 - ((float)i / cometLength);
            // Begrenzen der Helligkeit auf max 100%
            brightness = min(brightness * cmd.intensity / 255.0, 1.0);

            uint32_t color = strip->Color(
                cmd.red * brightness,
                cmd.green * brightness,
                cmd.blue * brightness);
            strip->setPixelColor(pos, color);
        }

        break;
    }

    case 41:
    { // Doppler
        strip->clear();
        int center = NUM_LEDS / 2;
        int pulseWidth = cmd.length > 0 ? cmd.length : 5; // Breite des Pulses

        // Berechne Position mit nicht-linearer Geschwindigkeit
        float normalizedStep = (float)effectStep / 255.0;
        float position = NUM_LEDS * (1.0 - cos(2 * PI * normalizedStep)) / 2.0;

        // Berechne relative Geschwindigkeit für Doppler-Effekt
        float relativeSpeed = sin(2 * PI * normalizedStep);
        float colorShift = (relativeSpeed + 1.0) / 2.0; // Normalisiert auf 0-1

        // Setze LEDs mit Doppler-Farbverschiebung
        for (int i = 0; i < NUM_LEDS; i++)
        {
            float distance = abs(i - position);
            if (distance < pulseWidth)
            {
                float intensity = (1.0 - distance / pulseWidth) * cmd.intensity / 255.0;
                // Rot wenn sich nähert, Blau wenn sich entfernt
                uint32_t color = strip->Color(
                    cmd.red * intensity * (1.0 - colorShift),
                    cmd.green * intensity,
                    cmd.blue * intensity * colorShift);
                strip->setPixelColor(i, color);
            }
        }

        break;
    }

    case 42:
    { // Feuerwerk
        static int center = NUM_LEDS / 2;
        strip->clear();

        // Berechne Explosionsphase (0-1)
        float phase = (float)effectStep / 255.0;

        // Mehrere Funken mit unterschiedlichen Positionen
        for (int spark = 0; spark < 5; spark++)
        {                                                            // 5 Hauptfunken
            int sparkCenter = center + (spark - 2) * (NUM_LEDS / 8); // Verteile Funken um das Zentrum
            float spread = phase * (NUM_LEDS / 3);                   // Größere Ausbreitung

            for (int i = 0; i < NUM_LEDS; i++)
            {
                float distance = abs(i - sparkCenter);
                if (distance <= spread)
                {
                    // Helligkeit basierend auf Distanz und Phase
                    float brightness = (1.0 - distance / spread) * (1.0 - phase);
                    // Zufällige Variation für Funkeneffekt
                    brightness *= (0.7 + (random(30) / 100.0));

                    // Additive Farbmischung für überlappende Funken
                    uint32_t existingColor = strip->getPixelColor(i);
                    uint8_t existingR = (existingColor >> 16) & 0xFF;
                    uint8_t existingG = (existingColor >> 8) & 0xFF;
                    uint8_t existingB = existingColor & 0xFF;

                    // Zufällige Farbvariation für den Schweif
                    uint8_t newR = min(255, (int)(existingR + (cmd.red * cmd.intensity * brightness) / 255));
                    uint8_t newG = min(255, (int)(existingG + (cmd.green * cmd.intensity * brightness) / 255));
                    uint8_t newB = min(255, (int)(existingB + (cmd.blue * cmd.intensity * brightness) / 255));

                    // Leichte rötliche oder bläuliche Variation
                    if (random(2) == 0)
                    {
                        newR = min(255, (int)(newR + random(30)));
                    }
                    else
                    {
                        newB = min(255, (int)(newB + random(30)));
                    }

                    strip->setPixelColor(i, strip->Color(newR, newG, newB));
                }
            }
        }

        if (effectStep >= 255)
        {                                                    // Neue Explosion
            center = random(NUM_LEDS / 4, 3 * NUM_LEDS / 4); // Zufälliges Zentrum in der mittleren Hälfte
            effectStep = 0;                                  // Reset effectStep for next cycle
        }
        break;
    }

    case 43:
    {                                                                  // DNA Helix
        float wavelength = cmd.length > 0 ? cmd.length : NUM_LEDS / 2; // Eine komplette Windung
        float phase = 2.0 * PI * (effectStep * 10) / 255.0;
        // Rotationsphase

        for (int i = 0; i < NUM_LEDS; i++)
        {
            // Berechne zwei gegenläufige Sinuswellen
            float wave1 = sin(2.0 * PI * i / wavelength + phase);
            float wave2 = sin(2.0 * PI * i / wavelength + phase + PI); // Versetzt um 180°

            // Amplitude der Wellen (0.0 - 1.0)
            float amp1 = (wave1 + 1.0) * 0.5;
            float amp2 = (wave2 + 1.0) * 0.5;

            // Setze Farben für beide Stränge
            uint32_t color = strip->Color(
                (cmd.red * cmd.intensity * amp1) / 255,
                (cmd.green * cmd.intensity) / 255, // Mittelstrang konstant
                (cmd.blue * cmd.intensity * amp2) / 255);
            strip->setPixelColor(i, color);
        }

        break;
    }

    case 0x68:
    {                                                              // Rainbow
        float wavelength = cmd.length > 0 ? cmd.length : NUM_LEDS; // Wellenlänge, default ganze Striplänge

        for (int i = 0; i < NUM_LEDS; i++)
        {
            // Berechne Position im Farbkreis (0-255)
            int hue = (int)((effectStep + (i * 256.0f * wavelength / NUM_LEDS))) % 256;

            // Hole Regenbogenfarbe und wende Intensität an
            uint32_t color = wheelColor(hue);
            uint8_t r = ((color >> 16) & 0xFF) * cmd.intensity / 255;
            uint8_t g = ((color >> 8) & 0xFF) * cmd.intensity / 255;
            uint8_t b = (color & 0xFF) * cmd.intensity / 255;

            strip->setPixelColor(i, strip->Color(r, g, b));
        }

        break;
    }

    case 0x69:
    { // Blink (einzelne LED)
        // Prüfe ob die LED-ID gültig ist
        if (cmd.length >= NUM_LEDS)
        {
            Serial.printf("BLINK: LED index %d out of range (max %d)\n", cmd.length, NUM_LEDS - 1);
            break;
        }

        // Bestimme Blink-Zustand basierend auf effectStep
        bool isOn = (effectStep % 2 == 0);

        // Setze die spezifizierte LED
        if (isOn)
        {
            strip->setPixelColor(cmd.length, strip->Color(
                                                 (cmd.red * cmd.intensity) / 255,
                                                 (cmd.green * cmd.intensity) / 255,
                                                 (cmd.blue * cmd.intensity) / 255));
        }
        else
        {
            strip->setPixelColor(cmd.length, 0); // LED aus
        }

        break;
    }

    case 0x70:
    { // SINGLE
        if (cmd.length >= NUM_LEDS)
        {
            Serial.println("SINGLE: LED index out of range");
            return;
        }
        strip->clear();
        strip->setPixelColor(cmd.length, colorWithIntensity);
        break;
    }

    case 0x71:
    { // ADD_SINGLE
        if (cmd.length >= NUM_LEDS)
        {
            Serial.println("ADD_SINGLE: LED index out of range");
            return;
        }
        strip->setPixelColor(cmd.length, colorWithIntensity);
        break;
    }
    }

    effectStep++;
    strip->show();
}

bool readCommand(Command &cmd)
{
    if (!client.available())
    {
        return false;
    }

    Serial.printf("Available bytes: %d\n", client.available());

    // Prüfe auf System-Kommando
    char firstChar = client.peek();
    Serial.printf("First character: %c (ASCII: %d)\n", firstChar, firstChar);

    if (firstChar == 'l' || firstChar == 'c')
    {
        String cmdStr = client.readStringUntil('\n');
        Serial.printf("System command received: %s\n", cmdStr.c_str());
        if (cmdStr.startsWith("led--"))
        {
            int number = cmdStr.substring(5).toInt();
            saveLedNumber(number);
        }
        else if (cmdStr.startsWith("color--"))
        {
            Serial.printf("Received color command: %s\n", cmdStr.c_str());
            uint8_t color = cmdStr.substring(7).toInt();
            saveStandbyColors(color);
        }

        // Nach System-Kommando sofort prüfen ob ein visuelles Kommando folgt
        if (client.available() >= sizeof(Command))
        {
            uint8_t nextByte = client.peek();
            // Prüfe ob nächstes Byte ein valider Effekt-Code ist
            if (nextByte >= 0x24 && nextByte <= 0x71) // Gültige Effect-Codes
            {
                uint8_t buffer[sizeof(Command)];
                size_t bytesRead = client.readBytes(buffer, sizeof(Command));
                if (bytesRead == sizeof(Command))
                {
                    cmd.effect = buffer[0];
                    cmd.duration = (buffer[1] << 8) | buffer[2];
                    cmd.intensity = buffer[3];
                    cmd.red = buffer[4];
                    cmd.green = buffer[5];
                    cmd.blue = buffer[6];
                    cmd.rainbow = buffer[7];
                    cmd.speed = (buffer[8] << 8) | buffer[9];
                    cmd.length = buffer[10];
                    return true;
                }
            }
        }

        // Nur wenn kein visuelles Kommando folgt, Buffer leeren
        while (client.available())
            client.read();
        return false;
    }

    // Wenn kein System-Kommando, prüfe auf Visual Command
    if (client.available() >= sizeof(Command))
    {
        Serial.printf("Reading visual command (%d bytes available)\n", client.available());
        uint8_t buffer[sizeof(Command)];
        size_t bytesRead = client.readBytes(buffer, sizeof(Command));

        if (bytesRead == sizeof(Command))
        {
            cmd.effect = buffer[0];
            cmd.duration = (buffer[1] << 8) | buffer[2];
            cmd.intensity = buffer[3];
            cmd.red = buffer[4];
            cmd.green = buffer[5];
            cmd.blue = buffer[6];
            cmd.rainbow = buffer[7];
            cmd.speed = (buffer[8] << 8) | buffer[9];
            cmd.length = buffer[10];
            return true;
        }
    }

    // Wenn keiner der Fälle zutrifft, Buffer leeren
    while (client.available())
        client.read();
    return false;
}

void indicateConnectionProblem()
{
    // Schnelles blinken für 3 Sekunden
    for (int i = 0; i < 6; i++)
    {
        digitalWrite(BUILTIN_LED, HIGH);
        delay(250);
        digitalWrite(BUILTIN_LED, LOW);
        delay(250);
    }
}

int loadLedNumber()
{
    // Prüfen ob valid data vorhanden
    if (EEPROM.read(MAGIC_ADDR) != MAGIC_NUMBER)
    {
        Serial.println("No valid LED number found, using default: 70");
        return 70; // Default-Wert
    }

    // LED-Nummer laden
    int number;
    EEPROM.get(LED_NUMBER_ADDR, number);
    Serial.printf("Loaded LED number from EEPROM: %d\n", number);
    return number > 0 ? number : 70; // Sicherheitscheck
}

// Modifizierte saveLedNumber() Funktion:
void saveLedNumber(int number)
{
    if (number <= 0)
    {
        Serial.println("Invalid LED number, not saving");
        return;
    }

    // Magic number speichern zur Validierung
    EEPROM.write(MAGIC_ADDR, MAGIC_NUMBER);

    // LED-Nummer speichern
    EEPROM.put(LED_NUMBER_ADDR, number);

    // Änderungen bestätigen
    EEPROM.commit();

    // Aktualisiere die aktuelle LED-Anzahl
    NUM_LEDS = number;

    // Strip sicher neu initialisieren
    if (strip != NULL)
    {
        strip->clear(); // Alle LEDs ausschalten
        strip->show();
        delete strip;
    }
    strip = new Adafruit_NeoPixel(NUM_LEDS, LED_PIN, LED_TYPE);
    strip->begin();

    Serial.printf("Saved and updated LED number to: %d\n", number);
}

void saveStandbyColors(uint8_t color)
{
    // Prüfe ob gültige Farbe
    if (color > 3)
    {
        Serial.println("Invalid color number, not saving");
        return;
    }

    // Magic number speichern zur Validierung
    EEPROM.write(COLOR_MAGIC_ADDR, COLOR_MAGIC_NUMBER);

    // Farbe speichern
    EEPROM.write(COLOR_ADDR, color);

    // Änderungen bestätigen
    EEPROM.commit();

    // Aktuelle Farbe aktualisieren
    currentStandbyColor = color;

    Serial.printf("Saved standby color: %d\n", color);
}

uint8_t loadStandbyColor()
{
    // Prüfe ob valid data vorhanden
    if (EEPROM.read(COLOR_MAGIC_ADDR) != COLOR_MAGIC_NUMBER)
    {
        Serial.println("No valid color found, using default white");
        return 0; // Default-Wert (weiss)
    }

    // Farbe laden
    uint8_t color = EEPROM.read(COLOR_ADDR);
    if (color > 3)
    {
        Serial.println("Stored color invalid, using default white");
        return 0;
    }

    Serial.printf("Loaded standby color: %d\n", color);
    return color;
}
