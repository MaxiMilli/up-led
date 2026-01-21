#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "constants.h"

#define SERIAL_UPSTREAM_START_BYTE 0xBB

void logMessage(const char *message);

const uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

enum class FrameState
{
  WAITING_FOR_START,
  RECEIVING_PAYLOAD
};

FrameState frameState = FrameState::WAITING_FOR_START;
uint8_t frameBuffer[CONFIG_FRAME_SIZE];
uint8_t bufferIndex = 0;
uint32_t frameStartTime = 0;
uint32_t ledOffTime = 0;
bool ledBlinking = false;
bool testMode = false;
uint32_t lastTestFrameTime = 0;

/**
 * @brief Sends upstream message to Hub via Serial
 * @param msgType Message type (MSG_TYPE_PAIRING, MSG_TYPE_CONFIG_ACK)
 * @param macAddr Source MAC address (6 bytes)
 * @param data Optional additional data
 * @param dataLen Length of additional data
 */
void sendToHub(uint8_t msgType, const uint8_t *macAddr, const uint8_t *data = nullptr, uint8_t dataLen = 0)
{
  uint8_t frame[32];
  uint8_t idx = 0;

  frame[idx++] = SERIAL_UPSTREAM_START_BYTE;
  frame[idx++] = msgType;

  for (int i = 0; i < 6; i++)
  {
    frame[idx++] = macAddr[i];
  }

  for (int i = 0; i < dataLen && idx < 31; i++)
  {
    frame[idx++] = data[i];
  }

  // Calculate CRC-8 checksum for bytes 1 to idx-1 (excluding start byte)
  uint8_t checksum = 0;
  for (int i = 1; i < idx; i++)
  {
    checksum = pgm_read_byte(&CRC8_TABLE[checksum ^ frame[i]]);
  }
  frame[idx++] = checksum;

  Serial.write(frame, idx);

  char logBuffer[64];
  snprintf(
      logBuffer,
      sizeof(logBuffer),
      "RX->Hub type=0x%02X from=%02X:%02X:%02X:%02X:%02X:%02X",
      msgType,
      macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  logMessage(logBuffer);
}

/**
 * @brief ESP-NOW receive callback - handles incoming messages from Nanos
 * @param macAddr Source MAC address
 * @param data Received data
 * @param dataLen Length of received data
 */
void onDataReceived(const uint8_t *macAddr, const uint8_t *data, int dataLen)
{
  if (dataLen < 1)
  {
    return;
  }

  uint8_t command = data[0];

  switch (command)
  {
  case 0xA0: // <-- HINZUFÜGEN: Der Nano sendet 0xA0
    sendToHub(MSG_TYPE_PAIRING, macAddr);
    break;

  case 0x21: // CMD_CONFIG_ACK
    if (dataLen >= 2)
    {
      sendToHub(MSG_TYPE_CONFIG_ACK, macAddr, &data[1], 1);
    }
    break;

  default:
    char logBuffer[48];
    snprintf(logBuffer, sizeof(logBuffer), "Unknown cmd=0x%02X from Nano", command);
    logMessage(logBuffer);
    break;
  }
}

/**
 * @brief Logs a message with gateway prefix
 * @param message The message to log
 */
void logMessage(const char *message)
{
  Serial.print("[GW] ");
  Serial.println(message);
}

/**
 * @brief Logs a message with a numeric value
 * @param message The message prefix
 * @param value The numeric value to append
 */
void logMessageValue(const char *message, uint32_t value)
{
  Serial.print("[GW] ");
  Serial.print(message);
  Serial.println(value);
}

/**
 * @brief Triggers a short LED blink
 */
void blinkLed()
{
  digitalWrite(LED_PIN, HIGH);
  ledBlinking = true;
  ledOffTime = millis() + LED_BLINK_DURATION_MS;
}

/**
 * @brief Updates LED state for non-blocking blink
 */
void updateLed()
{
  if (ledBlinking && millis() >= ledOffTime)
  {
    digitalWrite(LED_PIN, LOW);
    ledBlinking = false;
  }
}

/**
 * @brief Calculates CRC-8 checksum of payload
 * @param payload Pointer to payload data
 * @param length Length of payload
 * @returns CRC-8 checksum byte
 */
uint8_t calculateChecksum(const uint8_t *payload, uint8_t length)
{
  uint8_t crc = 0x00;

  for (uint8_t i = 0; i < length; i++)
  {
    crc = pgm_read_byte(&CRC8_TABLE[crc ^ payload[i]]);
  }

  return crc;
}

/**
 * @brief Extracts sequence number from payload (Big Endian)
 * @param payload Pointer to payload data
 * @returns 16-bit sequence number
 */
uint16_t extractSequence(const uint8_t *payload)
{
  return (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
}

/**
 * @brief ESP-NOW send callback
 * @param macAddr Destination MAC address
 * @param status Send status
 */
void onDataSent(const uint8_t *macAddr, esp_now_send_status_t status)
{
  if (status != ESP_NOW_SEND_SUCCESS)
  {
    logMessage("ESP-NOW send failed");
  }
}

/**
 * @brief Initializes ESP-NOW communication with Long Range mode
 * @returns true if successful, false otherwise
 */
bool initEspNow()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (ESPNOW_LONG_RANGE_ENABLED)
  {
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
    logMessage("Long Range mode enabled");
  }

  esp_wifi_set_max_tx_power(ESPNOW_TX_POWER_DBM * 4);

  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK)
  {
    logMessage("ESP-NOW init failed");
    return false;
  }

  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataReceived);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    logMessage("Failed to add broadcast peer");
    return false;
  }

  int8_t txPower;
  esp_wifi_get_max_tx_power(&txPower);
  char powerStr[32];
  snprintf(powerStr, sizeof(powerStr), "TX Power: %.1f dBm", txPower / 4.0);
  logMessage(powerStr);

  return true;
}

