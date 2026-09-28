#include "SignalDecoder.h"
#include "ArgoMeter.h"

namespace SignalDecoder {
namespace {

portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
Reading      s_read[SIGNAL_COUNT] = {};

// One entry per signal, in enum order.
struct Def {
    uint32_t    id;
    const char* name;
    const char* unit;
    uint8_t     decimals;
    uint8_t     need_dlc;   // the payload must be at least this long
};

const Def DEFS[SIGNAL_COUNT] = {
    { 0x18FFDC01, "speed",       "km/jam", 0, 6 },
    { 0x18FEDCD5, "odometer",    "km",     0, 3 },
    { 0x0CFF7D03, "soc",         "%",      0, 2 },
    { 0x0CFF7E03, "pack",        "V",      1, 4 },
    { 0x0CFF7E03, "current",     "A",      0, 6 },
    { 0x0CFF7E03, "battTempMax", "°C", 0, 7 },
    { 0x0CFF7E03, "battTempMin", "°C", 0, 8 },
    { 0x0CFF7902, "rpm",         "rpm",    0, 6 },
};

inline uint16_t le16(const uint8_t* d, uint8_t i) {
    return (uint16_t)d[i] | ((uint16_t)d[i + 1] << 8);
}

inline void store(Signal s, float v, uint32_t at) {
    s_read[s].value = v;
    s_read[s].at_ms = at;
    s_read[s].ever  = true;
}

}  // namespace

void feed(const CanFrame& frame) {
    // Every mapping in the table is an extended identifier. Checking this first
    // means a standard-frame bus costs one comparison per frame.
    if (!frame.extended) return;

    const uint8_t* d  = frame.data;
    const uint8_t  n  = frame.dlc;
    const uint32_t at = frame.rx_millis;

    // Only the four identifiers that carry anything reach the lock.
    switch (frame.id) {
        case 0x18FFDC01: {
            if (n < DEFS[SIG_SPEED].need_dlc) return;
            const float kmh = le16(d, 4) / 256.0f;
            portENTER_CRITICAL(&s_mux);
            store(SIG_SPEED, kmh, at);
            portEXIT_CRITICAL(&s_mux);
            // The meter runs off the same frame that carries speed, so waiting
            // time accumulates on the bus's own clock and stops dead when the
            // bus does. A meter that keeps charging while the vehicle has
            // stopped reporting is charging for nothing.
            ArgoMeter::onSpeed(kmh, at);
            return;
        }
        case 0x18FEDCD5: {
            if (n < DEFS[SIG_ODOMETER].need_dlc) return;
            const float km = (float)le16(d, 1);
            portENTER_CRITICAL(&s_mux);
            store(SIG_ODOMETER, km, at);
            portEXIT_CRITICAL(&s_mux);
            // Billed distance comes from the ODOMETER, not from integrating
            // speed: the odometer is the vehicle's own count and does not drift
            // with however often a frame arrives.
            ArgoMeter::onOdometer(km, at);
            return;
        }
        case 0x0CFF7D03: {
            if (n < DEFS[SIG_SOC].need_dlc) return;
            portENTER_CRITICAL(&s_mux);
            store(SIG_SOC, d[1] * 0.5f, at);
            portEXIT_CRITICAL(&s_mux);
            return;
        }
        case 0x0CFF7902: {
            if (n < DEFS[SIG_RPM].need_dlc) return;
            portENTER_CRITICAL(&s_mux);
            store(SIG_RPM, (float)((int32_t)le16(d, 4) - 12000), at);
            portEXIT_CRITICAL(&s_mux);
            return;
        }
        case 0x0CFF7E03: {
            // One frame, four readings, each with its own length requirement so
            // a short payload gives what it can rather than nothing.
            portENTER_CRITICAL(&s_mux);
            if (n >= DEFS[SIG_PACK].need_dlc)     store(SIG_PACK,     (float)le16(d, 2), at);
            if (n >= DEFS[SIG_CURRENT].need_dlc)  store(SIG_CURRENT,  (float)((int32_t)le16(d, 4) - 1000), at);
            if (n >= DEFS[SIG_TEMP_MAX].need_dlc) store(SIG_TEMP_MAX, (float)((int16_t)d[6] - 40), at);
            if (n >= DEFS[SIG_TEMP_MIN].need_dlc) store(SIG_TEMP_MIN, (float)((int16_t)d[7] - 40), at);
            portEXIT_CRITICAL(&s_mux);
            return;
        }
        default:
            return;
    }
}

void snapshot(Reading* out) {
    portENTER_CRITICAL(&s_mux);
    for (uint8_t i = 0; i < SIGNAL_COUNT; ++i) out[i] = s_read[i];
    portEXIT_CRITICAL(&s_mux);
}

void writeJson(JsonWriter& j) {
    Reading snap[SIGNAL_COUNT];
    snapshot(snap);
    const uint32_t now = millis();

    j.add("\"signals\":{");
    for (uint8_t i = 0; i < SIGNAL_COUNT; ++i) {
        if (i) j.add(",");
        j.add("\"%s\":", DEFS[i].name);
        if (!snap[i].ever) {
            // Never arrived is not zero. A dashboard that cannot tell the two
            // apart will draw a needle at rest and call it a reading.
            j.add("null");
            continue;
        }
        j.add("{\"v\":%.*f,\"age_ms\":%lu}",
              (int)DEFS[i].decimals, (double)snap[i].value,
              (unsigned long)(now - snap[i].at_ms));
    }
    j.add("}");
}

const char* name(Signal s)     { return DEFS[s].name; }
const char* unit(Signal s)     { return DEFS[s].unit; }
uint8_t     decimals(Signal s) { return DEFS[s].decimals; }

}  // namespace SignalDecoder
