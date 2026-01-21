#include "wifi_connection.h"

#include "logging.h"

bool wifi_connected = false;

void ConnectToWifi() {
  LOG("Connecting to WiFi network: " + String(kSsid));
  // Delete old config
  WiFi.disconnect(true);
  // Register event handler
  WiFi.onEvent(WiFiEvent);  // Will call WiFiEvent() from another thread.

  // Initiate connection
  WiFi.begin(kSsid, kWifiPwd);

  LOG("Waiting for WIFI connection...");
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      // When connected
      LOGF("WiFi connected! IP address: %s\n", WiFi.localIP().toString());
      wifi_connected = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (wifi_connected) {
        LOG("WiFi lost connection");
      }
      wifi_connected = false;
      break;
    default:
      break;
  }
}

void DisconnectWifi() {
  LOG("Disconnecting from WiFi");
  WiFi.disconnect(true);
}
