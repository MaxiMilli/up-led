#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "constants.h"
#include "config.h"

// Broadcast address for ESP-NOW
const uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Button state tracking
bool buttonPressed[kNumButtons] = {false};
uint32_t lastDebounceTime[kNumButtons] = {0};
uint32_t lastStrobeTime = 0;
uint16_t sequenceNumber = 0;

// Track which groups are currently active (buttons held)
uint16_t activeGroups = 0;

// Track current random group for each button (so it stays the same while held)
uint16_t currentRandomGroup[kNumButtons] = {0};

// Debug: periodic state dump
uint32_t lastDebugTime = 0;
constexpr uint32_t kDebugIntervalMs = 5000;  // Every 5 seconds

/**
 * @brief Get a random group from the pool
 */
uint16_t getRandomGroup()
{
    return kRandomGroupPool[random(kRandomGroupPoolSize)];
}

/**
 * @brief Log message to Serial
 */
void logMessage(const char *message)
{
    Serial.print("[APPLAUS] ");
    Serial.println(message);
}

/**
 * @brief Log message with format
 */
void logMessageF(const char *format, ...)
{
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    Serial.print("[APPLAUS] ");
    Serial.println(buffer);
}

/**
 * @brief Build and send a 16-byte command frame via ESP-NOW
 */
bool sendCommand(uint8_t effect, uint16_t groups, uint8_t r, uint8_t g, uint8_t b,
                 uint16_t speed, uint8_t intensity, uint8_t flags = 0, uint8_t ttl = kDefaultTTL)
{
    uint8_t frame[kFrameSize];

    // Sequence number (big endian)
    frame[0] = (sequenceNumber >> 8) & 0xFF;
    frame[1] = sequenceNumber & 0xFF;
    sequenceNumber++;

    // Flags byte: upper 4 bits = TTL, lower 4 bits = flags
    frame[2] = MakeFlagsByte(ttl, flags);

    // Effect/Command
    frame[3] = effect;

    // Groups (big endian)
    frame[4] = (groups >> 8) & 0xFF;
    frame[5] = groups & 0xFF;

    // Duration (big endian) - 0 for continuous
    frame[6] = 0x00;
    frame[7] = 0x00;

    // Length
    frame[8] = 0x00;

    // Rainbow
    frame[9] = 0x00;

    // RGB
    frame[10] = r;
    frame[11] = g;
    frame[12] = b;

    // Speed (big endian)
    frame[13] = (speed >> 8) & 0xFF;
    frame[14] = speed & 0xFF;

    // Intensity
    frame[15] = intensity;

    esp_err_t result = esp_now_send(broadcastAddress, frame, kFrameSize);

    return result == ESP_OK;
}

/**
 * @brief Send red strobe to specified groups
 */
void sendStrobe(uint16_t groups)
{
    if (sendCommand(Cmd::kEffectStrobe, groups,
                    kStrobeR, kStrobeG, kStrobeB,
                    kStrobeSpeed, kStrobeIntensity,
                    Flag::kPriority))
    {
        logMessageF("STROBE -> groups=0x%04X", groups);
    }
    else
    {
        logMessage("STROBE send failed");
    }
}

/**
 * @brief Send blackout to all groups (multiple times for reliability)
 */
void sendBlackout()
{
    for (int i = 0; i < 4; i++)
    {
        if (sendCommand(Cmd::kStateBlackout, Group::kBroadcast,
                        0, 0, 0, 0, 0, Flag::kPriority))
        {
            logMessageF("BLACKOUT -> all (%d/4)", i + 1);
        }
        else
        {
            logMessage("BLACKOUT send failed");
        }
        delay(20);  // Small delay between sends
    }
}

/**
 * @brief ESP-NOW send callback
 */
void onDataSent(const uint8_t *macAddr, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS)
    {
        logMessage("ESP-NOW send failed");
    }
}

/**
 * @brief Initialize ESP-NOW
 */
bool initEspNow()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (kLongRangeEnabled)
    {
        esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
        logMessage("Long Range mode enabled");
    }

    esp_wifi_set_max_tx_power(kTxPowerDbm * 4);
    esp_wifi_set_channel(kWifiChannel, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK)
    {
        logMessage("ESP-NOW init failed");
        return false;
    }

    esp_now_register_send_cb(onDataSent);

    // Add broadcast peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = kWifiChannel;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        logMessage("Failed to add broadcast peer");
        return false;
    }

    return true;
}

/**
 * @brief Initialize all buttons as INPUT_PULLUP
 */
