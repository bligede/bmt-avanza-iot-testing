// =============================================================================
//  WifiManager.h — DIAGNOSTIC WiFi link (D-016).
//
//  ###########################################################################
//  #  THIS IS NOT THE PRODUCTION UPLINK.                                     #
//  #                                                                         #
//  #  Blueprint §6 specifies 4G (A7670C) + MQTT over TLS as the uplink for   #
//  #  the fleet. That path is ModemManager + MqttManager and is untouched by #
//  #  this module.                                                           #
//  #                                                                         #
//  #  WiFi exists for ONE reason: bench and vehicle bring-up. It removes the #
//  #  modem, the SIM, the broker and the TLS handshake — none of which have  #
//  #  ever been exercised, and all of which D-005 blocks — from a test whose #
//  #  only question is whether CAN frames are arriving.                      #
//  #                                                                         #
//  #  Nothing here publishes telemetry anywhere. It is OFF by default.       #
//  ###########################################################################
//
//  Non-blocking, like every other network module here. WiFi.begin() is already
//  asynchronous; this polls WiFi.status() and applies a backoff. It never waits
//  in a loop, so the 10 s task watchdog is never at risk.
//
//  Runs on core 1. Note that the ESP-IDF WiFi driver tasks run at a HIGHER
//  priority than the CAN reader and may be scheduled on core 0 — see
//  docs/19-TEST-AVANZA-WIFI.md, "Risk: WiFi versus CAN acquisition". Watch
//  rx_missed on the dashboard during the test; that number is the measurement.
// =============================================================================
#pragma once

#include <Arduino.h>
#include "Config.h"

enum class WifiState : uint8_t {
    Disabled = 0,
    Idle,
    Connecting,
    Connected,
    Reconnecting,
    Failed,
};

struct WifiStats {
    uint32_t connects;
    uint32_t disconnects;
    uint32_t connect_failures;
    int32_t  rssi;              // dBm, 0 when not connected
    uint32_t connected_since_ms;
    char     ip[16];
    char     ssid[33];
};

namespace WifiManager {

// Opens the radio and arms the state machine. Does not block.
void begin();

// One step. Call often; returns immediately.
void poll();

bool        isConnected();
WifiState   state();
const char* stateName(WifiState s);
WifiStats   stats();

// True when the build carries a hotspot SSID at all.
bool isConfigured();

// Shuts the radio down, so a restart does not leave the AP holding a half-open
// association.
void end();

}  // namespace WifiManager
