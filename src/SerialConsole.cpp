#include "SerialConsole.h"

#include <LittleFS.h>

#include "Logger.h"
#include "CanManager.h"
#include "RawCanLogger.h"
#include "WifiManager.h"
#include "WebDashboard.h"
#include "WatchdogManager.h"
#include "GpsManager.h"
#include "EnvironmentManager.h"
#include "FanManager.h"

namespace {

const char* TAG = "CONSOLE";

char    s_line[80];
uint8_t s_len = 0;

bool matches(const char* line, const char* cmd) {
    return strncmp(line, cmd, strlen(cmd)) == 0;
}

void listDir(const char* path) {
    File dir = LittleFS.open(path);
    if (!dir || !dir.isDirectory()) {
        Serial.printf("  %s: not present\r\n", path);
        return;
    }
    uint32_t count = 0, bytes = 0;
    File e = dir.openNextFile();
    while (e) {
        Serial.printf("  %-28s %9u B\r\n", e.name(), (unsigned)e.size());
        bytes += static_cast<uint32_t>(e.size());
        ++count;
        e.close();
        e = dir.openNextFile();
    }
    dir.close();
    Serial.printf("  %lu file(s), %lu bytes\r\n",
                  (unsigned long)count, (unsigned long)bytes);
}

void dumpFile(const char* path) {
    if (!LittleFS.exists(path)) {
        Serial.printf("No such file: %s\r\n", path);
        return;
    }
    File f = LittleFS.open(path, "r");
    if (!f) { Serial.println("Cannot open"); return; }
    Serial.printf("---- BEGIN %s (%u bytes) ----\r\n", path, (unsigned)f.size());
    char buf[128];
    while (f.available()) {
        const size_t n = f.readBytes(buf, sizeof(buf) - 1);
        buf[n] = '\0';
        Serial.print(buf);
    }
    f.close();
    Serial.printf("\r\n---- END %s ----\r\n", path);
}

void showIds() {
    const uint16_t n = CanManager::seenIdCount();
    Serial.printf("Distinct CAN IDs seen: %u\r\n", (unsigned)n);
    if (n == 0) {
        Serial.println("  (none)");
        Serial.println("  If the bench test passed, this is a fact about the");
        Serial.println("  car, not the device: many vehicles keep the OBD-II");
        Serial.println("  CAN channel silent until a scan tool asks. We never");
        Serial.println("  ask — we are listen-only.");
        return;
    }
    Serial.println("        ID     frames");
    for (uint16_t i = 0; i < n; ++i) {
        uint32_t id = 0, count = 0;
        if (CanManager::seenIdAt(i, &id, &count)) {
            if (id > 0x7FF) {
                Serial.printf("  0x%08lX  %9lu  EXT\r\n",
                              (unsigned long)id, (unsigned long)count);
            } else {
                Serial.printf("     0x%03lX  %9lu\r\n",
                              (unsigned long)id, (unsigned long)count);
            }
        }
    }
}

void execute(char* line) {
    while (*line == ' ') ++line;
    size_t n = strlen(line);
    while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\r')) line[--n] = '\0';
    if (n == 0) return;

    if (matches(line, "help") || matches(line, "?")) {
        SerialConsole::printHelp();

    } else if (matches(line, "status")) {
        SerialConsole::printStatus();

    } else if (matches(line, "ids")) {
        showIds();

    } else if (matches(line, "capture serial")) {
        RawCanLogger::setSink(RAWLOG_SINK_SERIAL);
        Serial.println("Capture -> serial. Needs LOG_LEVEL=DEBUG to show frames.");

    } else if (matches(line, "capture file")) {
        RawCanLogger::setSink(RAWLOG_SINK_FILE);
        Serial.printf("Capture -> %s\r\n", RawCanLogger::currentPath());

    } else if (matches(line, "capture both")) {
        RawCanLogger::setSink(RAWLOG_SINK_BOTH);
        Serial.println("Capture -> serial + file");

    } else if (matches(line, "capture off")) {
        RawCanLogger::setSink(RAWLOG_SINK_NONE);
        Serial.printf("Capture off. %lu frames, %lu bytes written.\r\n",
                      (unsigned long)RawCanLogger::framesWritten(),
                      (unsigned long)RawCanLogger::bytesWritten());

    } else if (matches(line, "capture status")) {
        Serial.printf("sink=%u frames=%lu bytes=%lu path=%s\r\n",
                      (unsigned)RawCanLogger::sink(),
                      (unsigned long)RawCanLogger::framesWritten(),
                      (unsigned long)RawCanLogger::bytesWritten(),
                      RawCanLogger::currentPath());

    } else if (matches(line, "ls")) {
        listDir(RAWLOG_DIR);

    } else if (matches(line, "cat ")) {
        dumpFile(line + 4);

    } else if (matches(line, "clearcaptures")) {
        RawCanLogger::clearCaptures();

    } else if (matches(line, "wifi")) {
        const WifiStats w = WifiManager::stats();
        Serial.printf("state=%s ssid=%s ip=%s rssi=%ld dBm\r\n",
                      WifiManager::stateName(WifiManager::state()),
                      w.ssid, w.ip, (long)w.rssi);
        if (WifiManager::isConnected()) {
            Serial.printf("Dashboard: http://%s/\r\n", w.ip);
        } else {
            Serial.println("Not connected. ESP32-S3 is 2.4 GHz only — check the");
            Serial.println("hotspot is not 5 GHz, and that SSID/PASS match.");
        }

    } else if (matches(line, "gps")) {
        const GnssFix  f = GpsManager::fix();
        const GpsStats g = GpsManager::stats();
        if (GpsManager::hasFreshFix()) {
            Serial.printf("fix=YES lat=%.6f lon=%.6f sats=%u hdop=%.1f "
                          "sog=%.1f km/h\r\n",
                          f.lat, f.lon, (unsigned)f.satellites,
                          (double)f.hdop, (double)f.speed_kmh);
        } else {
            Serial.printf("fix=NO sats=%u quality=%u\r\n",
                          (unsigned)f.satellites, (unsigned)f.fix_quality);
        }
        Serial.printf("bytes=%lu lines=%lu gga_rmc=%lu badcrc=%lu "
                      "utc_valid=%s epoch=%llu\r\n",
                      (unsigned long)g.bytes_received,
                      (unsigned long)g.lines_seen,
                      (unsigned long)g.sentences_ok,
                      (unsigned long)g.sentences_bad_checksum,
                      f.time_valid ? "yes" : "no",
                      (unsigned long long)f.epoch);

        // Say what the numbers mean, in the order that narrows fastest.
        // GpsManager::isSilent() is the single source of truth for "is there a
        // module at all". Recomputing that judgement here would let the console
        // and the dashboard disagree about the same board.
        const uint32_t up_s = millis() / 1000U;

        if (GpsManager::isSilent()) {
            if (g.bytes_received == 0) {
                Serial.println("-> NO BYTES at all on the wire.");
            } else {
                Serial.printf("-> Only %lu byte(s) in %lu s — that is line "
                              "noise, not a module.\r\n",
                              (unsigned long)g.bytes_received,
                              (unsigned long)up_s);
            }
            Serial.println("   Wiring or power. GPS TX must reach GPIO18");
            Serial.println("   (module TX -> ESP32 RX, not RX -> RX), and the");
            Serial.println("   module needs 3V3 and GND. Baud is irrelevant");
            Serial.println("   until a steady byte stream appears.");
        } else if (g.lines_seen == 0) {
            Serial.println("-> Steady byte stream, no complete lines.");
            Serial.println("   Wrong baud: the module talks, we listen at 9600.");
        } else if (g.sentences_ok == 0 && g.sentences_bad_checksum > 0) {
            Serial.println("-> Lines arrive, every checksum fails. Baud close");
            Serial.println("   but wrong, or a noisy line.");
        } else if (g.sentences_ok == 0) {
            Serial.println("-> Lines parse but none are GGA/RMC. The module");
            Serial.println("   emits other sentence types only.");
        } else if (!GpsManager::hasFreshFix()) {
            Serial.println("-> Module healthy. No fix yet — needs sky view.");
        }

    } else if (matches(line, "env")) {
        const EnvReading e = EnvironmentManager::reading();
        const EnvStats   s = EnvironmentManager::stats();
        if (!EnvironmentManager::isEnabled()) {
            Serial.println("DHT22 disabled (ENABLE_DHT22 = 0)");
        } else if (e.valid) {
            Serial.printf("temp=%.1f C humidity=%.1f %%RH (age %lu ms)\r\n",
                          (double)e.temperature_c, (double)e.humidity_pct,
                          (unsigned long)(millis() - e.at_ms));
        } else {
            Serial.println("No valid reading. Check DATA -> GPIO15 and the");
            Serial.println("pull-up; some modules need an external 10k.");
        }
        Serial.printf("reads_ok=%lu read_errors=%lu checksum_errors=%lu\r\n",
                      (unsigned long)s.reads_ok, (unsigned long)s.read_errors,
                      (unsigned long)s.checksum_errors);

    } else if (matches(line, "fan auto")) {
        FanManager::setMode(FanMode::Auto);
    } else if (matches(line, "fan on")) {
        FanManager::setMode(FanMode::ForcedOn);
    } else if (matches(line, "fan off")) {
        FanManager::setMode(FanMode::Off);
    } else if (matches(line, "fan")) {
        const FanStats f = FanManager::stats();
        Serial.printf("mode=%s running=%s temp=%.1f C (valid=%s) "
                      "run=%lu s transitions=%lu\r\n",
                      FanManager::modeName(f.mode), f.running ? "yes" : "no",
                      (double)f.last_temperature_c,
                      f.temperature_valid ? "yes" : "no",
                      (unsigned long)f.run_seconds,
                      (unsigned long)f.transitions);
        Serial.printf("thresholds: on %.1f C / off %.1f C  "
                      "*** STILL PLACEHOLDERS ***\r\n",
                      (double)FAN_ON_TEMP, (double)FAN_OFF_TEMP);

    } else if (matches(line, "restart")) {
        Serial.println("Restarting...");
        Serial.flush();
        delay(300);
        ESP.restart();

    } else {
        Serial.printf("Unknown command: %s  (type 'help')\r\n", line);
    }
}

}  // namespace

