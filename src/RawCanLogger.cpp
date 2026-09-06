#include "RawCanLogger.h"

#include <LittleFS.h>
#include "Logger.h"
#include "CanBusSafety.h"

namespace {

const char* TAG = "RAWLOG";

uint8_t  s_sink          = RAWLOG_SINK_NONE;
File     s_file;
bool     s_file_open     = false;
uint32_t s_bytes         = 0;
uint32_t s_frames        = 0;
uint32_t s_segment       = 0;
char     s_path[48]      = {0};
bool     s_ceiling_warned = false;

bool openSegment(uint32_t index) {
    if (s_file_open) { s_file.close(); s_file_open = false; }
    snprintf(s_path, sizeof(s_path), "%s/can-%03lu.log",
             RAWLOG_DIR, (unsigned long)index);
    s_file = LittleFS.open(s_path, "a");
    if (!s_file) {
        LOG_E(TAG, "Cannot open %s", s_path);
        return false;
    }
    s_file_open = true;
    LOG_I(TAG, "Capturing to %s", s_path);
    return true;
}

}  // namespace

namespace RawCanLogger {

void begin(uint8_t sink) {
    s_sink  = sink;
    s_bytes = 0;
    s_frames = 0;
    s_segment = 0;
    s_ceiling_warned = false;

    if (sink == RAWLOG_SINK_FILE || sink == RAWLOG_SINK_BOTH) {
        startFileCapture();
    }
    if (sink == RAWLOG_SINK_NONE) {
        LOG_I(TAG, "Raw capture off (production default)");
    }
}

size_t format(const CanFrame& frame, char* out, size_t outLen) {
    char bytes[32] = {0};
    Logger::formatBytes(bytes, sizeof(bytes), frame.data, frame.dlc);

    // Extended IDs print as 8 hex digits so a parser can tell them apart from
    // an 11-bit ID by width alone; the explicit marker removes any doubt.
    int n;
    if (frame.extended) {
        n = snprintf(out, outLen, "%lu | ID: 0x%08lX | DLC: %u | %s | EXT%s",
                     (unsigned long)frame.rx_millis, (unsigned long)frame.id,
                     (unsigned)frame.dlc, bytes, frame.remote ? " RTR" : "");
    } else {
        n = snprintf(out, outLen, "%lu | ID: 0x%03lX | DLC: %u | %s%s",
                     (unsigned long)frame.rx_millis, (unsigned long)frame.id,
                     (unsigned)frame.dlc, bytes, frame.remote ? " | RTR" : "");
    }
    if (n < 0 || static_cast<size_t>(n) >= outLen) return 0;
    return static_cast<size_t>(n);
}

void write(const CanFrame& frame) {
    if (s_sink == RAWLOG_SINK_NONE) return;

    char line[96];
    const size_t len = format(frame, line, sizeof(line));
    if (len == 0) return;

    if (s_sink == RAWLOG_SINK_SERIAL || s_sink == RAWLOG_SINK_BOTH) {
        LOG_D(TAG, "%s", line);
    }

    if ((s_sink == RAWLOG_SINK_FILE || s_sink == RAWLOG_SINK_BOTH) && s_file_open) {
        // Stop before the capture starves the telemetry buffer. Blueprint §6.2
        // puts the 24 h buffer above raw capture in the priority order.
        if (s_bytes >= RAWLOG_MAX_BYTES) {
            if (!s_ceiling_warned) {
                LOG_W(TAG, "Capture ceiling %lu B reached — stopping file sink "
                           "to protect the telemetry buffer",
                      (unsigned long)RAWLOG_MAX_BYTES);
                s_ceiling_warned = true;
                stopFileCapture();
            }
            return;
        }
        if (s_file.position() >= RAWLOG_SEGMENT_BYTES) {
            openSegment(++s_segment);
        }
        s_file.print(line);
        s_file.print('\n');
        s_bytes += len + 1;
        ++s_frames;

        // Flush periodically rather than per frame: a flush per frame on a busy
        // bus would erase-cycle the flash hard and stall the storage task.
        if ((s_frames & 0x3F) == 0) s_file.flush();
    }
}

void setSink(uint8_t sink) {
    if (sink == s_sink) return;
    const bool wantFile = (sink == RAWLOG_SINK_FILE || sink == RAWLOG_SINK_BOTH);
    const bool haveFile = (s_sink == RAWLOG_SINK_FILE || s_sink == RAWLOG_SINK_BOTH);
    s_sink = sink;
    if (wantFile && !haveFile)      startFileCapture();
    else if (!wantFile && haveFile) stopFileCapture();
    LOG_I(TAG, "Sink -> %u", (unsigned)sink);
}

uint8_t sink() { return s_sink; }
bool isCapturing() { return s_sink != RAWLOG_SINK_NONE; }

bool startFileCapture() {
    if (!LittleFS.exists(RAWLOG_DIR)) {
        LittleFS.mkdir(RAWLOG_DIR);
    }
    s_ceiling_warned = false;
    return openSegment(s_segment);
}

void stopFileCapture() {
    if (s_file_open) {
        s_file.flush();
        s_file.close();
        s_file_open = false;
        LOG_I(TAG, "Capture closed: %lu frames, %lu bytes",
              (unsigned long)s_frames, (unsigned long)s_bytes);
    }
}

uint32_t bytesWritten()  { return s_bytes; }
uint32_t framesWritten() { return s_frames; }
const char* currentPath() { return s_path; }

bool clearCaptures() {
    stopFileCapture();
    File dir = LittleFS.open(RAWLOG_DIR);
    if (!dir || !dir.isDirectory()) return false;

    uint32_t removed = 0;
    File entry = dir.openNextFile();
    while (entry) {
        char path[64];
        snprintf(path, sizeof(path), "%s/%s", RAWLOG_DIR, entry.name());
        entry.close();
        if (LittleFS.remove(path)) ++removed;
        entry = dir.openNextFile();
    }
    dir.close();

    s_bytes = 0; s_frames = 0; s_segment = 0;
    LOG_I(TAG, "Cleared %lu capture files", (unsigned long)removed);
    return true;
}

}  // namespace RawCanLogger
