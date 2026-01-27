#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "constants.h"

// Broadcast address for ESP-NOW
const uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Button state tracking
bool buttonPressed[kNumButtons] = {false};
uint32_t lastDebounceTime[kNumButtons] = {0};

// Sequence number for commands
uint16_t sequenceNumber = 0;

// Active effect tracking
int8_t activeButton = -1;              // Which button triggered the current effect (-1 = none)
EffectPhase currentPhase = EffectPhase::IDLE;
uint32_t phaseStartTime = 0;
uint32_t lastCommandTime = 0;

// Demo mode tracking
bool demoActive = false;
uint8_t demoEffectIndex = 0;
uint32_t demoLastChange = 0;

// Debug
uint32_t lastDebugTime = 0;
constexpr uint32_t kDebugIntervalMs = 5000;

// Effect names for logging
const char* getEffectName(uint8_t effect)
{
    switch (effect)
    {
    case Cmd::kEffectSolid: return "Solid";
    case Cmd::kEffectBlink: return "Blink";
    case Cmd::kEffectRainbow: return "Rainbow";
    case Cmd::kEffectRainbowCycle: return "RainbowCycle";
    case Cmd::kEffectChase: return "Chase";
    case Cmd::kEffectTheaterChase: return "TheaterChase";
    case Cmd::kEffectTwinkle: return "Twinkle";
    case Cmd::kEffectFire: return "Fire";
    case Cmd::kEffectPulse: return "Pulse";
    case Cmd::kEffectWave: return "Wave";
    case Cmd::kEffectMeteor: return "Meteor";
    case Cmd::kEffectBounce: return "Bounce";
    case Cmd::kEffectScanner: return "Scanner";
    case Cmd::kEffectConfetti: return "Confetti";
    case Cmd::kEffectLightning: return "Lightning";
    case Cmd::kEffectPolice: return "Police";
    case Cmd::kEffectStacking: return "Stacking";
    case Cmd::kEffectRipple: return "Ripple";
    case Cmd::kEffectPlasma: return "Plasma";
    default: return "Unknown";
    }
}

/**
 * @brief Log message to Serial
 */
void logMessage(const char *message)
{
    Serial.print("[CROWD] ");
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

    Serial.print("[CROWD] ");
    Serial.println(buffer);
}

/**
 * @brief Build and send a 16-byte command frame via ESP-NOW
 */
bool sendCommand(uint8_t effect, uint8_t r, uint8_t g, uint8_t b,
                 uint16_t speed, uint8_t intensity, uint8_t length = 0,
                 uint8_t flags = 0, uint8_t ttl = kDefaultTTL)
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

    // Groups (big endian) - always broadcast
    frame[4] = (Group::kBroadcast >> 8) & 0xFF;
    frame[5] = Group::kBroadcast & 0xFF;

    // Duration (big endian) - 0 for continuous
    frame[6] = 0x00;
    frame[7] = 0x00;

    // Length
    frame[8] = length;

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
 * @brief Send the main effect
 */
void sendMainEffect(const ButtonEffect &effect)
{
    if (sendCommand(effect.effectType,
                    effect.effectR, effect.effectG, effect.effectB,
                    effect.effectSpeed, effect.effectIntensity, effect.effectLength,
                    Flag::kPriority))
    {
        logMessageF("%s -> RGB(%u,%u,%u) spd=%u len=%u",
                    getEffectName(effect.effectType),
                    effect.effectR, effect.effectG, effect.effectB,
                    effect.effectSpeed, effect.effectLength);
    }
}

/**
 * @brief Send solid effect
 */
void sendSolid(const ButtonEffect &effect)
{
    if (sendCommand(Cmd::kEffectSolid,
                    effect.solidR, effect.solidG, effect.solidB,
                    0, effect.solidIntensity, 0,
                    Flag::kPriority))
    {
        logMessageF("SOLID -> RGB(%u,%u,%u) int=%u%%",
                    effect.solidR, effect.solidG, effect.solidB,
                    (effect.solidIntensity * 100) / 255);
    }
}

/**
 * @brief Send blackout to all groups
 */
void sendBlackout()
{
    for (int i = 0; i < 3; i++)
    {
        if (sendCommand(Cmd::kStateBlackout, 0, 0, 0, 0, 0, 0, Flag::kPriority))
        {
            logMessageF("BLACKOUT (%d/3)", i + 1);
        }
        delay(20);
    }
}

