#include "ArgoMeter.h"

namespace ArgoMeter {
namespace {

portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

bool     s_running   = false;
bool     s_waiting   = false;   // speed below the threshold, while running
bool     s_anchored  = false;   // an odometer reading has been seen since start
float    s_odo_prev  = 0.0f;    // last odometer value, running or not
float    s_km        = 0.0f;    // billed distance, kilometres
uint32_t s_wait_ms   = 0;       // billed waiting time
uint32_t s_started   = 0;       // millis() when Mulai was pressed
uint32_t s_speed_at  = 0;       // arrival of the last speed frame

inline float floorStep(float v, float step) {
    if (v <= 0.0f) return 0.0f;
    // +1e-6 so a value that is exactly on a tick is not pushed down by the
    // float representation of, say, 0.7 being 0.69999999.
    return floorf(v / step + 1e-6f) * step;
}

}  // namespace

bool start() {
    portENTER_CRITICAL(&s_mux);
    const bool was = s_running;
    if (!was) {
        s_running  = true;
        s_waiting  = false;
        s_km       = 0.0f;
        s_wait_ms  = 0;
        s_started  = millis();
        // If the odometer has already been seen, anchor immediately; otherwise
        // the first odometer frame after this will anchor. Either way no
        // distance is billed until there is a baseline to measure from.
        s_anchored = (s_odo_prev > 0.0f);
    }
    portEXIT_CRITICAL(&s_mux);
    return !was;
}

bool stop() {
    portENTER_CRITICAL(&s_mux);
    const bool was = s_running;
    s_running = false;
    s_waiting = false;
    portEXIT_CRITICAL(&s_mux);
    return was;
}

bool running() {
    portENTER_CRITICAL(&s_mux);
    const bool r = s_running;
    portEXIT_CRITICAL(&s_mux);
    return r;
}

void onSpeed(float kmh, uint32_t at_ms) {
    portENTER_CRITICAL(&s_mux);
    const uint32_t prev = s_speed_at;
    s_speed_at = at_ms;

    if (s_running) {
        const bool waiting_now = (kmh < WAIT_BELOW_KMH);
        // Bill the interval that just ENDED, under the state it was in. Using
        // the new state would bill the moment of stopping as waiting before any
        // waiting happened.
        if (s_waiting && prev != 0) {
            const uint32_t gap = at_ms - prev;
            if (gap <= MAX_GAP_MS) s_wait_ms += gap;
        }
        s_waiting = waiting_now;
    } else {
        s_waiting = false;
    }
    portEXIT_CRITICAL(&s_mux);
}

void onOdometer(float km, uint32_t at_ms) {
    (void)at_ms;
    portENTER_CRITICAL(&s_mux);
    const float prev = s_odo_prev;
    s_odo_prev = km;

    if (s_running) {
        if (!s_anchored) {
            // First odometer since Mulai: this is the baseline, and nothing is
            // billed for the distance covered before it arrived.
            s_anchored = true;
        } else if (prev > 0.0f) {
            const float delta = km - prev;
            // Forward movement only. A backwards or absurd jump is a decode
            // glitch or a counter wrap, and neither is distance a passenger owes.
            if (delta > 0.0f && delta < 10.0f && !s_waiting) s_km += delta;
        }
    }
    portEXIT_CRITICAL(&s_mux);
}

void writeJson(JsonWriter& j) {
    portENTER_CRITICAL(&s_mux);
    const bool     running  = s_running;
    const bool     waiting  = s_waiting;
    const bool     anchored = s_anchored;
    const float    km       = s_km;
    const uint32_t wait_ms  = s_wait_ms;
    const uint32_t started  = s_started;
    const float    odo      = s_odo_prev;
    portEXIT_CRITICAL(&s_mux);

    const float km_billed  = floorStep(km, KM_STEP);
    const float min_billed = floorStep(wait_ms / 60000.0f, MIN_STEP);

    j.add("\"argo\":{\"running\":%s,\"waiting\":%s,\"anchored\":%s,"
          "\"km_isi\":%.1f,\"wait_min\":%.1f,\"elapsed_s\":%lu,\"odometer\":%.0f}",
          running  ? "true" : "false",
          waiting  ? "true" : "false",
          anchored ? "true" : "false",
          (double)km_billed, (double)min_billed,
          (unsigned long)(running ? (millis() - started) / 1000 : 0),
          (double)odo);
}

}  // namespace ArgoMeter
