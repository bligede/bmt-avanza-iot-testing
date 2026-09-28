// =============================================================================
//  ArgoMeter.h: the taxi meter, on the device, in quantities only.
//
//  WHAT THIS DOES AND DOES NOT COMPUTE. It counts the two things a fare is made
//  of: billed distance and waiting time. It does NOT know what a kilometre
//  costs. Rupiah are computed by the screen, from mdt-ui/tariff.js, which is the
//  one file in the whole system where a price is written down. Two places that
//  both know the price are two places that can disagree about a passenger's
//  money.
//
//  THE BILLING RULE (D-011 in project-mdt-tds, from the SELARIDE documentation):
//  the basis is "kilometer isi, yaitu kilometer berbayar", LOADED kilometres.
//  So the meter only counts while a passenger is aboard, and on this device
//  that is a button someone presses, because nothing here can see a passenger.
//
//  DISTANCE COMES FROM THE ODOMETER, not from integrating speed. The odometer is
//  the vehicle's own count: it does not drift with how often a frame arrives, it
//  does not accumulate error over a shift, and it survives a gap in reception.
//  The MDT frame integrates speed because it has no odometer; this device has
//  one, proven, so it uses the better source.
//
//  DISTANCE AND TIME ARE NEVER BILLED TOGETHER. Below the waiting threshold the
//  waiting clock runs and the distance accumulator is frozen. A minute crawling
//  at 4 km/jam covers 67 metres, and billing both would charge for it twice.
//
//  NOT PERSISTED. A reboot mid-trip loses the running fare and the driver has to
//  press Mulai again. Deliberate for the 29 Sep road test: the alternative is
//  writing to the filesystem that holds the CAN captures, and no display feature
//  is worth putting those at risk.
// =============================================================================
#pragma once

#include <Arduino.h>
#include "JsonWriter.h"

namespace ArgoMeter {

// Below this speed the waiting clock runs instead of the distance.
// Source: Surya Wirasdyartha, WhatsApp 26 Sep 2026. The rate that goes with it
// was revised to Rp 1.000 per 60 seconds by the operator on 28 Sep 2026, is
// still NOT in the SELARIDE documentation, and is still marked unapproved on
// the screen: revising a figure is not the same as approving it.
constexpr float WAIT_BELOW_KMH = 5.0f;

// Distance is billed a WHOLE kilometre at a time, so the screen shows what was
// actually billed and a passenger multiplying it by the rate lands on the same
// number. Operator instruction, 28 September 2026: the argometer reads as a
// whole number. It also matches the source, because the DFSK odometer has a
// resolution of one kilometre and a tenth was never something this could see.
//
// What it costs, and it is a revenue decision rather than a display one: a trip
// shorter than one kilometre now bills nothing for distance.
constexpr float KM_STEP = 1.0f;

// Waiting has no tick at all. It is reported in milliseconds and the screen
// runs it as a stopwatch, so the money follows the clock the passenger is
// watching instead of jumping a tenth of a minute at a time.

// A gap longer than this is reception stopping, not the vehicle waiting. Time
// that nobody observed is not billed.
constexpr uint32_t MAX_GAP_MS = 2000;

// Pressed by a person, on the argo screen. Returns false if it was already in
// that state, so the screen can say so rather than silently doing nothing.
bool start();
bool stop();

bool running();

// Fed from the CAN reader task, by SignalDecoder, on the frames that carry them.
void onSpeed(float kmh, uint32_t at_ms);
void onOdometer(float km, uint32_t at_ms);

// Appends `"argo":{...}`: the state, the two billed quantities, the trip clock,
// and whether the distance has an odometer to count from yet.
void writeJson(JsonWriter& j);

}  // namespace ArgoMeter