/**
 * @brief Send a demo effect with random wild parameters
 */
void sendDemoEffect(uint8_t effectIndex)
{
    uint8_t effect = kDemoEffects[effectIndex % kDemoEffectCount];

    // Generate wild random colors
    uint8_t r = random(128, 256);
    uint8_t g = random(128, 256);
    uint8_t b = random(128, 256);

    // Random speed (fast to medium)
    uint16_t speed = random(30, 120);

    // Full intensity
    uint8_t intensity = 255;

    // Random length for effects that use it
    uint8_t length = random(3, 10);

    if (sendCommand(effect, r, g, b, speed, intensity, length, Flag::kPriority))
    {
        logMessageF("DEMO [%u/%u] %s -> RGB(%u,%u,%u) spd=%u len=%u",
                    effectIndex + 1, kDemoEffectCount,
                    getEffectName(effect), r, g, b, speed, length);
    }
}

/**
 * @brief Start demo mode
 */
void startDemo()
{
    demoActive = true;
    demoEffectIndex = 0;
    demoLastChange = millis();

    logMessage("=== DEMO MODE START ===");
    sendDemoEffect(demoEffectIndex);
}

/**
 * @brief Stop demo mode
 */
void stopDemo()
{
    if (demoActive)
    {
        logMessage("=== DEMO MODE STOP ===");
        sendBlackout();
    }
    demoActive = false;
    demoEffectIndex = 0;
}

/**
 * @brief Update demo mode - cycle effects every 2s
 */
void updateDemo()
{
    if (!demoActive)
        return;

    uint32_t now = millis();
    if (now - demoLastChange >= kDemoIntervalMs)
    {
        demoLastChange = now;
        demoEffectIndex = (demoEffectIndex + 1) % kDemoEffectCount;
        sendDemoEffect(demoEffectIndex);
    }
}

/**
 * @brief Start a new effect sequence for a button
 */
void startEffect(uint8_t buttonIndex)
{
    if (buttonIndex >= kNumButtons)
        return;

    const ButtonEffect &effect = kButtonEffects[buttonIndex];

    // Skip disabled buttons
    if (!effect.enabled)
    {
        logMessageF("Button %u has no effect configured", buttonIndex);
        return;
    }

    // Handle demo mode specially
    if (effect.isDemo)
    {
        // Stop any running timed effect
        if (activeButton >= 0)
        {
            activeButton = -1;
            currentPhase = EffectPhase::IDLE;
        }
        startDemo();
        return;
    }

    // Stop demo if running
    if (demoActive)
    {
        stopDemo();
    }

    // Handle instant effects (send once, no timing)
    if (effect.isInstant)
    {
        logMessageF("Instant effect for button %u", buttonIndex);

        // Stop any running timed effect
        if (activeButton >= 0)
        {
            activeButton = -1;
            currentPhase = EffectPhase::IDLE;
        }

        // Send the effect (multiple times for reliability)
        for (int i = 0; i < 3; i++)
        {
            sendMainEffect(effect);
            delay(20);
        }
        return;
    }

    // Timed effect sequence
    activeButton = buttonIndex;
    currentPhase = EffectPhase::PULSE;
    phaseStartTime = millis();

    logMessageF("Starting timed effect for button %u", buttonIndex);

    // Send effect 3x for reliability, then let nanos run autonomously
    for (int i = 0; i < 3; i++)
    {
        sendMainEffect(effect);
        delay(30);
    }
}

/**
 * @brief Stop the current effect and send blackout
 */
void stopEffect()
{
    if (activeButton >= 0)
    {
        logMessageF("Stopping effect for button %d", activeButton);
        sendBlackout();
    }

    activeButton = -1;
    currentPhase = EffectPhase::IDLE;
    phaseStartTime = 0;
}

/**
 * @brief Update the running effect sequence
 */
