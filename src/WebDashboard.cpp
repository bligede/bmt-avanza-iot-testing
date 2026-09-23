// =============================================================================
//  WebDashboard.cpp — the HTTP layer, and nothing else.
//
//  Routes requests to the modules that own the answers:
//
//      page            web/  → tools/embed_web.py → generated/WebAssets.h
//      JSON documents  StateJson
//      notes           NotesStore
//      raw trace       FrameRing
//
//  Before this split the page, its styles, its script, every JSON builder and
//  the routes lived in one 1,084-line file, and changing a table column meant
//  editing HTML inside a C++ raw string. The page is now ordinary web source,
//  gzipped into flash at build time.
//
//  Nothing reachable from here writes to the CAN bus. The only requests that
//  change state are /api/mark (a line in the capture file) and /api/note (a
//  note file kept apart from the captures); neither has a path to the driver.
// =============================================================================
#include "WebDashboard.h"

#include "Logger.h"

#if ENABLE_WEB_DASHBOARD

#include <WebServer.h>
#include <LittleFS.h>

#include "FrameRing.h"
#include "JsonWriter.h"
#include "NotesStore.h"
#include "RawCanLogger.h"
#include "StateJson.h"
#include "generated/WebAssets.h"

namespace {

const char* TAG = "WEB";

WebServer s_server(WEB_PORT);
uint32_t  s_requests = 0;
bool      s_started  = false;
char      s_json[WEB_JSON_BUF];

void sendJson(bool fitted, const JsonWriter& j) {
    if (!fitted) {
        // Say so rather than serving malformed JSON the page would fail to parse
        // — and fail to parse silently.
        LOG_W(TAG, "JSON response truncated — raise WEB_JSON_BUF");
        s_server.send(503, "application/json", "{\"error\":\"response buffer too small\"}");
        return;
    }
    s_server.sendHeader("Cache-Control", "no-store");
    s_server.send(200, "application/json", j.c_str());
}

// ---- page -------------------------------------------------------------------
void handleIndex() {
    ++s_requests;
    s_server.sendHeader("Content-Encoding", "gzip");
    s_server.sendHeader("Cache-Control", "no-store");
    s_server.send_P(200, "text/html; charset=utf-8",
                    reinterpret_cast<const char*>(INDEX_HTML_GZ), INDEX_HTML_GZ_LEN);
}

// ---- documents --------------------------------------------------------------
// GET /api/state[?ids=all|named|tds]
//
// Without the parameter the document carries every identifier, so every tool
// written against this endpoint keeps working. The page asks for the mode the
// operator picked.
void handleState() {
    ++s_requests;
    StateJson::IdFilter filter = StateJson::IdFilter::All;
    if (s_server.hasArg("ids")) {
        const String mode = s_server.arg("ids");
        if (mode == "tds")        filter = StateJson::IdFilter::Tds;
        else if (mode == "named") filter = StateJson::IdFilter::Named;
    }
    JsonWriter j(s_json, sizeof(s_json));
    sendJson(StateJson::buildState(j, s_requests, filter), j);
}

void handleFrames() {
    ++s_requests;
    JsonWriter j(s_json, sizeof(s_json));
    sendJson(StateJson::buildFrames(j), j);
}

void handleNotes() {
    ++s_requests;
    JsonWriter j(s_json, sizeof(s_json));
    sendJson(StateJson::buildNotes(j), j);
}

void handleCaptures() {
    ++s_requests;
    JsonWriter j(s_json, sizeof(s_json));
    sendJson(StateJson::buildCaptures(j), j);
}

// ---- actions ----------------------------------------------------------------
// Recording control from the phone. Until now both needed a laptop on the
// serial console, which in a vehicle means unplugging the device from the car.
//
// Pausing is not destructive, so it just does it. Clearing IS destructive, so
// it demands the unit id as confirmation: a stray request cannot wipe a run,
// and whoever sends it has at least read the dashboard header.
void handleSink() {
    ++s_requests;
    const String mode = s_server.arg("mode");
    if (mode == "off") {
        RawCanLogger::setSink(RAWLOG_SINK_NONE);
    } else if (mode == "file") {
        RawCanLogger::setSink(RAWLOG_SINK_FILE);
    } else {
        s_server.send(400, "text/plain", "mode must be off or file");
        return;
    }
    s_server.sendHeader("Cache-Control", "no-store");
    s_server.send(200, "text/plain", RawCanLogger::isCapturing() ? "recording" : "paused");
}

void handleClear() {
    ++s_requests;
    if (s_server.arg("confirm") != UNIT_ID) {
        s_server.send(403, "text/plain", "confirm= must be the unit id shown in the header");
        return;
    }
    const uint8_t sink = RawCanLogger::sink();
    if (!RawCanLogger::clearCaptures()) {
        s_server.send(500, "text/plain", "Could not clear /capture");
        return;
    }
    if (sink != RAWLOG_SINK_NONE) RawCanLogger::startFileCapture();
    LOG_W(TAG, "Captures cleared over HTTP");
    s_server.sendHeader("Cache-Control", "no-store");
    s_server.send(200, "text/plain", "cleared");
}

// An operator marker, written into the capture file. The label is stripped of
// anything that could forge a frame or header line inside RawCanLogger::mark().
void handleMark() {
    ++s_requests;
    if (!RawCanLogger::isCapturing()) {
        s_server.send(409, "text/plain", "Capture is not running");
        return;
    }
    String label = s_server.arg("label");
    if (label.length() == 0) label = "MARK";
    if (label.length() > 48) label = label.substring(0, 48);
    RawCanLogger::mark(label.c_str());
    s_server.sendHeader("Cache-Control", "no-store");
    s_server.send(200, "text/plain", String(RawCanLogger::markCount()));
}

// A note against one identifier. Kept in NotesStore, never in the capture:
// a note is what somebody thinks an identifier is, and that stays separate from
// the evidence (D-015). Empty text clears the note.
void handleNote() {
    ++s_requests;
    if (!NotesStore::available()) {
        s_server.send(503, "text/plain", "Filesystem not mounted - notes cannot be kept");
        return;
    }
    const String idArg = s_server.arg("id");
    char* end = nullptr;
    const unsigned long id = strtoul(idArg.c_str(), &end, 10);
    if (idArg.length() == 0 || end == nullptr || *end != '\0' || id > 0x1FFFFFFFUL) {
        s_server.send(400, "text/plain", "id must be a decimal CAN identifier");
        return;
    }
    const bool extended = s_server.arg("x") == "1" || id > 0x7FFUL;
    char stored[NOTE_TEXT_MAX + 1];
    if (!NotesStore::set(static_cast<uint32_t>(id), extended,
                         s_server.arg("text").c_str(), stored, sizeof(stored))) {
        s_server.send(507, "text/plain", "Note not saved - table full or write failed");
        return;
    }
    // Echo what was actually kept, after trimming and length limits, so the page
    // shows the truth rather than what was typed.
    JsonWriter j(s_json, sizeof(s_json));
    j.add("{\"id\":%lu,\"x\":%u,\"t\":", id, extended ? 1u : 0u);
    j.addString(stored);
    j.add("}");
    sendJson(!j.overflow(), j);
}

// ---- downloads --------------------------------------------------------------
// Capture files, for conversion with tools/capture_to_webcan.py and analysis
// with the reverse-engineering skill. The file currently being written lacks
// whatever is still staged in RAM (up to RAWLOG_WRITE_BUF) — take a finished
// segment for anything that has to be complete.
void handleDownload() {
    ++s_requests;
    const String name = s_server.arg("name");
    bool safe = name.length() > 0 && name.length() <= 40;
    for (size_t i = 0; safe && i < name.length(); ++i) {
        const char ch = name[i];
        safe = isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_' || ch == '.';
    }
    if (!safe || name.indexOf("..") >= 0) {
        s_server.send(400, "text/plain", "Bad file name");
        return;
    }
    const String path = (name == "journal.log") ? String(NOTES_DIR "/journal.log")
                                                : String(RAWLOG_DIR "/") + name;
    File f = LittleFS.open(path, "r");
    if (!f || f.isDirectory()) {
        if (f) f.close();
        s_server.send(404, "text/plain", "No such file");
        return;
    }
    s_server.sendHeader("Content-Disposition", String("attachment; filename=\"") +
                        UNIT_ID + "-" + name + "\"");
    s_server.streamFile(f, "text/plain");
    f.close();
}

void handleNotFound() {
    ++s_requests;
    s_server.send(404, "text/plain",
                  "Not found. Routes: /  /api/state  /api/frames  /api/notes  "
                  "/api/captures  /api/capture?name=  POST /api/mark  POST /api/note");
}

}  // namespace

