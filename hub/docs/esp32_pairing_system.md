# ESP32 Nano Pairing System

Dieses Dokument beschreibt die Implementierung des Pairing-Systems auf ESP32-Seite.

## Übersicht

Das System besteht aus drei Komponenten:

1. **Hub** (Python/Raspberry Pi) - Zentrale Steuerung
2. **Gateway-ESP32** (USB-Serial) - Brücke zwischen Hub und ESP-NOW
3. **Nano-ESP32** (LED-Controller) - Empfängt Befehle, zeigt LEDs an

## Kommunikationsprotokoll

### Hub → Gateway → Nano (Broadcast Commands)

Standard 18-Byte Frame für Effekt-Befehle:

```
[0xAA][PAYLOAD 16 Bytes][CHECKSUM]
```

### Hub → Gateway → Nano (Config an einzelnen Nano)

24-Byte Frame für Konfigurationsbefehle:

```
Byte 0:     0xAA (START_BYTE)
Byte 1:     Command (0x82 = CONFIG_SET, 0x81 = PAIRING_ACK)
Byte 2-7:   Target MAC (6 Bytes)
Byte 8-9:   Register (16-bit, big-endian)
Byte 10-11: LED Count (16-bit, big-endian)
Byte 12-22: Reserved
Byte 23:    Checksum (XOR von Byte 1-22)
```

### Nano → Gateway → Hub (Pairing Request)

19-Byte Frame:

```
Byte 0:     0xAA (START_BYTE)
Byte 1:     Message Type (0x01 = PAIRING, 0x02 = CONFIG_ACK)
Byte 2-7:   Source MAC (6 Bytes)
Byte 8-17:  Data (10 Bytes, für Erweiterungen)
Byte 18:    Checksum (XOR von Byte 1-17)
```

## Gateway-ESP32 Firmware

Der Gateway ist per USB-Serial mit dem Hub verbunden und leitet Nachrichten bidirektional weiter.

```cpp
#include <esp_now.h>
#include <WiFi.h>

#define START_BYTE 0xAA
#define CMD_CONFIG_SET 0x82
#define CMD_PAIRING_ACK 0x81

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);

    if (esp_now_init() != ESP_OK) {
        return;
    }

    esp_now_register_recv_cb(onEspNowReceive);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
}

void loop() {
    if (Serial.available() >= 18) {
        uint8_t buffer[24];
        int len = Serial.readBytes(buffer, 1);

        if (buffer[0] != START_BYTE) return;

        uint8_t cmd = Serial.peek();

        if (cmd == CMD_CONFIG_SET || cmd == CMD_PAIRING_ACK) {
            Serial.readBytes(buffer + 1, 23);
            sendConfigToNano(buffer);
        } else {
            Serial.readBytes(buffer + 1, 17);
            broadcastCommand(buffer);
        }
    }
}

void broadcastCommand(uint8_t* frame) {
    esp_now_send(broadcastAddress, frame + 1, 16);
}

void sendConfigToNano(uint8_t* frame) {
    uint8_t targetMac[6];
    memcpy(targetMac, frame + 2, 6);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, targetMac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) == ESP_OK || esp_now_is_peer_exist(targetMac)) {
        esp_now_send(targetMac, frame + 1, 22);
    }
}

void onEspNowReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    uint8_t frame[19];
    frame[0] = START_BYTE;
    frame[1] = data[0];
    memcpy(frame + 2, info->src_addr, 6);
    memcpy(frame + 8, data + 1, 10);

    uint8_t checksum = 0;
    for (int i = 1; i < 18; i++) {
        checksum ^= frame[i];
    }
    frame[18] = checksum;

    Serial.write(frame, 19);
}
```

## Nano-ESP32 Firmware

Der Nano empfängt ESP-NOW Broadcasts und reagiert auf den Button für Pairing.

