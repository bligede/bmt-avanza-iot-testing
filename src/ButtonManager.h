// =============================================================================
//  ButtonManager.h — GPIO0 momentary button (master prompt §24).
//
//  GPIO0 is RESERVED. It is deliberately wired to nothing that could change a
//  safety property of the device:
//
//      * it cannot enable CAN transmit — no such function exists to call;
//      * it cannot change the CAN bitrate on a live vehicle;
//      * it cannot disable TLS or alter the transmission policy.
//
//  What it does today is print a status summary, which is genuinely useful when
//  a technician is under a dashboard with a laptop and no network. Long-press
//  is recognised and reported but intentionally left unbound: assigning it a
//  destructive action before anyone has asked for one is how a field reset
//  happens by accident.
//
//  GPIO0 is also the ESP32 boot-strap pin. Holding it during reset enters the
//  bootloader — that is the chip, not this firmware.
// =============================================================================
#pragma once

#include <Arduino.h>
#include "Config.h"

enum class ButtonEvent : uint8_t {
    None = 0,
    ShortPress,
    LongPress,
};

namespace ButtonManager {

void begin();

// Debounces and returns any event that completed this call.
ButtonEvent poll();

bool     isPressed();
uint32_t pressCount();

}  // namespace ButtonManager
