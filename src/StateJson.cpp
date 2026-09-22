#include "StateJson.h"

#include <LittleFS.h>
#include <time.h>
#include "Config.h"
#include "CanManager.h"
#include "EnvironmentManager.h"
#include "FanManager.h"
#include "FrameRing.h"
#include "GpsManager.h"
#include "NotesStore.h"
#include "FrameStream.h"
#include "RawCanLogger.h"
#include "StatusLed.h"
#include "SystemHealth.h"
#include "WifiManager.h"

namespace {

void appendCan(JsonWriter& j) {
    const CanStats c   = CanManager::stats();
    const CanState st  = CanManager::state();
    const uint32_t sil = CanManager::silenceMs();

    j.add("\"can\":{\"running\":%s,\"state\":\"%s\",\"bitrate\":%lu,"
          "\"listen_only\":%s,\"rx\":%lu,\"drop\":%lu,\"missed\":%lu,"
          "\"err\":%lu,\"rec\":%lu,\"silence_ms\":%lu,"
          "\"uniq\":%u,\"idfull\":%s},",
          st == CanState::Running ? "true" : "false",
          st == CanState::Running       ? "RUNNING"
            : st == CanState::BusError      ? "BUS_ERROR"
            : st == CanState::InstallFailed ? "INSTALL_FAILED" : "DOWN",
          (unsigned long)CanManager::bitrate(),
          CanManager::isListenOnlyLocked() ? "true" : "false",
          (unsigned long)c.frames_received,
          (unsigned long)c.frames_dropped_queue,
          (unsigned long)c.rx_missed,
          (unsigned long)c.bus_errors,
          (unsigned long)c.recoveries,
          (unsigned long)(sil == UINT32_MAX ? 0 : sil),
          // In the first object written, so it survives a truncation that cuts
          // the identifier array below. The page compares the two.
          (unsigned)c.unique_ids_seen,
          CanManager::seenIdOverflow() ? "true" : "false");
}

// Every identifier with its latest payload. The page renders this as the main
// table: bytes in hex and decimal, the ones that just changed lit up. Payload
// is compact hex with no spaces — 16 characters instead of 23 per frame, which
// across 128 identifiers is the difference between fitting and not.
void appendIds(JsonWriter& j) {
    j.add("\"ids\":[");
    const uint16_t n = CanManager::seenIdCount();
    bool first = true;
    for (uint16_t i = 0; i < n && !j.overflow(); ++i) {
        SeenIdView v;
        if (!CanManager::seenIdSnapshot(i, &v)) continue;
        char hex[17];
        for (uint8_t b = 0; b < v.dlc && b < 8; ++b) {
            static const char* digits = "0123456789ABCDEF";
            hex[b * 2]     = digits[v.data[b] >> 4];
            hex[b * 2 + 1] = digits[v.data[b] & 0x0F];
        }
        hex[(v.dlc > 8 ? 8 : v.dlc) * 2] = '\0';
        j.add("%s{\"id\":%lu,\"x\":%u,\"n\":%lu,\"t\":%lu,\"l\":%u,\"d\":\"%s\"}",
              first ? "" : ",", (unsigned long)v.id, v.extended ? 1u : 0u,
              (unsigned long)v.count, (unsigned long)v.last_ms, (unsigned)v.dlc, hex);
        first = false;
    }
    j.add("],");
}

void appendGps(JsonWriter& j) {
    const GnssFix  f     = GpsManager::fix();
    const bool     fresh = GpsManager::hasFreshFix();
    const GpsStats g     = GpsManager::stats();

    // lat/lon only with a real fix. 0,0 is a place in the Gulf of Guinea, not a
    // way of saying "unknown".
    j.add("\"gps\":{\"fix\":%s,\"lat\":%.6f,\"lon\":%.6f,\"sats\":%u,"
          "\"hdop\":%.1f,\"sog\":%.1f,\"time_valid\":%s,\"epoch\":%llu,"
          "\"sentences\":%lu,\"badcrc\":%lu,\"silent\":%s,"
          "\"bytes\":%lu,\"lines\":%lu,"
          "\"view\":%u,\"trk\":%u,\"cnr\":%u,\"gsv\":",
          fresh ? "true" : "false",
          fresh ? f.lat : 0.0, fresh ? f.lon : 0.0,
          (unsigned)f.satellites, (double)f.hdop, (double)f.speed_kmh,
          f.time_valid ? "true" : "false",
          (unsigned long long)f.epoch,
          (unsigned long)g.sentences_ok,
          (unsigned long)g.sentences_bad_checksum,
          GpsManager::isSilent() ? "true" : "false",
          (unsigned long)g.bytes_received,
          (unsigned long)g.lines_seen,
          (unsigned)f.sats_in_view, (unsigned)f.sats_tracked,
          (unsigned)f.best_cnr);
    // Escaped: a GSV line is text off a UART and can hold anything.
    j.addString(g.last_gsv);
    j.add("},");
}

void appendThermal(JsonWriter& j) {
    const EnvReading e  = EnvironmentManager::reading();
    const EnvStats   es = EnvironmentManager::stats();
    const FanStats   f  = FanManager::stats();

    // The counters and the STAGE each failure reached, not just the value:
    // read_err alone cannot tell a sensor that never answered from one that
    // answered badly, and those need opposite fixes.
    j.add("\"env\":{\"enabled\":%s,\"valid\":%s,\"t\":%.1f,\"h\":%.1f,"
          "\"ok\":%lu,\"read_err\":%lu,\"crc_err\":%lu,"
          "\"idle\":%s,\"nores\":%lu,\"hshake\":%lu,\"trunc\":%lu,"
          "\"rng\":%lu,\"bits\":%u,\"raw\":\"%02X %02X %02X %02X %02X\"},",
          EnvironmentManager::isEnabled() ? "true" : "false",
          e.valid ? "true" : "false",
          (double)e.temperature_c, (double)e.humidity_pct,
          (unsigned long)es.reads_ok, (unsigned long)es.read_errors,
          (unsigned long)es.checksum_errors,
          es.line_idle_high ? "true" : "false",
          (unsigned long)es.fail_no_response, (unsigned long)es.fail_handshake,
          (unsigned long)es.fail_truncated, (unsigned long)es.fail_range,
          (unsigned)es.last_bits,
          es.last_frame[0], es.last_frame[1], es.last_frame[2],
          es.last_frame[3], es.last_frame[4]);
    j.add("\"fan\":{\"mode\":\"%s\",\"on\":%s,\"t\":%.1f,\"tvalid\":%s,"
          "\"run_s\":%lu,\"trans\":%lu},",
          FanManager::modeName(f.mode), f.running ? "true" : "false",
          (double)f.last_temperature_c, f.temperature_valid ? "true" : "false",
          (unsigned long)f.run_seconds, (unsigned long)f.transitions);
}

void appendSystem(JsonWriter& j, uint32_t requestsServed) {
    const WifiStats w = WifiManager::stats();
    const FrameStream::StreamStats st = FrameStream::stats();
    j.add("\"cap\":{\"sink\":%u,\"frames\":%lu,\"bytes\":%lu,\"path\":\"%s\","
          "\"qdrop\":%lu,"
          "\"net\":{\"on\":%s,\"up\":%s,\"n\":%lu,\"b\":%lu,\"drop\":%lu,\"ses\":%lu}},",
          (unsigned)RawCanLogger::sink(),
          (unsigned long)RawCanLogger::framesWritten(),
          (unsigned long)RawCanLogger::bytesWritten(),
          RawCanLogger::currentPath(),
          (unsigned long)RawCanLogger::queueDrops(),
          st.listening ? "true" : "false", st.connected ? "true" : "false",
          (unsigned long)st.frames, (unsigned long)st.bytes,
          (unsigned long)st.dropped, (unsigned long)st.sessions);
    j.add("\"rssi\":%ld,\"ip\":\"%s\",\"wifi\":\"%s\",\"reqs\":%lu,"
          "\"led\":\"%s\",\"clock\":%lld,\"marks\":%lu,\"notes\":%u,",
          (long)w.rssi, w.ip, WifiManager::stateName(WifiManager::state()),
          (unsigned long)requestsServed,
          StatusLed::statusName(StatusLed::current()),
          // Zero until SNTP answers; the page says "not set" rather than
          // showing 1970, because a capture with boot_epoch=0 has to be aligned
          // by hand.
          (long long)(time(nullptr) > 1600000000 ? time(nullptr) : 0),
          (unsigned long)RawCanLogger::markCount(),
          (unsigned)NotesStore::count());
}

}  // namespace

