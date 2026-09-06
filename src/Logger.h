// =============================================================================
//  Logger.h — levelled serial console (master prompt §27).
//
//  Levels are compiled out, not filtered at runtime, so a production build
//  carries no DEBUG strings at all. Never log a credential: MQTT_PASS, APN_PASS
//  and the TLS material must not reach the console (master prompt §33).
// =============================================================================
#pragma once

#include <Arduino.h>
#include "Config.h"

namespace Logger {

void begin(unsigned long baud = LOG_BAUD);

// Thread-safe: serialised with a mutex so two cores cannot interleave a line.
void printf(int level, const char* tag, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));

// Hex dump helper for CAN payloads: "00 30 39 00 00 00 00 00".
void formatBytes(char* out, size_t outLen, const uint8_t* data, uint8_t len);

// Replaces every character of a secret with '*' for the rare case where a
// credential-shaped value has to be acknowledged on the console at all.
const char* redact(const char* secret);

}  // namespace Logger

// Tags mirror the console format in the master prompt: [BOOT] [CAN] [GPS] ...
#if LOG_LEVEL >= LOG_LEVEL_ERROR
  #define LOG_E(tag, ...) Logger::printf(LOG_LEVEL_ERROR, tag, __VA_ARGS__)
#else
  #define LOG_E(tag, ...) do {} while (0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARN
  #define LOG_W(tag, ...) Logger::printf(LOG_LEVEL_WARN, tag, __VA_ARGS__)
#else
  #define LOG_W(tag, ...) do {} while (0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
  #define LOG_I(tag, ...) Logger::printf(LOG_LEVEL_INFO, tag, __VA_ARGS__)
#else
  #define LOG_I(tag, ...) do {} while (0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
  #define LOG_D(tag, ...) Logger::printf(LOG_LEVEL_DEBUG, tag, __VA_ARGS__)
#else
  #define LOG_D(tag, ...) do {} while (0)
#endif
