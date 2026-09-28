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
// is NOT in the SELARIDE documentation and is marked unapproved on the screen.
constexpr float WAIT_BELOW_KMH = 5.0f;

// One tick of each quantity, so the screen shows what was actually billed and a
// passenger multiplying it by the rate lands on the same number.
constexpr float KM_STEP  = 0.1f;
constexpr float MIN_STEP = 0.1f;

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
