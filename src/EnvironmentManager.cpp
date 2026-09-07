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

    // Idle level, sampled before the start pulse. With the pull-up on and the
    // sensor quiet this must be HIGH; a LOW here is an electrical fault and no
    // protocol timing will rescue it.
    s_stats.line_idle_high = (digitalRead(DHT_PIN) == HIGH);

    // Start signal: hold low for at least 1 ms, then release.
    //
    // The pin is left in open-drain mode by begin(), so releasing the line is a
    // single digitalWrite and nothing here calls pinMode.
    //
    // That is not a tidiness point, it is the bug that made this sensor look
    // dead. pinMode() on the ESP32-S3 costs tens of microseconds; calling it to
    // release the line pushed us past the sensor's 80 us response pulse, so the
    // handshake locked onto the first DATA bit instead. Every frame then came in
    // shifted by one bit. The checksum could not catch it — a one-bit shift
    // doubles each byte, and doubling is linear mod 256, so the sum still
    // matched — and the readings decoded as 124 %RH and 62.9 C.
    digitalWrite(DHT_PIN, LOW);
    delayMicroseconds(1200);

    // Interrupts go off BEFORE the release. A task switch landing between the
    // release and the first wait would swallow the whole 4 ms response and look
    // identical to a missing sensor.
    noInterrupts();
    digitalWrite(DHT_PIN, HIGH);

    int  stage = 0;   // 0 none, 1 no response, 2 handshake, 3 truncated
    int  bits  = 0;

    // Synchronise on pulse DURATION, not on counting edges.
    //
    // Counting edges assumed the first low after the release is always the
    // sensor's 80 us response. When the release ran long the response was over
    // before we looked, the count locked onto the first DATA bit instead, and
    // every frame arrived shifted by one bit. That is invisible to the
    // checksum: a one-bit shift doubles each byte, doubling is linear mod 256,
    // so the sum still matches. The readings decoded as 124 %RH and 62.9 C —
    // wrong, self-consistent, and impossible to spot from the counters.
    //
    // The response low is ~80 us and a bit low is ~50 us, so measuring the
    // first low tells us which one we are looking at.
    bool at_bit_high = false;   // true when already standing on a bit's rising edge

    if (waitLevel(HIGH, 300) < 0) {
        stage = 2;              // released, and something is still holding it down
    } else if (waitLevel(LOW, 300) < 0) {
        stage = 1;              // nothing ever pulled the line down
    } else {
        const int32_t lowLen = waitLevel(HIGH, 300);
        if (lowLen < 0) {
            stage = 2;
        } else if (lowLen >= 65) {
            // The sensor's response low. We are in the 80 us response high now;
            // bit 0 starts at the next falling edge.
            if (waitLevel(LOW, 300) < 0) stage = 2;
        } else {
            // Too short to be the response: it was bit 0's low, and we are
            // standing on bit 0's rising edge already.
            at_bit_high = true;
        }
    }

    if (stage == 0) {
        // 40 bits. Each starts with ~50 us low; the high that follows is ~26 us
        // for a zero and ~70 us for a one.
        //
        // The bit is read by SAMPLING at 45 us, not by timing the falling edge.
        // Timing the edge cost us the fortieth bit on every single read: after
        // the last bit the sensor releases the bus, so for that one bit there is
        // no falling edge to measure and the wait always timed out. Thirty-nine
        // bits would arrive, the frame would be discarded, and the failure
        // looked from the outside exactly like a sensor that was not there.
        //
        // Sampling also needs no resynchronisation after a zero: 45 us is
        // already past its 26 us high, so the pin reads low and the loop is
        // lined up for the next bit.
        for (; bits < 40; ++bits) {
            if (at_bit_high) at_bit_high = false;   // already at the rise
            else if (waitLevel(HIGH, 150) < 0) { stage = 3; break; }
            delayMicroseconds(45);
            bytes[bits / 8] <<= 1;
            if (digitalRead(DHT_PIN) == HIGH) {
                bytes[bits / 8] |= 1;
                // Resync on the falling edge — except after the final bit,
                // where none is coming.
                if (bits < 39 && waitLevel(LOW, 200) < 0) { stage = 3; break; }
            }
        }
    }
    interrupts();
    s_stats.last_bits = static_cast<uint8_t>(bits);
    memcpy(s_stats.last_frame, bytes, sizeof(bytes));   // decoded or not

    if (stage != 0) {
        ++s_stats.read_errors;
        if (stage == 1)      ++s_stats.fail_no_response;
        else if (stage == 2) ++s_stats.fail_handshake;
        else                 ++s_stats.fail_truncated;
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
        ++s_stats.fail_range;
        return false;
    }
    return true;
}

#endif  // ENABLE_DHT22

}  // namespace

namespace EnvironmentManager {

void begin() {
#if ENABLE_DHT22
    // Open drain with the pull-up on: the sensor and the MCU share the line and
    // neither may ever drive it high. It also keeps pinMode out of the timing
    // path in readSensor(), where it was costing us the first bit of the frame.
    pinMode(DHT_PIN, OUTPUT_OPEN_DRAIN | PULLUP);
    digitalWrite(DHT_PIN, HIGH);
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