namespace StateJson {

bool buildState(JsonWriter& j, uint32_t requestsServed) {
    // now_ms lets the page age each identifier against the device's own clock,
    // not the browser's — the two drift, and the phone may sleep.
    j.add("{\"unit\":\"%s\",\"fw\":\"%s\",\"uptime_s\":%lu,\"now_ms\":%lu,",
          UNIT_ID, FW_VERSION, (unsigned long)(millis() / 1000),
          (unsigned long)millis());
    appendCan(j);
    appendIds(j);
    appendGps(j);
    appendThermal(j);
    appendSystem(j, requestsServed);
    SystemHealth::writeJson(j);
    j.add("}");
    return !j.overflow();
}

bool buildFrames(JsonWriter& j) {
    j.add("{");
    FrameRing::writeJson(j);
    j.add("}");
    return !j.overflow();
}

bool buildNotes(JsonWriter& j) {
    j.add("{\"available\":%s,", NotesStore::available() ? "true" : "false");
    NotesStore::writeJson(j);
    j.add("}");
    return !j.overflow();
}

bool buildCaptures(JsonWriter& j) {
    j.add("{\"current\":\"%s\",\"files\":[", RawCanLogger::currentPath());
    bool first = true;
    File dir = LittleFS.open(RAWLOG_DIR);
    if (dir && dir.isDirectory()) {
        for (File f = dir.openNextFile(); f && !j.overflow(); f = dir.openNextFile()) {
            if (!f.isDirectory()) {
                j.add("%s{\"name\":", first ? "" : ",");
                j.addString(f.name());
                j.add(",\"size\":%lu}", (unsigned long)f.size());
                first = false;
            }
            f.close();
        }
    }
    if (dir) dir.close();
    // The notes journal travels with the captures: it is how a run's notes get
    // back to a desk, even though it is never written into a capture itself.
    File journal = LittleFS.open(NOTES_DIR "/journal.log", "r");
    j.add("],\"journal\":%lu}", journal ? (unsigned long)journal.size() : 0UL);
    if (journal) journal.close();
    return !j.overflow();
}

}  // namespace StateJson
