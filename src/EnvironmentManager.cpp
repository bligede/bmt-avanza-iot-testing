#include "EnvironmentManager.h"
#include "Logger.h"

namespace {

const char* TAG = "ENV";

EnvReading s_reading = {false, 0.0f, 0.0f, 0};
EnvStats   s_stats   = {};
#if ENABLE_DHT22
uint32_t   s_last_sample_ms = 0;
#endif

#if ENABLE_DHT22

// Waits for the pin to reach `level`, up to `timeout_us`. Returns the elapsed
// microseconds, or -1 on timeout.
int32_t waitLevel(uint8_t level, uint32_t timeout_us) {
    const uint32_t start = micros();
    while (digitalRead(DHT_PIN) != level) {
        if (micros() - start > timeout_us) return -1;
    }
    return static_cast<int32_t>(micros() - start);
}

// One DHT22 transaction. ~5 ms with interrupts off — the sensor's protocol is
// pure timing and an interrupt in the middle corrupts the frame.
bool readSensor(float* temperature, float* humidity) {
    uint8_t bytes[5] = {0};

    // Start signal: pull low for at least 1 ms, then release.
    pinMode(DHT_PIN, OUTPUT);
    digitalWrite(DHT_PIN, LOW);
    delayMicroseconds(1200);
    digitalWrite(DHT_PIN, HIGH);
    delayMicroseconds(30);
    pinMode(DHT_PIN, INPUT_PULLUP);

    noInterrupts();
    bool ok = true;

    // Response: 80 us low, 80 us high.
    if (waitLevel(LOW, 100) < 0)  ok = false;
    if (ok && waitLevel(HIGH, 100) < 0) ok = false;
    if (ok && waitLevel(LOW, 100) < 0)  ok = false;

    // 40 bits. Each starts with ~50 us low; the high that follows is ~26 us
    // for a zero and ~70 us for a one.
    if (ok) {
        for (int i = 0; i < 40 && ok; ++i) {
            if (waitLevel(HIGH, 100) < 0) { ok = false; break; }
            const int32_t highLen = waitLevel(LOW, 150);
            if (highLen < 0) { ok = false; break; }
            bytes[i / 8] <<= 1;
            if (highLen > 45) bytes[i / 8] |= 1;
        }
    }
    interrupts();

    if (!ok) {
        ++s_stats.read_errors;
        return false;
    }

    const uint8_t sum = static_cast<uint8_t>(bytes[0] + bytes[1] + bytes[2] + bytes[3]);
    if (sum != bytes[4]) {
        ++s_stats.checksum_errors;
        return false;
    }

    *humidity = static_cast<float>((bytes[0] << 8) | bytes[1]) / 10.0f;
    int16_t raw = static_cast<int16_t>(((bytes[2] & 0x7F) << 8) | bytes[3]);
    *temperature = static_cast<float>(raw) / 10.0f;
    if (bytes[2] & 0x80) *temperature = -*temperature;

    // Reject readings outside the datasheet range: a garbled frame that happens
    // to checksum is rare but not impossible.
    if (*humidity < 0.0f || *humidity > 100.0f ||
        *temperature < -40.0f || *temperature > 80.0f) {
        ++s_stats.read_errors;
        return false;
    }
    return true;
}

#endif  // ENABLE_DHT22

}  // namespace

namespace EnvironmentManager {

void begin() {
#if ENABLE_DHT22
    pinMode(DHT_PIN, INPUT_PULLUP);
    memset(&s_stats, 0, sizeof(s_stats));
    // The sensor needs a moment after power-up before it answers.
    s_last_sample_ms = millis();
    LOG_I(TAG, "DHT22 on GPIO%d, sampling every %lu ms",
          DHT_PIN, (unsigned long)DHT_SAMPLE_INTERVAL_MS);
    LOG_I(TAG, "Not in the production payload (Blueprint §7.1 does not list it)");
#else
    LOG_I(TAG, "DHT22 disabled (ENABLE_DHT22 = 0)");
#endif
}

bool poll() {
#if ENABLE_DHT22
    const uint32_t now = millis();
    if (now - s_last_sample_ms < DHT_SAMPLE_INTERVAL_MS) return false;
    s_last_sample_ms = now;

    float t = 0.0f, h = 0.0f;
    if (readSensor(&t, &h)) {
        s_reading.valid         = true;
        s_reading.temperature_c = t;
        s_reading.humidity_pct  = h;
        s_reading.at_ms         = now;
        ++s_stats.reads_ok;
        LOG_D(TAG, "%.1f C, %.1f %%RH", (double)t, (double)h);
        return true;
    }

    // Two consecutive failures mean the reading is no longer trustworthy.
    if (s_reading.valid && (now - s_reading.at_ms) > (DHT_SAMPLE_INTERVAL_MS * 2)) {
        s_reading.valid = false;
        LOG_W(TAG, "DHT22 not responding — reading marked invalid");
    }
    return false;
#else
    return false;
#endif
}

EnvReading reading() { return s_reading; }
EnvStats   stats()   { return s_stats; }

bool isEnabled() {
#if ENABLE_DHT22
    return true;
#else
    return false;
#endif
}

}  // namespace EnvironmentManager