void initButtons()
{
    for (uint8_t i = 0; i < kNumButtons; i++)
    {
        uint8_t pin = kButtonPins[i];

        // Skip disabled buttons (pin = 255)
        if (pin == 255)
        {
            buttonPressed[i] = false;
            logMessageF("Button %d DISABLED", i);
            continue;
        }

        pinMode(pin, INPUT_PULLUP);
        buttonPressed[i] = false;
        lastDebounceTime[i] = 0;

        // Read initial state
        int initialState = digitalRead(pin);
        logMessageF("Button %d on GPIO %d -> groups=0x%04X (init=%s)",
                    i, pin, kButtonGroups[i],
                    initialState == LOW ? "PRESSED" : "released");
    }
}

/**
 * @brief Check all buttons and handle state changes
 */
void processButtons()
{
    uint32_t now = millis();
    bool anyButtonPressed = false;
    uint16_t newActiveGroups = 0;

    for (uint8_t i = 0; i < kNumButtons; i++)
    {
        // Skip disabled buttons
        if (kButtonPins[i] == 255)
            continue;

        // Read button (LOW = pressed with INPUT_PULLUP)
        bool currentState = (digitalRead(kButtonPins[i]) == LOW);

        // Debounce
        if (currentState != buttonPressed[i])
        {
            if (now - lastDebounceTime[i] > kDebounceMs)
            {
                lastDebounceTime[i] = now;
                buttonPressed[i] = currentState;

                if (currentState)
                {
                    // Button just pressed
                    // If using random group, pick a new one
                    if (kButtonGroups[i] == kRandomGroup)
                    {
                        currentRandomGroup[i] = getRandomGroup();
                        logMessageF("Button %d PRESSED (GPIO %d) -> RANDOM group 0x%04X",
                                    i, kButtonPins[i], currentRandomGroup[i]);
                    }
                    else
                    {
                        logMessageF("Button %d PRESSED (GPIO %d)", i, kButtonPins[i]);
                    }
                }
                else
                {
                    // Button just released
                    logMessageF("Button %d RELEASED (GPIO %d)", i, kButtonPins[i]);
                    currentRandomGroup[i] = 0;
                }
            }
        }

        // Track active groups
        if (buttonPressed[i])
        {
            anyButtonPressed = true;
            // Use random group if configured, otherwise use configured group
            if (kButtonGroups[i] == kRandomGroup)
            {
                newActiveGroups |= currentRandomGroup[i];
            }
            else
            {
                newActiveGroups |= kButtonGroups[i];
            }
        }
    }

    // Update active groups
    activeGroups = newActiveGroups;

    // Send strobe periodically while any button is pressed
    if (anyButtonPressed)
    {
        if (now - lastStrobeTime >= kStrobeIntervalMs)
        {
            lastStrobeTime = now;
            sendStrobe(activeGroups);
        }
    }
    else if (activeGroups == 0 && now - lastStrobeTime < kStrobeIntervalMs + 100)
    {
        // All buttons released - send blackout once
        // (check timing to avoid sending blackout repeatedly)
        if (lastStrobeTime > 0)
        {
            sendBlackout();
            lastStrobeTime = 0;
        }
    }
}

void setup()
{
    Serial.begin(115200);
    delay(100);

    // Seed random number generator
    randomSeed(analogRead(0) + millis());

    logMessage("Applausmaschine starting...");
    logMessageF("Configured with %d buttons", kNumButtons);

    if (!initEspNow())
    {
        logMessage("ESP-NOW init failed, rebooting in 5s...");
        delay(5000);
        ESP.restart();
    }

    logMessage("ESP-NOW initialized");

    uint8_t mac[6];
    WiFi.macAddress(mac);
    logMessageF("MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    initButtons();

    logMessage("Ready - press buttons to trigger strobe!");
}

/**
 * @brief Print debug info about all button states
 */
void debugPrintStates()
{
    uint32_t now = millis();
    if (now - lastDebugTime < kDebugIntervalMs)
        return;

    lastDebugTime = now;

    Serial.print("[DEBUG] Pins: ");
    for (uint8_t i = 0; i < kNumButtons; i++)
    {
        if (kButtonPins[i] == 255)
        {
            Serial.print("X ");
            continue;
        }
        int state = digitalRead(kButtonPins[i]);
        Serial.printf("G%d=%d ", kButtonPins[i], state);
    }
    Serial.println();
}

void loop()
{
    processButtons();
    debugPrintStates();
    delay(1);  // Small delay to prevent tight loop
}
