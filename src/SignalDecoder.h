// =============================================================================
//  SignalDecoder.h: the proven DFSK Gelora E mappings, turned into values.
//
//  This device has always been a RECORDER: it shows identifiers, raw payloads
//  and notes, and every interpretation happened on a laptop afterwards. The
//  dashboards asked for on 28 Sep 2026 both need something this firmware did
//  not have, which is named values off the bus.
//
//  WHAT MAY LIVE IN THIS TABLE. Only a mapping whose status is `terbukti` in
//  docs/evidence/dfsk-gelora-e/README.md, which means it MOVED with the vehicle
//  and survived a proof that needed no human reference. A value that merely
//  looked right while parked is not in here: `0x0CFF1601` sat at 26 °C for
//  forty-seven minutes of driving, including a 133 A pull, and was withdrawn.
//
//  THESE IDENTIFIERS BELONG TO A DFSK GELORA E, the test vehicle. The fleet
//  runs Wuling and has never been mapped. Copying this table into a fleet build
//  is the fastest way to make a device display a confident wrong number.
//
//  Evidence: docs/evidence/dfsk-gelora-e/gelora-004.md
//  Byte convention: b0 is the first data byte. le16(n) is bytes n and n+1,
//  little-endian, which is how the evidence and notes.tsv both write it.
//
//  THREADING. feed() runs on the CAN reader task (core 0) and must never
//  block: no filesystem, no network, no mutex a core 1 task can hold. It takes
//  a spinlock for the few microseconds it needs to store a reading, the same
//  way CanManager guards its status block. snapshot() takes the same lock from
//  the web task.
// =============================================================================
#pragma once

#include <Arduino.h>
#include "CanManager.h"
#include "JsonWriter.h"

namespace SignalDecoder {

enum Signal : uint8_t {
    SIG_SPEED = 0,    // km/jam, 0x18FFDC01 le16(4) / 256
    SIG_ODOMETER,     // km,     0x18FEDCD5 le16(1)
    SIG_SOC,          // %,      0x0CFF7D03 b1 * 0,5
    SIG_PACK,         // V,      0x0CFF7E03 le16(2)
    SIG_CURRENT,      // A,      0x0CFF7E03 le16(4) - 1000, negative under regen
    SIG_TEMP_MAX,     // °C,     0x0CFF7E03 b6 - 40
    SIG_TEMP_MIN,     // °C,     0x0CFF7E03 b7 - 40
    SIG_RPM,          // rpm,    0x0CFF7902 le16(4) - 12000
    SIGNAL_COUNT
};

struct Reading {
    float    value;
    uint32_t at_ms;     // millis() of the frame that produced it
    bool     ever;      // false until the first frame carrying it arrives
};

// Every frame passes through here. Frames that carry nothing in the table cost
// one comparison against the sorted identifier list and return.
void feed(const CanFrame& frame);

// A consistent copy of all readings. `out` must hold SIGNAL_COUNT entries.
void snapshot(Reading* out);

// Appends `"signals":{"speed":{"v":42.5,"age_ms":120},...}`. A signal that has
// never arrived is emitted as null rather than as zero, because zero is a
// reading and "nothing yet" is not.
void writeJson(JsonWriter& j);

// The short name used in JSON and on both dashboards.
const char* name(Signal s);

// The unit, for the dashboard that lists everything.
const char* unit(Signal s);

// Decimal places the value deserves. Speed is 1/256 km/jam under the skin, but
// nobody drives to a hundredth of a km/jam.
uint8_t decimals(Signal s);

}  // namespace SignalDecoder
