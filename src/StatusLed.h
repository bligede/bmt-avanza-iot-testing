// =============================================================================
//  StatusLed.h — bicolor LED, common cathode (master prompt §23).
//
//      GPIO21 -> resistor -> RED
//      GPIO47 -> resistor -> GREEN
//      common cathode -> GND
//
//  Entirely non-blocking: a pattern table plus a millis() tick. There is no
//  delay() anywhere in this module — an LED must never be able to slow the
//  device down, let alone CAN acquisition.
//
//  The displayed status is the most severe condition currently true, so a
//  technician reads one light and knows the worst thing happening.
// =============================================================================
#pragma once

#include <Arduino.h>
#include "Config.h"

// Ordered by severity, lowest first. The highest active status wins.
enum class LedStatus : uint8_t {
    Boot = 0,
    MqttOk,           // everything nominal
    NetworkOk,        // data link up, broker not yet connected
    ModemConnecting,
    GpsNoFix,
    Buffering,        // CAN fine, uplink down, records going to flash
    CanOk,            // CAN fine, nothing else up yet
    CanError,
    SystemError,
    COUNT
};

namespace StatusLed {

void begin();

// Sets or clears a condition. Several can be true at once.
void set(LedStatus status, bool active);

// Advance the pattern. Call every LED_TICK_MS from the housekeeping task.
void tick();

LedStatus current();
const char* statusName(LedStatus s);

// Both LEDs off. Used before a reboot so a half-lit LED is not mistaken for a
// running device.
void allOff();

}  // namespace StatusLed
