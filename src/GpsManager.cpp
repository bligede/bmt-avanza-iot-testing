#include "GpsManager.h"

#include <HardwareSerial.h>
#include "Logger.h"

namespace {

const char* TAG = "GPS";

HardwareSerial s_uart(GPS_UART_NUM);
GnssFix  s_fix;
GpsStats s_stats = {};

char     s_line[100];
uint8_t  s_len = 0;
bool     s_overflow = false;

}  // namespace

namespace GpsManager {

void begin() {
    LOG_I(TAG, "Initializing...");
    s_fix = NmeaParser::makeEmptyFix();
    memset(&s_stats, 0, sizeof(s_stats));
    s_len = 0;

    s_uart.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
#if GPS_TX_PIN >= 0
    LOG_I(TAG, "UART%d @ %d baud, RX=GPIO%d TX=GPIO%d",
          GPS_UART_NUM, GPS_BAUD, GPS_RX_PIN, GPS_TX_PIN);
#else
    LOG_I(TAG, "UART%d @ %d baud, RX=GPIO%d, TX not connected (receive only)",
          GPS_UART_NUM, GPS_BAUD, GPS_RX_PIN);
#endif
    LOG_I(TAG, "No fix yet — lat/lon report invalid until one is acquired");
}

bool poll() {
    bool consumed = false;

    // Bounded per call so one very chatty second cannot monopolise the task.
    int budget = 512;
    while (s_uart.available() > 0 && budget-- > 0) {
        const char c = static_cast<char>(s_uart.read());
        ++s_stats.bytes_received;   // counted before any interpretation
        s_stats.last_byte_ms = millis();

        if (c == '\n' || c == '\r') {
            if (s_len == 0) continue;
            s_line[s_len] = '\0';

            if (s_overflow) {
                // A sentence longer than the buffer is corrupt by definition.
                s_overflow = false;
                s_len = 0;
                continue;
            }

            const bool hadFix = s_fix.valid;
            const NmeaResult res = NmeaParser::apply(s_line, s_fix);
            s_len = 0;
            consumed = true;
            ++s_stats.lines_seen;   // every complete line, including ignored ones

            switch (res) {
                case NmeaResult::BadChecksum:
                    ++s_stats.sentences_bad_checksum;
                    break;
                case NmeaResult::Gga:
                case NmeaResult::Rmc:
                    ++s_stats.sentences_ok;
                    s_stats.last_sentence_ms = millis();
                    break;
                default:
                    break;
            }

            if (s_fix.valid) {
                s_stats.last_fix_ms = millis();
                if (!hadFix) {
                    ++s_stats.fixes;
                    s_stats.ever_fixed = true;
                    LOG_I(TAG, "Fix acquired: %.6f, %.6f (%u sats, HDOP %.1f)",
                          s_fix.lat, s_fix.lon,
                          (unsigned)s_fix.satellites, (double)s_fix.hdop);
                }
            } else if (hadFix) {
                ++s_stats.fix_lost;
                LOG_W(TAG, "Fix lost — lat/lon now report invalid");
            }

            // DIVERGENCE FROM THE FLEET FIRMWARE — the only one in this file.
            //
            // Upstream pushes GNSS time into TimeService, which arbitrates
            // between GNSS, NTP and modem-network sources for the telemetry
            // timestamp. This build has no telemetry and no clock service, so
            // there is nothing to arbitrate: the fix carries `epoch` and
            // `time_valid` and the dashboard reads them directly.
            //
            // If TimeService is ever needed here, take it from
            // bmt-can-bus-telemetry rather than reinventing it, and restore
            // this block instead of writing a second one.
            continue;
        }

        if (s_len < sizeof(s_line) - 1) {
            s_line[s_len++] = c;
        } else {
            s_overflow = true;
            s_len = 0;
        }
    }
    return consumed;
}

GnssFix fix() { return s_fix; }

bool hasFreshFix() {
    if (!s_fix.valid) return false;
    return (millis() - s_stats.last_fix_ms) < GPS_STALE_FIX_MS;
}

// "Silent" means NO BYTES on the wire — a wiring or power fault.
//
// It deliberately does not mean "no GGA/RMC". A module at the wrong baud, or
// one configured to emit only GSV/GSA/VTG, is talking perfectly well and needs
// an entirely different fix. Conflating the two sends a technician to check
// wiring that is already correct.
bool isSilent() {
    const uint32_t up_s = millis() / 1000U;
    if (up_s < 5) return false;              // too early to judge anything

    // Judge by RATE, not by "any byte at all".
    //
    // A previous version returned false as soon as ONE byte had ever arrived.
    // A floating RX pin picks up the odd spurious edge, so a single noise byte
    // flipped the module from "not connected" to "searching" permanently — and
    // the panel then contradicted its own diagnostic line underneath. A NEO-6M
    // at 9600 emits several hundred bytes a second; anything under a few per
    // second is noise, not a receiver.
    if ((s_stats.bytes_received / up_s) < 5) return true;

    // It was talking and then stopped — a module that lost power or a wire
    // that came loose mid-session.
    return (millis() - s_stats.last_byte_ms) > GPS_FIX_TIMEOUT_MS;
}

GpsStats stats() { return s_stats; }

void end() {
    s_uart.end();
    s_fix = NmeaParser::makeEmptyFix();
}

}  // namespace GpsManager
