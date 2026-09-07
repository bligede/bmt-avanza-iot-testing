// =============================================================================
//  NmeaParser.h — NMEA 0183 for the GY-GPS6MV2 (u-blox NEO-6M).
//
//  Hand-rolled rather than pulled from a library so the sketch keeps zero
//  third-party dependencies and so the parser can be unit-tested on the host
//  against real sentences (tests/test_nmea.cpp).
//
//  Only GGA and RMC are needed: GGA carries the fix quality and satellite
//  count, RMC carries the date, which GGA does not.
//
//  A sentence with a bad checksum, or with the RMC status field set to 'V',
//  never reaches the fix. Master prompt §11: no fabricated coordinates.
// =============================================================================
#pragma once

#include "Portability.h"

struct GnssFix {
    bool     valid;          // a real 2D/3D fix — safe to publish
    double   lat;            // decimal degrees, south negative
    double   lon;            // decimal degrees, west negative
    float    speed_kmh;      // ground speed, for cross-checking CAN speed
    float    hdop;
    uint8_t  satellites;
    uint8_t  fix_quality;    // GGA field 6: 0 = no fix

    bool     time_valid;     // a complete, plausible UTC date+time
    uint64_t epoch;          // UTC seconds

    // Sky view, from GSV. What the receiver can HEAR is a different question
    // from whether it can fix, and it is the only one that separates "no sky"
    // from "no antenna". Without it, a module streaming perfect NMEA with no
    // fix looks identical to one with its antenna disconnected: both report
    // zero satellites used and no position.
    uint8_t  sats_in_view;   // GSV field 3
    uint8_t  sats_tracked;   // of those, how many report a signal at all
    uint8_t  best_cnr;       // strongest carrier-to-noise, dB-Hz; 0 = hears nothing
};

enum class NmeaResult : uint8_t {
    Ignored = 0,     // not a sentence we use
    Gga,
    Rmc,
    Gsv,
    BadChecksum,
    Malformed,
};

namespace NmeaParser {

GnssFix makeEmptyFix();

// Verifies the "*hh" checksum. Accepts a sentence with or without the trailing
// CR/LF, with or without the leading '$'.
bool checksumOk(const char* sentence);

// Applies one sentence to `fix`. `fix` accumulates across sentences: GGA
// supplies position and quality, RMC supplies date/time and ground speed.
NmeaResult apply(const char* sentence, GnssFix& fix);

// ddmm.mmmm + hemisphere -> decimal degrees. Returns false on a malformed field.
bool parseCoordinate(const char* value, char hemisphere, double* out);

// Converts a UTC date/time to a Unix epoch. Proleptic Gregorian, no leap
// seconds — which is what the epoch field in the payload wants.
uint64_t toEpoch(int year, int month, int day, int hour, int minute, int second);

}  // namespace NmeaParser