void updateEffect()
{
    if (activeButton < 0 || currentPhase == EffectPhase::IDLE)
        return;

    uint32_t now = millis();
    uint32_t elapsed = now - phaseStartTime;
    const ButtonEffect &effect = kButtonEffects[activeButton];

    switch (currentPhase)
    {
    case EffectPhase::PULSE:
        // Check if effect duration expired
        if (elapsed >= effect.effectDurationMs)
        {
            // Move to next phase
            if (effect.solidDurationMs > 0)
            {
                // Transition to solid phase
                currentPhase = EffectPhase::SOLID;
                phaseStartTime = now;
                logMessage("Phase: EFFECT -> SOLID");

                // Send solid 3x for reliability, then let nanos run
                for (int i = 0; i < 3; i++)
                {
                    sendSolid(effect);
                    delay(30);
                }
            }
            else
            {
                // No solid phase, go to blackout
                sendBlackout();
                activeButton = -1;
                currentPhase = EffectPhase::IDLE;
                logMessage("Phase: EFFECT -> BLACKOUT (done)");
            }
        }
        // No periodic resend - nanos handle the effect autonomously
        break;

    case EffectPhase::SOLID:
        // Check if solid duration expired
        if (elapsed >= effect.solidDurationMs)
        {
            // Done, send blackout
            sendBlackout();
            activeButton = -1;
            currentPhase = EffectPhase::IDLE;
            logMessage("Phase: SOLID -> BLACKOUT (done)");
        }
        // No periodic resend - nanos handle the effect autonomously
        break;

    default:
        break;
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

        if (pin == 255)
        {
            buttonPressed[i] = false;
            logMessageF("Button %d DISABLED", i);
            continue;
        }

        pinMode(pin, INPUT_PULLUP);
        buttonPressed[i] = false;
        lastDebounceTime[i] = 0;

        logMessageF("Button %d on GPIO %d initialized", i, pin);
    }
}

/**
 * @brief Check all buttons and handle state changes
 */
void processButtons()
{
    uint32_t now = millis();

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

                const ButtonEffect &effect = kButtonEffects[i];

                if (currentState)
                {
                    // Button just pressed
                    logMessageF("Button %d PRESSED (GPIO %d)", i, kButtonPins[i]);

                    // Start this button's effect (interrupts any running effect)
                    startEffect(i);
                }
                else
                {
                    // Button released
                    logMessageF("Button %d RELEASED (GPIO %d)", i, kButtonPins[i]);

                    // For demo mode: stop when released
                    if (effect.enabled && effect.isDemo && demoActive)
                    {
                        stopDemo();
                    }
                    // For timed effects: continue running on timer
                }
            }
        }
    }
}

void setup()
{
    Serial.begin(115200);
    delay(100);

    // Seed random for demo mode
    randomSeed(analogRead(0) + millis());

    logMessage("Crowdcontrol starting...");
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

    logMessage("Ready - press buttons to trigger effects!");
    logMessage("GPIO 4  (Demo): Wild effects while held (2s cycle)");
    logMessage("GPIO 23 (Drums): Slow green pulse 7s");
    logMessage("GPIO 22 (Pauken): Medium orange pulse 7s");
    logMessage("GPIO 19 (Tschinellen): Fast red pulse 7s + red solid 30s");
    logMessage("GPIO 18 (Liras): White 100%");
    logMessage("GPIO 5  (Trompeten): Pink Ripple");
    logMessage("GPIO 17 (Posaunen): Rainbow");
    logMessage("GPIO 16 (Bass): Blackout");
}

void loop()
{
    processButtons();
    updateEffect();
    updateDemo();

    // Debug output
    uint32_t now = millis();
    if (now - lastDebugTime >= kDebugIntervalMs)
    {
        lastDebugTime = now;

        if (demoActive)
        {
            logMessageF("[DEBUG] Demo mode active, effect %u/%u",
                        demoEffectIndex + 1, kDemoEffectCount);
        }
        else if (activeButton >= 0)
        {
            const char *phaseName = "?";
            switch (currentPhase)
            {
            case EffectPhase::PULSE:
                phaseName = "EFFECT";
                break;
            case EffectPhase::SOLID:
                phaseName = "SOLID";
                break;
            default:
                phaseName = "IDLE";
                break;
            }
            uint32_t remaining = 0;
            const ButtonEffect &effect = kButtonEffects[activeButton];
            uint32_t elapsed = now - phaseStartTime;

            if (currentPhase == EffectPhase::PULSE && elapsed < effect.effectDurationMs)
                remaining = effect.effectDurationMs - elapsed;
            else if (currentPhase == EffectPhase::SOLID && elapsed < effect.solidDurationMs)
                remaining = effect.solidDurationMs - elapsed;

            logMessageF("[DEBUG] Button %d, Phase: %s, Remaining: %lums",
                        activeButton, phaseName, remaining);
        }
    }

    delay(1);
}
