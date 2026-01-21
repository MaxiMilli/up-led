#pragma once

#include <Arduino.h>

#include "command.h"

constexpr const char *kHubIP = "hub.local";
constexpr const int kHubPort = 9000;

void RegisterOnHub();

bool IsHubConnected();

bool GetCommandFromHub(Command &command);

void SendToHub(String message);

void DisconnectFromHub();
