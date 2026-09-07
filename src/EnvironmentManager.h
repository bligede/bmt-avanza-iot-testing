// =============================================================================
//  EnvironmentManager.h — DHT22 on GPIO15 (master prompt §21).
//
//  The hardware has a DHT22, but Blueprint v1.0 §7.1 does not list temperature
//  or humidity in the payload. Adding them changes the backend schema, so the
//  module is OFF by default and its readings never reach the production payload
//  without an explicit project-leader approval — enforced by a compile guard in
//  Config.h §20, not by convention.
//
//  What it IS useful for right now: feeding the fan controller a real enclosure
//  temperature instead of the SoC die temperature, and giving the thermal
//  validation in docs/14-TODO.md some numbers to work from.
//
//  The bit-banged driver blocks for about 5 ms with interrupts disabled. It
//  runs on core 1 at the lowest priority, so it cannot touch CAN acquisition on
//  core 0. Sampling is limited to DHT_SAMPLE_INTERVAL_MS.
// =============================================================================
#pragma once

#include <Arduino.h>
#include "Config.h"

struct EnvReading {
    bool  valid;
    float temperature_c;
    float humidity_pct;
    uint32_t at_ms;
};

struct EnvStats {
    uint32_t reads_ok;
    uint32_t read_errors;        // every failed transaction, all causes
    uint32_t checksum_errors;

    // WHERE the transaction died. read_errors alone cannot tell a sensor that
    // never answers (power, wiring, dead part) from one that answers badly
    // (pull-up, timing, interference) — and those need opposite fixes. This is
    // the same blind spot the GPS counters had; see GpsStats.
    uint32_t fail_no_response;   // line stayed high: nothing pulled it down
    uint32_t fail_handshake;     // came low, but the 80/80 response never finished
    uint32_t fail_truncated;     // handshake fine, bit stream stopped partway
    uint32_t fail_range;         // checksum passed, values outside the datasheet
    uint8_t  last_bits;          // bits received in the most recent attempt
    uint8_t  last_frame[5];      // the five raw bytes, whatever they decoded to

    // DATA level with the internal pull-up on and the sensor idle. This must be
    // HIGH. LOW means no pull-up is reaching the pin, DATA is shorted to
    // ground, or the part is holding the line down — none of which any protocol
    // timing can fix, so it is worth knowing before reading anything else.
    bool     line_idle_high;
};

namespace EnvironmentManager {

void begin();

// Samples if the interval has elapsed. Returns true when a new reading landed.
bool poll();

EnvReading reading();
EnvStats   stats();
bool       isEnabled();

}  // namespace EnvironmentManager
