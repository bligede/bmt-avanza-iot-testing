// =============================================================================
//  FrameStream.h — send every captured frame to a laptop over WiFi, live.
//
//  Why this exists. Writing a frame to the internal flash costs frames: the
//  TWAI interrupt lives in flash (CONFIG_TWAI_ISR_IN_IRAM is not set in this
//  framework), so every flash write disables the instruction cache and the
//  interrupt cannot run. The controller FIFO then overruns. Measured on a
//  DFSK Gelora E at 644 frames/s: 4.9 % of the bus lost while recording,
//  and nothing at all lost with flash idle. See docs/evidence/frame-loss.md.
//
//  WiFi does not disable the cache. Streaming the same lines to a laptop
//  therefore records the bus WITHOUT the losses, and without the 12 MB
//  ceiling. The cost is that a receiver has to be present: a dropped link
//  is a hole in the recording, counted and reported, never silently filled.
//
//  Format: byte for byte what RawCanLogger writes to a file, header lines
//  included, so every existing tool reads the result unchanged.
//
//  One client at a time. A second connection replaces the first — an ESP32
//  has no business fanning out, and the last one to ask is the one in front
//  of the vehicle.
// =============================================================================
#pragma once

#include <Arduino.h>
#include "Config.h"
#include "CanManager.h"

namespace FrameStream {

struct StreamStats {
    bool     listening;
    bool     connected;
    uint32_t frames;        // frames handed to the socket since this client connected
    uint32_t bytes;
    uint32_t dropped;       // frames discarded because the socket could not keep up
    uint32_t sessions;      // clients served since boot
    uint32_t client_ip;
};

// Starts listening on CAPTURE_STREAM_PORT. Safe to call before WiFi is up.
void begin();

// Accepts a waiting client and flushes the staging buffer. Cheap; call often.
//
// Call it from the SAME task that calls write(), which is the storage task.
// The first version polled from housekeeping while frames arrived on storage,
// and the device rebooted as soon as the bus was busy: two tasks were moving
// the same buffer under each other. A recursive mutex now guards it too, but
// one owner is the design, and the lock is only the seat belt.
void poll();

// Queues one frame. Never blocks: when the socket is behind, the frame is
// dropped and counted, because stalling here would cost bus frames instead.
void write(const CanFrame& frame);

StreamStats stats();

}  // namespace FrameStream
