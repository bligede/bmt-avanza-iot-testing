#include "FrameStream.h"

#include <WiFi.h>
#include <lwip/sockets.h>
#include <time.h>

#include "Logger.h"
#include "RawCanLogger.h"

namespace {

const char* TAG = "STREAM";

WiFiServer s_server(CAPTURE_STREAM_PORT);
WiFiClient s_client;
bool     s_listening = false;
uint32_t s_frames    = 0;
uint32_t s_bytes     = 0;
uint32_t s_dropped   = 0;
uint32_t s_sessions  = 0;

// poll() and write() both touch the buffer and the socket. They are called
// from the same task by design (see FrameStream.h), but a lock costs nothing
// here and turns a future mistake into a wait instead of a crash: the first
// version ran poll() from housekeeping while write() ran from storage, and the
// device rebooted the moment frames started flowing.
SemaphoreHandle_t s_lock = nullptr;

struct Guard {
    bool held;
    explicit Guard(uint32_t wait_ms) {
        held = s_lock && xSemaphoreTakeRecursive(s_lock, pdMS_TO_TICKS(wait_ms)) == pdTRUE;
    }
    ~Guard() { if (held) xSemaphoreGiveRecursive(s_lock); }
};

// Staging buffer. One TCP write per frame would spend more time in lwIP than
// in the CAN reader; a few kilobytes at a time costs nothing and keeps the
// socket from being poked 600 times a second.
char   s_buf[CAPTURE_STREAM_BUF];
size_t s_len       = 0;
uint32_t s_last_tx = 0;

void flush(bool force) {
    if (s_len == 0 || !s_client || !s_client.connected()) return;
    const uint32_t now = millis();
    if (!force && s_len < CAPTURE_STREAM_CHUNK && now - s_last_tx < CAPTURE_STREAM_FLUSH_MS) return;

    // Straight to the socket with MSG_DONTWAIT, deliberately not through
    // WiFiClient::write(): that one blocks until lwIP has room, and a blocked
    // storage task means a filling CAN queue. WiFiClient::availableForWrite()
    // is not an option either — WiFiClient never implements it, so it returns
    // the Print base class's 0 and nothing would ever be sent.
    const int fd = s_client.fd();
    if (fd < 0) return;
    const int sent = ::send(fd, s_buf, s_len, MSG_DONTWAIT);
    if (sent > 0) {
        s_bytes += static_cast<uint32_t>(sent);
        s_len -= static_cast<size_t>(sent);
        if (s_len) memmove(s_buf, s_buf + sent, s_len);
        s_last_tx = now;
    } else if (sent < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
        LOG_W(TAG, "Receiver socket error %d; dropping it", errno);
        s_client.stop();
        s_len = 0;
    }
}

// The same provenance header a capture file opens with, so a streamed file
// and a pulled file are the same kind of evidence.
void sendHeader() {
    const time_t now = time(nullptr);
    char h[192];
    const int n = snprintf(h, sizeof(h),
        "# BMT CAN capture v1 (stream)\n"
        "# unit=%s fw=%s bitrate=%lu listen_only=1\n"
        "# stream_open millis=%lu boot_epoch=%lld\n",
        UNIT_ID, FW_VERSION, (unsigned long)CanManager::bitrate(),
        (unsigned long)millis(),
        (long long)(now > 1600000000 ? now - (time_t)(millis() / 1000) : 0));
    if (n > 0 && static_cast<size_t>(n) < sizeof(s_buf)) {
        memcpy(s_buf, h, static_cast<size_t>(n));
        s_len = static_cast<size_t>(n);
        flush(true);
    }
}

}  // namespace

namespace FrameStream {

void begin() {
    s_lock = xSemaphoreCreateRecursiveMutex();
    s_server.begin();
    s_server.setNoDelay(true);
    s_listening = true;
    LOG_I(TAG, "Frame stream listening on port %d", CAPTURE_STREAM_PORT);
}

void poll() {
    if (!s_listening) return;
    Guard g(20);
    if (!g.held) return;

    if (s_server.hasClient()) {
        WiFiClient fresh = s_server.available();
        if (s_client && s_client.connected()) {
            LOG_W(TAG, "A second receiver connected; dropping the first");
            s_client.stop();
        }
        s_client = fresh;
        s_client.setNoDelay(true);
        s_len = 0;
        s_frames = 0;
        s_bytes = 0;
        s_dropped = 0;
        ++s_sessions;
        LOG_I(TAG, "Receiver %s connected", s_client.remoteIP().toString().c_str());
        sendHeader();
    }

    if (s_client && !s_client.connected()) {
        LOG_W(TAG, "Receiver gone after %lu frames, %lu dropped",
              (unsigned long)s_frames, (unsigned long)s_dropped);
        s_client.stop();
        s_len = 0;
    }
    flush(false);
}

void write(const CanFrame& frame) {
    if (!s_client || !s_client.connected()) return;
    // Never wait here: this runs on the path that drains the CAN queue.
    Guard g(0);
    if (!g.held) { ++s_dropped; return; }

    char line[96];
    const size_t len = RawCanLogger::formatLine(frame, line, sizeof(line));
    if (len == 0) return;

    if (s_len + len + 1 > sizeof(s_buf)) {
        flush(true);                      // one attempt to make room
        if (s_len + len + 1 > sizeof(s_buf)) {
            ++s_dropped;                  // receiver is behind: lose the frame, not the bus
            return;
        }
    }
    memcpy(s_buf + s_len, line, len);
    s_len += len;
    s_buf[s_len++] = '\n';
    ++s_frames;
    flush(false);
}

StreamStats stats() {
    Guard g(10);
    StreamStats s;
    s.listening = s_listening;
    s.connected = s_client && s_client.connected();
    s.frames    = s_frames;
    s.bytes     = s_bytes;
    s.dropped   = s_dropped;
    s.sessions  = s_sessions;
    s.client_ip = s.connected ? static_cast<uint32_t>(s_client.remoteIP()) : 0;
    return s;
}

}  // namespace FrameStream
