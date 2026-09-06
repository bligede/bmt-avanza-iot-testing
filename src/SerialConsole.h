// =============================================================================
//  SerialConsole.h — technician console on the USB serial port.
//
//  Exists so a capture can be started, stopped and pulled off the device
//  without a rebuild.
//
//  WHAT IT DELIBERATELY CANNOT DO:
//    * enable CAN transmit — no such function exists to call, and the build
//      would reject one (CanBusSafety.h);
//    * change the CAN bitrate on a live vehicle — that is a flash-time
//      decision, made on the bench.
//
//  Every command either reports state or manages capture files. That is the
//  entire surface.
// =============================================================================
#pragma once

#include <Arduino.h>
#include "Config.h"

namespace SerialConsole {

void begin();

// Reads whatever is available and runs any complete line. Non-blocking.
void poll();

void printHelp();
void printStatus();

}  // namespace SerialConsole