/**
 * @brief Sends payload via ESP-NOW broadcast
 * @param payload Pointer to 16-byte payload
 */
void sendPayload(const uint8_t *payload)
{
  uint16_t seq = extractSequence(payload);

  esp_err_t result = esp_now_send(broadcastAddress, payload, ESPNOW_PAYLOAD_SIZE);

  if (result == ESP_OK)
  {
    char logBuffer[32];
    snprintf(logBuffer, sizeof(logBuffer), "TX SEQ=%u", seq);
    logMessage(logBuffer);
    blinkLed();
  }
  else
  {
    logMessage("ESP-NOW send error");
  }
}

void processConfigFrame(const uint8_t *payload);

/**
 * @brief Processes a complete frame from buffer
 * Frame format: [0]=START, [1-16]=payload, [17]=checksum
 * Payload: [0-1]=groups, [2-3]=duration, [4]=effect, [5]=length, [6]=brightness,
 *          [7-8]=speed, [9-11]=rgb, [12]=intensity, [13-15]=reserved
 */
void processFrame()
{
  uint8_t *payload = &frameBuffer[1];
  uint8_t effect = payload[4];

  uint8_t calculatedChecksum = calculateChecksum(payload, ESPNOW_PAYLOAD_SIZE);
  uint8_t receivedChecksum = frameBuffer[ESPNOW_PAYLOAD_SIZE + 1];

  if (calculatedChecksum != receivedChecksum)
  {
    uint16_t seq = extractSequence(payload);
    char logBuffer[48];
    snprintf(
        logBuffer,
        sizeof(logBuffer),
        "Checksum error SEQ=%u (exp=0x%02X got=0x%02X)",
        seq,
        calculatedChecksum,
        receivedChecksum);
    logMessage(logBuffer);
    frameState = FrameState::WAITING_FOR_START;
    return;
  }

  if (effect == CMD_PAIRING_ACK || effect == CMD_CONFIG_SET)
  {
    processConfigFrame(payload);
  }
  else
  {
    sendPayload(payload);
  }

  frameState = FrameState::WAITING_FOR_START;
}

/**
 * @brief Processes incoming serial data
 */
void processSerial()
{
  while (Serial.available())
  {
    uint8_t byte = Serial.read();

    switch (frameState)
    {
    case FrameState::WAITING_FOR_START:
      if (byte == SERIAL_START_BYTE)
      {
        frameState = FrameState::RECEIVING_PAYLOAD;
        bufferIndex = 0;
        frameBuffer[bufferIndex++] = byte;
        frameStartTime = millis();
      }
      break;

    case FrameState::RECEIVING_PAYLOAD:
      frameBuffer[bufferIndex++] = byte;

      if (bufferIndex >= ESPNOW_PAYLOAD_SIZE + 2)
      {
        processFrame();
      }
      break;
    }
  }
}

/**
 * @brief Checks for frame reception timeout
 */
void checkFrameTimeout()
{
  if (frameState == FrameState::RECEIVING_PAYLOAD)
  {
    if (millis() - frameStartTime > FRAME_TIMEOUT_MS)
    {
      logMessage("Frame timeout, resync");
      frameState = FrameState::WAITING_FOR_START;
    }
  }
}

/**
 * @brief Sends test frame in test mode
 */