namespace SerialConsole {

void begin() {
    s_len = 0;
    LOG_I(TAG, "Console ready — type 'help'");
}

void printHelp() {
    Serial.println();
    Serial.println("=== Avanza CAN bring-up console ==========================");
    Serial.println(" status            full status report");
    Serial.println(" ids               distinct CAN IDs seen, with counts");
    Serial.println(" gps               GNSS fix, ground speed, UTC");
    Serial.println(" env               DHT22 temperature and humidity");
    Serial.println(" fan               fan state and thresholds");
    Serial.println(" fan auto|on|off   fan mode");
    Serial.println(" wifi              WiFi state and dashboard URL");
    Serial.println("");
    Serial.println(" capture file      raw frames to LittleFS (default)");
    Serial.println(" capture serial    raw frames to this console (needs DEBUG)");
    Serial.println(" capture both");
    Serial.println(" capture off");
    Serial.println(" capture status");
    Serial.println(" ls                list capture files");
    Serial.println(" cat <path>        print a file — how you pull a capture");
    Serial.println(" clearcaptures     delete all capture files");
    Serial.println("");
    Serial.println(" restart");
    Serial.println("");
    Serial.println(" This console cannot enable CAN transmit or change the CAN");
    Serial.println(" bitrate. Those are not runtime options.");
    Serial.println("==========================================================");
    Serial.println();
}

void printStatus() {
    const CanStats  c = CanManager::stats();
    const WifiStats w = WifiManager::stats();

    LOG_I("STATUS", "------------ %s %s ------------", FW_NAME, FW_VERSION);
    LOG_I("STATUS", "unit=%s uptime=%lu s heap=%lu B reset=%s",
          UNIT_ID, (unsigned long)(millis() / 1000),
          (unsigned long)ESP.getFreeHeap(), WatchdogManager::lastResetReason());
    LOG_I("STATUS", "CAN   %s @ %lu kbps LISTEN-ONLY=%s",
          CanManager::state() == CanState::Running ? "RUNNING" : "DOWN",
          (unsigned long)(CanManager::bitrate() / 1000),
          CanManager::isListenOnlyLocked() ? "LOCKED" : "NO");
    // silenceMs() returns UINT32_MAX when no frame has EVER arrived. Printing
    // the raw number reads like a bug in a phase report; say what it means.
    const uint32_t sil = CanManager::silenceMs();
    char silBuf[24];
    if (sil == UINT32_MAX) snprintf(silBuf, sizeof(silBuf), "no frames yet");
    else                   snprintf(silBuf, sizeof(silBuf), "%lu ms", (unsigned long)sil);

    LOG_I("STATUS", "      rx=%lu drop=%lu missed=%lu err=%lu rec=%lu ids=%u silence=%s",
          (unsigned long)c.frames_received,
          (unsigned long)c.frames_dropped_queue,
          (unsigned long)c.rx_missed,
          (unsigned long)c.bus_errors,
          (unsigned long)c.recoveries,
          (unsigned)CanManager::seenIdCount(),
          silBuf);
    LOG_I("STATUS", "CAP   sink=%u frames=%lu bytes=%lu path=%s",
          (unsigned)RawCanLogger::sink(),
          (unsigned long)RawCanLogger::framesWritten(),
          (unsigned long)RawCanLogger::bytesWritten(),
          RawCanLogger::currentPath());
    const GnssFix    gf = GpsManager::fix();
    const EnvReading er = EnvironmentManager::reading();
    const FanStats   fs = FanManager::stats();
    LOG_I("STATUS", "GPS   fix=%s sats=%u sog=%.1f km/h sentences=%lu badcrc=%lu",
          GpsManager::hasFreshFix() ? "YES" : "NO",
          (unsigned)gf.satellites, (double)gf.speed_kmh,
          (unsigned long)GpsManager::stats().sentences_ok,
          (unsigned long)GpsManager::stats().sentences_bad_checksum);
    char encBuf[16];
    if (er.valid) snprintf(encBuf, sizeof(encBuf), "%.1f C", (double)er.temperature_c);
    else          snprintf(encBuf, sizeof(encBuf), "--");
    LOG_I("STATUS", "TEMP  enclosure=%s fan_sensor=%.1f C fan=%s/%s run=%lu s",
          encBuf,
          (double)fs.last_temperature_c,
          FanManager::modeName(fs.mode), fs.running ? "ON" : "OFF",
          (unsigned long)fs.run_seconds);
    LOG_I("STATUS", "WIFI  %s ip=%s rssi=%ld dBm  web_requests=%lu",
          WifiManager::stateName(WifiManager::state()), w.ip, (long)w.rssi,
          (unsigned long)WebDashboard::requestsServed());
    LOG_I("STATUS", "----------------------------------------------------");
}

void poll() {
    int budget = 64;
    while (Serial.available() > 0 && budget-- > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            if (s_len > 0) {
                s_line[s_len] = '\0';
                execute(s_line);
                s_len = 0;
            }
            continue;
        }
        if (s_len < sizeof(s_line) - 1) {
            s_line[s_len++] = c;
        } else {
            s_len = 0;
            Serial.println("Line too long");
        }
    }
}

}  // namespace SerialConsole
