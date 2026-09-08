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
uint32_t s_seg_bytes     = 0;   // includes what is still staged in RAM
uint32_t s_queue_drops   = 0;
uint32_t s_marks         = 0;

// Staging buffer. See RAWLOG_WRITE_BUF in Config.h for why this exists.
char     s_wr[RAWLOG_WRITE_BUF];
size_t   s_wr_len        = 0;
uint32_t s_blocks        = 0;   // buffer writes since boot, for fsync pacing

void flushBuffer() {
    if (s_wr_len == 0 || !s_file_open) return;
    s_file.write(reinterpret_cast<const uint8_t*>(s_wr), s_wr_len);
    s_wr_len = 0;
    // fsync every eighth block rather than every frame. A sync per frame would
    // erase-cycle the flash hard and stall the storage task; never syncing
    // would lose the tail on a power cut.
    if ((++s_blocks & 0x07) == 0) s_file.flush();
}

void appendRaw(const char* text, size_t len) {
    if (!s_file_open || len == 0) return;
    if (s_wr_len + len > sizeof(s_wr)) flushBuffer();
    if (len > sizeof(s_wr)) {                 // never happens; handled anyway
        s_file.write(reinterpret_cast<const uint8_t*>(text), len);
        s_bytes += len; s_seg_bytes += len;
        return;
    }
    memcpy(s_wr + s_wr_len, text, len);
    s_wr_len   += len;
    s_bytes    += len;
    s_seg_bytes += len;
}

// Every segment opens with its own provenance. A capture file without metadata
// becomes a mystery within weeks: which vehicle, which firmware, which bitrate,
// and what wall-clock time "millis 177678" actually was.
void writeHeader() {
    char h[240];
    const time_t now = time(nullptr);
    const int n = snprintf(h, sizeof(h),
        "# BMT CAN capture v1\n"
        "# unit=%s fw=%s bitrate=%lu listen_only=1\n"
        "# segment=%lu millis_at_open=%lu boot_epoch=%lld\n",
        UNIT_ID, FW_VERSION, (unsigned long)CAN_DEFAULT_BITRATE,
        (unsigned long)s_segment, (unsigned long)millis(),
        (long long)(now > 1600000000 ? now - (time_t)(millis() / 1000) : 0));
    if (n > 0) appendRaw(h, static_cast<size_t>(n));
}

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
    s_seg_bytes = 0;
    writeHeader();
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
        // Segment size is tracked, not read back from the file: position()
        // lags whatever is still staged in RAM.
        if (s_seg_bytes >= RAWLOG_SEGMENT_BYTES) {
            flushBuffer();
            openSegment(++s_segment);
        }
        line[len] = '\n';
        appendRaw(line, len + 1);
        ++s_frames;
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

void noteQueueDrop() { ++s_queue_drops; }
uint32_t queueDrops() { return s_queue_drops; }

void mark(const char* label) {
    if (!s_file_open) return;
    char m[160];
    char safe[80];
    size_t k = 0;
    for (const char* p = label; *p && k < sizeof(safe) - 1; ++p) {
        // The label reaches here from an HTTP query string. Anything that could
        // forge a frame line or a header line is dropped rather than escaped.
        if (*p == '\n' || *p == '\r' || *p == '|' || *p == '#') continue;
        safe[k++] = *p;
    }
    safe[k] = '\0';
    const int n = snprintf(m, sizeof(m), "# MARK %lu %s\n",
                           (unsigned long)millis(), safe);
    if (n > 0) appendRaw(m, static_cast<size_t>(n));
    ++s_marks;
    flushBuffer();          // a marker is worthless if it is lost in a stall
    LOG_I(TAG, "Marker: %s", safe);
}

uint32_t markCount() { return s_marks; }

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
        flushBuffer();
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