namespace WebDashboard {

void begin() {
    FrameRing::begin();

    s_server.on("/",             HTTP_GET,  handleIndex);
    s_server.on("/api/state",    HTTP_GET,  handleState);
    s_server.on("/api/frames",   HTTP_GET,  handleFrames);
    s_server.on("/api/notes",    HTTP_GET,  handleNotes);
    s_server.on("/api/captures", HTTP_GET,  handleCaptures);
    s_server.on("/api/capture",  HTTP_GET,  handleDownload);
    s_server.on("/api/mark",     HTTP_POST, handleMark);
    s_server.on("/api/sink",     HTTP_POST, handleSink);
    s_server.on("/api/clear",    HTTP_POST, handleClear);
    s_server.on("/api/note",     HTTP_POST, handleNote);
    s_server.onNotFound(handleNotFound);
    s_server.begin();
    s_started = true;

    LOG_I(TAG, "Dashboard on port %d (page %u B gzipped)", WEB_PORT,
          (unsigned)INDEX_HTML_GZ_LEN);
}

void poll() {
    if (s_started) s_server.handleClient();
}

void noteFrame(const CanFrame& frame) {
    FrameRing::push(frame);
}

bool     isEnabled()      { return true; }
uint32_t requestsServed() { return s_requests; }

}  // namespace WebDashboard

#else   // ENABLE_WEB_DASHBOARD == 0

namespace WebDashboard {
void begin() {}
void poll()  {}
void noteFrame(const CanFrame&) {}
bool     isEnabled()      { return false; }
uint32_t requestsServed() { return 0; }
}  // namespace WebDashboard

#endif
