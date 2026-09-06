#include "FanManager.h"

#include "EnvironmentManager.h"
#include "Logger.h"

// A configuration where the fan can never switch off is worse than no fan
// control at all, so this is a build error rather than a runtime surprise.
static_assert(FAN_OFF_TEMP < FAN_ON_TEMP,
              "FAN_OFF_TEMP must be below FAN_ON_TEMP or the hysteresis inverts");

namespace {

const char* TAG = "FAN";

FanMode  s_mode        = static_cast<FanMode>(FAN_MODE_DEFAULT);
bool     s_running     = false;
uint32_t s_last_change = 0;
uint32_t s_transitions = 0;
uint32_t s_run_ms      = 0;
uint32_t s_run_since   = 0;
float    s_last_temp   = 0.0f;
bool     s_temp_valid  = false;

void drive(bool on) {
    if (on == s_running) return;
    // Respect the minimum dwell so a temperature hovering at the threshold
    // cannot toggle the MOSFET every poll.
    if (millis() - s_last_change < FAN_MIN_STATE_MS) return;

    s_running     = on;
    s_last_change = millis();
    ++s_transitions;
    digitalWrite(FAN_PIN, on ? HIGH : LOW);

    if (on) {
        s_run_since = millis();
    } else if (s_run_since != 0) {
        s_run_ms += millis() - s_run_since;
        s_run_since = 0;
    }
    LOG_I(TAG, "%s (%.1f C)", on ? "ON" : "OFF", (double)s_last_temp);
}

// Prefers the DHT22 when it is enabled and reading, because it measures the
// enclosure. Falls back to the SoC die sensor, which runs warmer than ambient.
bool readTemperature(float* out) {
#if ENABLE_DHT22 && !FAN_SENSOR_INTERNAL
    EnvReading r = EnvironmentManager::reading();
    if (r.valid) { *out = r.temperature_c; return true; }
#endif
    const float t = temperatureRead();   // ESP32-S3 internal sensor, degrees C
    if (t < -40.0f || t > 125.0f) return false;
    *out = t;
    return true;
}

}  // namespace

namespace FanManager {

void begin() {
#if ENABLE_FAN
    pinMode(FAN_PIN, OUTPUT);
    digitalWrite(FAN_PIN, LOW);
    s_running     = false;
    s_last_change = millis();
    LOG_I(TAG, "Ready on GPIO%d, mode %s", FAN_PIN, modeName(s_mode));
    LOG_W(TAG, "Thresholds are PLACEHOLDERS: on %.1f C / off %.1f C "
               "— TODO: VALIDASI TERMAL",
          (double)FAN_ON_TEMP, (double)FAN_OFF_TEMP);
#else
    LOG_I(TAG, "Fan control disabled");
#endif
}

void poll() {
#if ENABLE_FAN
    s_temp_valid = readTemperature(&s_last_temp);

    switch (s_mode) {
    case FanMode::Off:
        drive(false);
        break;
    case FanMode::ForcedOn:
        drive(true);
        break;
    case FanMode::Auto:
        if (!s_temp_valid) {
            // No trustworthy temperature. Leave the fan as it is rather than
            // guessing in either direction, and say so.
            static uint32_t lastWarn = 0;
            if (millis() - lastWarn > 60000) {
                lastWarn = millis();
                LOG_W(TAG, "No valid temperature — holding the fan at %s",
                      s_running ? "ON" : "OFF");
            }
            break;
        }
        if (!s_running && s_last_temp >= FAN_ON_TEMP)  drive(true);
        else if (s_running && s_last_temp <= FAN_OFF_TEMP) drive(false);
        break;
    }
#endif
}

void setMode(FanMode m) {
    if (m == s_mode) return;
    s_mode = m;
    LOG_I(TAG, "Mode -> %s", modeName(m));
    poll();
}

FanMode mode()      { return s_mode; }
bool    isRunning() { return s_running; }

FanStats stats() {
    FanStats s;
    s.running            = s_running;
    s.mode               = s_mode;
    s.last_temperature_c = s_last_temp;
    s.temperature_valid  = s_temp_valid;
    s.transitions        = s_transitions;
    uint32_t total = s_run_ms;
    if (s_run_since != 0) total += millis() - s_run_since;
    s.run_seconds = total / 1000U;
    return s;
}

const char* modeName(FanMode m) {
    switch (m) {
        case FanMode::Off:      return "OFF";
        case FanMode::Auto:     return "AUTO";
        case FanMode::ForcedOn: return "FORCED_ON";
        default:                return "?";
    }
}

}  // namespace FanManager
