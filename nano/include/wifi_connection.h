#pragma once

#include <WiFi.h>

constexpr const char *kSsid = "uzepatscher_lichtshow"; // "Jeanots Hotspot";
constexpr const char *kWifiPwd = "kWalkingLight";      // "244466666";

extern bool wifi_connected;

void ConnectToWifi();

void WiFiEvent(WiFiEvent_t event);

void DisconnectWifi();
