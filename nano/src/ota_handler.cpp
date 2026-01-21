#include "ota_handler.h"
#include "logging.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

// WiFi credentials for OTA updates
const char* kOtaWifiSsid = "uzepatscher_lichtshow";
const char* kOtaWifiPassword = "kWalkingLight";

// Hub connection settings
const char* kHubHost = "192.168.1.195";
const int kHubPort = 8000;

// WiFi connection timeout (ms)
const uint32_t kWifiTimeout = 10000;

// Version embedded at compile time via platformio.ini
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION 1
#endif

uint32_t GetFirmwareVersion()
{
    return FIRMWARE_VERSION;
}

bool CheckAndPerformOta()
{
    LOG("OTA: Starting update check...");
    LOGF("OTA: Current firmware version: %d\n", FIRMWARE_VERSION);

    // 1. Connect to WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(kOtaWifiSsid, kOtaWifiPassword);

    uint32_t start_time = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - start_time > kWifiTimeout)
        {
            LOG("OTA: WiFi connection timeout, skipping update");
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            delay(100);
            return false;
        }
        delay(100);
    }

    LOGF("OTA: WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());

    // 2. Check version from Hub
    HTTPClient http;
    String version_url = String("http://") + kHubHost + ":" + kHubPort + "/firmware/version";

    http.begin(version_url);
    http.setTimeout(5000);

    int http_code = http.GET();
    if (http_code != 200)
    {
        LOGF("OTA: Version check failed, HTTP code: %d\n", http_code);
        http.end();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(100);
        return false;
    }

    String response = http.getString();
    http.end();

    // Parse JSON: {"version": 42}
    int colon_pos = response.indexOf(':');
    int brace_pos = response.indexOf('}');
    if (colon_pos < 0 || brace_pos < 0)
    {
        LOG("OTA: Failed to parse version response");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(100);
        return false;
    }

    int server_version = response.substring(colon_pos + 1, brace_pos).toInt();
    LOGF("OTA: Server version: %d\n", server_version);

    // 3. Check if update needed
    if (server_version <= FIRMWARE_VERSION)
    {
        LOG("OTA: Firmware is up to date");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(100);
        return false;
    }

    // 4. Perform OTA update
    LOGF("OTA: Newer firmware available (v%d -> v%d), starting update...\n",
         FIRMWARE_VERSION, server_version);

    String firmware_url = String("http://") + kHubHost + ":" + kHubPort + "/firmware/binary";

    WiFiClient client;
    httpUpdate.rebootOnUpdate(true);

    t_httpUpdate_return ret = httpUpdate.update(client, firmware_url);

    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
        LOGF("OTA: Update failed! Error (%d): %s\n",
             httpUpdate.getLastError(),
             httpUpdate.getLastErrorString().c_str());
        break;

    case HTTP_UPDATE_NO_UPDATES:
        LOG("OTA: No updates available");
        break;

    case HTTP_UPDATE_OK:
        LOG("OTA: Update successful, rebooting...");
        // Device will reboot automatically
        break;
    }

    // If we get here, update failed
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    return false;
}