void sendTestFrame()
{
  if (!testMode)
  {
    return;
  }

  if (millis() - lastTestFrameTime >= TEST_FRAME_INTERVAL_MS)
  {
    lastTestFrameTime = millis();

    static uint16_t testSeq = 0;
    testSeq++;

    uint8_t testPayload[ESPNOW_PAYLOAD_SIZE] = {
        static_cast<uint8_t>(testSeq >> 8),
        static_cast<uint8_t>(testSeq & 0xFF),
        0x00,
        0x01,
        0xFF, 0xFF,
        0x00, 0x00,
        0x00,
        0x00,
        0x00, 0x00, 0x00,
        0x00, 0x00,
        0xFF};

    logMessage("Sending test frame (HEARTBEAT)");
    sendPayload(testPayload);
  }
}

/**
 * @brief Processes config frame and sends to specific Nano via ESP-NOW
 * @param payload Pointer to 16-byte payload (after start byte)
 *
 * Hub Frame Format (payload indices after start byte):
 *   [0-1]=groups, [2-3]=duration, [4]=effect, [5]=length, [6]=brightness,
 *   [7-8]=speed, [9-11]=rgb, [12]=intensity, [13-15]=reserved
 *
 * MAC is encoded in payload fields:
 *   - payload[9] (r) = MAC[0]
 *   - payload[10] (g) = MAC[1]
 *   - payload[11] (b) = MAC[2]
 *   - payload[7] (speed_hi) = MAC[3]
 *   - payload[8] (speed_lo) = MAC[4]
 *   - payload[12] (intensity) = MAC[5]
 * Config data:
 *   - payload[5] (length) = register
 *   - payload[2-3] (duration) = led_count
 */
void processConfigFrame(const uint8_t *payload)
{
  uint8_t command = payload[3];

  uint8_t targetMac[6];
  targetMac[0] = payload[10];
  targetMac[1] = payload[11];
  targetMac[2] = payload[12];
  targetMac[3] = (payload[13]);
  targetMac[4] = payload[14];
  targetMac[5] = payload[15];

  uint8_t reg = payload[8];
  uint16_t ledCount = (static_cast<uint16_t>(payload[6]) << 8) | payload[7];

  uint8_t standby_r = payload[4];
  uint8_t standby_g = payload[5];
  uint8_t standby_b = payload[2];

  uint8_t configPayload[16] = {0};
  configPayload[0] = 0x00;
  configPayload[1] = 0x00;
  configPayload[2] = 0x00;
  configPayload[3] = command;
  configPayload[4] = 0xFF;
  configPayload[5] = 0xFF;
  configPayload[6] = (ledCount >> 8) & 0xFF;
  configPayload[7] = ledCount & 0xFF;
  configPayload[8] = reg;
  configPayload[9] = standby_r;
  configPayload[10] = standby_g;
  configPayload[11] = standby_b;
  configPayload[12] = 0x00;
  configPayload[13] = 0x00;
  configPayload[14] = 0x00;
  configPayload[15] = 0x00;

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, targetMac, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;

  esp_err_t addResult = esp_now_add_peer(&peerInfo);

  esp_err_t result = esp_now_send(targetMac, configPayload, 16);

  if (addResult == ESP_OK || addResult == ESP_ERR_ESPNOW_EXIST)
  {
    esp_now_del_peer(targetMac);
  }

  char logBuffer[64];
  snprintf(
      logBuffer,
      sizeof(logBuffer),
      "Config->%02X:%02X:%02X:%02X:%02X:%02X reg=%d leds=%d %s",
      targetMac[0], targetMac[1], targetMac[2],
      targetMac[3], targetMac[4], targetMac[5],
      reg, ledCount,
      result == ESP_OK ? "OK" : "FAIL");
  logMessage(logBuffer);

  blinkLed();
}

void setup()
{
  Serial.begin(SERIAL_BAUD_RATE);
  delay(100);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  digitalWrite(LED_PIN, LOW);

  logMessage("Gateway starting...");

  testMode = (digitalRead(BOOT_BUTTON_PIN) == LOW);

  if (testMode)
  {
    logMessage("TEST MODE enabled (Boot button held)");
  }

  if (!initEspNow())
  {
    logMessage("Init failed, rebooting in 5s...");
    delay(INIT_FAIL_REBOOT_MS);
    ESP.restart();
  }

  digitalWrite(LED_PIN, HIGH);
  logMessage("Gateway ready");

  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macStr[24];
  snprintf(
      macStr,
      sizeof(macStr),
      "MAC: %02X:%02X:%02X:%02X:%02X:%02X",
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  logMessage(macStr);
}

void loop()
{
  processSerial();
  checkFrameTimeout();
  updateLed();
  sendTestFrame();
}
