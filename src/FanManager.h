// =============================================================================
//  FanManager.h — 12 V fan via IRLZ44N on GPIO13 (master prompt §22).
//
//      GPIO13 -> 330R -> IRLZ44N gate;  fan across drain/source.
//
//  Three modes: OFF, AUTO, FORCED_ON.
//
//  ############################################################################
//  #  TODO — VALIDASI TERMAL                                                  #
//  #  FAN_ON_TEMP and FAN_OFF_TEMP in Config.h are PLACEHOLDERS. The blueprint #
//  #  specifies no thermal limit, and inventing one is how a fan ends up       #
//  #  running all day in a parked car in Bali, or never running at all.        #
//  #  Measure the enclosure in the vehicle before locking these in.            #
//  ############################################################################
//
//  Hysteresis plus a minimum dwell time keep the fan from chattering around the
//  threshold, which is what kills a MOSFET and a fan bearing.
// =============================================================================
#pragma once

#include <Arduino.h>
#include "Config.h"

enum class FanMode : uint8_t {
    Off = 0,
    Auto,
    ForcedOn,
};

struct FanStats {
    bool     running;
    FanMode  mode;
    float    last_temperature_c;
    bool     temperature_valid;
    uint32_t transitions;
    uint32_t run_seconds;
};

namespace FanManager {

void begin();

// Evaluates the temperature and applies the hysteresis. Call from housekeeping.
void poll();

void    setMode(FanMode mode);
FanMode mode();
bool    isRunning();
FanStats stats();
const char* modeName(FanMode m);

}  // namespace FanManager
