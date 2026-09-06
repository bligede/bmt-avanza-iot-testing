// =============================================================================
//  Portability.h — lets the pure-logic modules compile both on the ESP32 and
//  on a host PC (tests/ directory, plain g++).
//
//  Modules that include ONLY this header (never <Arduino.h>) are covered by the
//  host unit tests in tests/:
//      CanSignal, TelemetryRecord, DataValidator, TransmissionPolicy,
//      NmeaParser, BufferFraming
//  Keep them free of Arduino/ESP-IDF calls so the tests keep working.
// =============================================================================
#pragma once

#if defined(ARDUINO)
  #include <Arduino.h>
#else
  #include <cstdint>
  #include <cstdio>
  #include <cstring>
  #include <cstdlib>
  #include <cmath>
  #include <cstdbool>
#endif
