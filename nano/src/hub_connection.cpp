#include "hub_connection.h"

#include <WiFi.h>

#include "logging.h"

bool connection_established = false;

WiFiClient client;

void RegisterOnHub()
{
  LOG("Connecting to server...");
  if (client.connect(kHubIP, kHubPort))
  {
    LOG("Connected to server!");

    String mac = WiFi.macAddress();
    client.print("register--" + mac);
    LOGF("Message sent: %s\n", "register--" + mac);
    connection_established = true;
  }
  else
  {
    LOG("Connection to server failed!");
    connection_established = false;
  }
}

bool IsHubConnected()
{
  bool connected = client.connected();
  if (connection_established && !connected)
  {
    LOG("Hub connection lost");
    connection_established = false;
  }

  return connected;
}

void ReadFromHub(uint8_t *commandBuffer, size_t bytes_to_read, size_t &length)
{
  length = 0; // Initialize the output length to 0
  if (IsHubConnected())
  {
    size_t index = 0;

    // Read server message
    while (client.available())
    {
      if (index < bytes_to_read)
      {
        commandBuffer[index++] = client.read();
      }
      else
      {
        LOG("Buffer overflow, message truncated.");
        break;
      }
    }

    length = index;
    if (length > 0)
    {
      LOGF("Server message: %.*s", length,
           commandBuffer); // TODO: Switch to hex printing
    }
  }
}

bool GetCommandFromHub(Command &command)
{
  uint8_t response_buffer[kCommandSize];
  size_t response_length;
  ReadFromHub(response_buffer, kCommandSize, response_length);
  if (response_length == kCommandSize)
  {
    command = DecodeCommand(response_buffer);
    return true;
  }

  return false;
}

void SendToHub(String message)
{
  if (IsHubConnected())
  {
    LOGF("Sending to server: %s", message);
    client.print(message);
  }
}

void DisconnectFromHub()
{
  LOG("Disconnecting from Hub");
  client.stop();
}
