#pragma once

#include <Arduino.h>

// undefine this, to disable logging by default for release
#define ENABLE_LOGGING_DEFAULT true

// Global variable to control logging at runtime in release
extern bool logging_enabled;

// Macros for logging
#define LOG(msg)                                   \
  if (ENABLE_LOGGING_DEFAULT || logging_enabled) { \
    Serial.println(msg);                           \
  }

#define LOGF(fmt, ...)                             \
  if (ENABLE_LOGGING_DEFAULT || logging_enabled) { \
    Serial.printf((fmt), ##__VA_ARGS__);           \
  }

// Function to enable logging
void InitializeLogging();
