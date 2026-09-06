// =============================================================================
//  GpsManager.h — GY-GPS6MV2 (u-blox NEO-6M) over UART.
//
//  In THIS build it exists for one reason: Blueprint §9.1 requires a CAN speed
//  candidate to be validated against GNSS ground speed, because speed is the
//  only signal with an independent reference. When a speed field turns up in
//  the Avanza capture, this is what confirms it.
//
//  The fix also carries UTC, which the dashboard shows directly. Unlike the
//  fleet firmware there is no TimeService here to arbitrate between GNSS, NTP
//  and modem time — there is no telemetry timestamp to arbitrate for. See the
//  note in GpsManager.cpp; that is the only divergence in this module.
//
//  With no fix, lat/lon are reported invalid. There is no code path that emits
//  0.0/0.0 as a position — the Gulf of Guinea is a real place and a map full of
//  vehicles there is worse than a gap.
//
//  A dead or unplugged GPS module must not affect anything else. The reader is
//  its own task, it never blocks longer than its UART timeout, and a total GPS
//  failure only clears the fix.
// =============================================================================
#pragma once

#include <Arduino.h>
#include "Config.h"
#include "NmeaParser.h"

struct GpsStats {
    // DIVERGENCE FROM THE FLEET FIRMWARE — worth porting back.
    //
    // Upstream counts only GGA/RMC (sentences_ok) and bad checksums. Anything
    // else — a module at the wrong baud producing garbage, or one configured
    // to emit only GSV/GSA/VTG — lands in the parser's "Ignored" bucket and is
    // counted nowhere. The result reads as sentences_ok=0, bad_checksum=0:
    // byte-for-byte identical to a module that is not wired at all.
    //
    // These two make that distinguishable, which is the difference between
    // "check your wiring" and "check your baud rate".
    uint32_t bytes_received;    // raw bytes off the UART, whatever they are
    uint32_t lines_seen;        // complete lines, including ones we ignore
    uint32_t last_byte_ms;      // so a module that dies mid-session is caught

    uint32_t sentences_ok;
    uint32_t sentences_bad_checksum;
    uint32_t fixes;
    uint32_t fix_lost;
    uint32_t last_fix_ms;
    uint32_t last_sentence_ms;
    bool     ever_fixed;
};

namespace GpsManager {

void begin();

// Drains the UART and applies whole sentences. Called from the GPS task.
// Returns true when a sentence was consumed this call.
bool poll();

// Current fix. Check .valid before using lat/lon — never assume.
GnssFix fix();

// True when there is a fix and it is fresher than GPS_STALE_FIX_MS.
bool hasFreshFix();

// True when the module has produced no sentence at all for GPS_FIX_TIMEOUT_MS,
// which usually means wiring or power rather than sky view.
bool isSilent();

GpsStats stats();
void end();

}  // namespace GpsManager