```cpp
#include <esp_now.h>
#include <WiFi.h>
#include <Preferences.h>
#include <FastLED.h>

#define BUTTON_PIN 0
#define LED_DATA_PIN 5
#define MAX_LEDS 300

#define START_BYTE 0xAA
#define CMD_CONFIG_SET 0x82
#define CMD_PAIRING_ACK 0x81
#define MSG_PAIRING 0x01
#define MSG_CONFIG_ACK 0x02

Preferences preferences;
CRGB leds[MAX_LEDS];

uint8_t gatewayMac[6];
uint16_t myRegister = 0;
uint16_t ledCount = 60;
bool pairingMode = false;
unsigned long pairingStartTime = 0;
const unsigned long PAIRING_TIMEOUT = 30000;

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    loadConfig();

    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, ledCount);
    FastLED.setBrightness(50);

    WiFi.mode(WIFI_STA);
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK) {
        return;
    }

    esp_now_register_recv_cb(onEspNowReceive);
}

void loop() {
    handleButton();
    handlePairingMode();
}

void handleButton() {
    static bool lastState = HIGH;
    static unsigned long pressStart = 0;

    bool state = digitalRead(BUTTON_PIN);

    if (lastState == HIGH && state == LOW) {
        pressStart = millis();
    }

    if (lastState == LOW && state == HIGH) {
        unsigned long duration = millis() - pressStart;

        if (duration > 50 && duration < 2000) {
            startPairingMode();
        }
    }

    lastState = state;
}

void startPairingMode() {
    pairingMode = true;
    pairingStartTime = millis();
    Serial.println("Pairing mode started");

    fill_solid(leds, ledCount, CRGB::Blue);
    FastLED.show();

    sendPairingRequest();
}

void handlePairingMode() {
    if (!pairingMode) return;

    if (millis() - pairingStartTime > PAIRING_TIMEOUT) {
        pairingMode = false;
        fill_solid(leds, ledCount, CRGB::Black);
        FastLED.show();
        Serial.println("Pairing timeout");
        return;
    }

    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 500) {
        lastBlink = millis();
        static bool on = false;
        on = !on;
        fill_solid(leds, ledCount, on ? CRGB::Blue : CRGB::Black);
        FastLED.show();
    }

    static unsigned long lastRequest = 0;
    if (millis() - lastRequest > 1000) {
        lastRequest = millis();
        sendPairingRequest();
    }
}

void sendPairingRequest() {
    uint8_t data[11];
    data[0] = MSG_PAIRING;
    memset(data + 1, 0, 10);

    esp_now_send(gatewayMac, data, 11);
}

void sendConfigAck() {
    uint8_t data[11];
    data[0] = MSG_CONFIG_ACK;
    memset(data + 1, 0, 10);

    esp_now_send(gatewayMac, data, 11);
}

void onEspNowReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (len < 1) return;

    uint8_t cmd = data[0];

    if (cmd == CMD_PAIRING_ACK) {
        memcpy(gatewayMac, info->src_addr, 6);

        fill_solid(leds, ledCount, CRGB::Green);
        FastLED.show();
        delay(500);

        return;
    }

    if (cmd == CMD_CONFIG_SET && len >= 11) {
        uint16_t newRegister = (data[7] << 8) | data[8];
        uint16_t newLedCount = (data[9] << 8) | data[10];

        myRegister = newRegister;
        ledCount = min(newLedCount, (uint16_t)MAX_LEDS);

        saveConfig();

        pairingMode = false;
        fill_solid(leds, ledCount, CRGB::Green);
        FastLED.show();

        sendConfigAck();

        Serial.printf("Config received: register=%d, leds=%d\n", myRegister, ledCount);
        return;
    }

    handleEffectCommand(data, len);
}

void handleEffectCommand(const uint8_t* data, int len) {
    if (len < 16) return;

    uint16_t groups = (data[4] << 8) | data[5];
    uint16_t myGroupMask = (myRegister > 0) ? (1 << myRegister) : 0;

    if (groups != 0xFFFF && (groups & myGroupMask) == 0 && (groups & 0x0001) == 0) {
        return;
    }

    uint8_t effect = data[3];
    uint8_t r = data[10];
    uint8_t g = data[11];
    uint8_t b = data[12];
    uint8_t intensity = data[15];

    FastLED.setBrightness(intensity);

    switch (effect) {
        case 0x20:
            fill_solid(leds, ledCount, CRGB(r, g, b));
            break;

        case 0x11:
            fill_solid(leds, ledCount, CRGB(r, g, b));
            FastLED.setBrightness(intensity / 4);
            break;

        case 0x10:
        case 0x14:
            fill_solid(leds, ledCount, CRGB::Black);
            break;
    }

    FastLED.show();
}

void loadConfig() {
    preferences.begin("nano", true);
    myRegister = preferences.getUShort("register", 0);
    ledCount = preferences.getUShort("ledCount", 60);
    preferences.getBytes("gateway", gatewayMac, 6);
    preferences.end();

    Serial.printf("Loaded config: register=%d, leds=%d\n", myRegister, ledCount);
}

void saveConfig() {
    preferences.begin("nano", false);
    preferences.putUShort("register", myRegister);
    preferences.putUShort("ledCount", ledCount);
    preferences.putBytes("gateway", gatewayMac, 6);
    preferences.end();

    Serial.println("Config saved to NVS");
}
```

## Pairing-Workflow

1. **User** öffnet Web-UI und klickt "Pairing starten"
2. **Hub** setzt `pairing_mode = true`
3. **User** drückt Button auf Nano (kurzer Druck < 2s)
4. **Nano** geht in Pairing-Modus, LED blinkt blau
5. **Nano** sendet wiederholte `MSG_PAIRING` Requests via ESP-NOW
6. **Gateway** empfängt Request, leitet an Hub weiter (Serial)
7. **Hub** erkennt Pairing-Request, sendet `PAIRING_ACK` zurück
8. **Hub** benachrichtigt Web-UI via WebSocket (`nano_pairing` Event)
9. **User** konfiguriert Register und LED-Count in UI
10. **User** klickt "Konfiguration senden"
11. **Hub** sendet `CONFIG_SET` Command an spezifische MAC
12. **Nano** empfängt Config, speichert in NVS, sendet `CONFIG_ACK`
13. **Hub** empfängt ACK, benachrichtigt UI (`nano_config_ack` Event)
14. **Nano** zeigt grünes LED-Feedback, verlässt Pairing-Modus

## Wichtige Hinweise

- Der Nano speichert die Gateway-MAC beim ersten erfolgreichen Pairing
- Die Konfiguration (Register, LED-Count) wird im NVS persistiert
- Bei Stromausfall/Neustart bleibt die Konfiguration erhalten
- Button-Druck startet nur Pairing, ändert keine bestehende Config
- Pairing-Timeout: 30 Sekunden (dann zurück zum normalen Betrieb)
