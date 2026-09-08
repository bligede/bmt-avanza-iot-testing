// =============================================================================
//  CanManager.h — TWAI (CAN 2.0B) receive path, permanently listen-only.
//
//  Blueprint §4:  "Perangkat beroperasi dalam mode listen-only secara permanen.
//                  Perangkat hanya mendengarkan lalu lintas bus CAN dan tidak
//                  pernah mengirim frame apa pun ke kendaraan. Pembatasan ini
//                  wajib dikunci di tingkat firmware, bukan sekadar konvensi
//                  pemakaian."
//
//  There is no API here that transmits, and none that changes the mode. The
//  mode is a compile-time constant, latched at init and re-asserted on every
//  recovery. Anything that would put a dominant bit on the bus — a data frame,
//  an ACK, an error frame — is impossible in this mode at the controller level.
//
//  The bitrate is chosen at build/installation time (Blueprint §6.2). It is
//  never auto-negotiated on a live vehicle: probing a bus by switching bitrates
//  is exactly how a monitoring device starts corrupting frames for real ECUs.
// =============================================================================
#pragma once

#include <Arduino.h>
#include "Config.h"

// One received frame, copied out of the driver so the reader task can hand it
// to two consumers (decoder, raw logger) without either holding a driver lock.
struct CanFrame {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
    bool     extended;
    bool     remote;        // RTR — logged, never answered
    uint32_t rx_millis;     // arrival time, used as the reading timestamp
};

enum class CanState : uint8_t {
    Uninitialised = 0,
    Running,
    BusError,        // error-passive or recovering
    Stopped,
    InstallFailed,
};

struct CanStats {
    uint32_t frames_received;
    uint32_t frames_dropped_queue;   // decoder queue was full
    uint32_t rx_missed;              // driver-level overrun
    uint32_t bus_errors;
    uint32_t recoveries;
    uint32_t last_frame_ms;
    uint32_t unique_ids_seen;
};

namespace CanManager {

// Installs the driver in listen-only mode and starts it. Returns false only on
// a driver-level failure; the caller must keep running either way, because a
// dead CAN bus is a diagnosis, not a reason to stop the rest of the device.
bool begin(uint32_t bitrate = CAN_DEFAULT_BITRATE);

// Blocking receive with a timeout, called from the CAN reader task on core 0.
// The timeout is what keeps the task responsive to the watchdog.
bool receive(CanFrame* out, uint32_t timeout_ms);

// Processes driver alerts and drives bus recovery. Called from the reader task.
// Recovery reinstalls in listen-only; there is no path that reinstalls in any
// other mode.
void poll();

CanState  state();
CanStats  stats();
uint32_t  bitrate();

// True once the driver has been installed in listen-only mode. Latched at
// init; nothing in the firmware can clear it.
bool isListenOnlyLocked();

// Milliseconds since the last frame. UINT32_MAX when nothing has arrived.
uint32_t silenceMs();

// True when a frame arrived within CAN_SILENCE_TIMEOUT_MS.
bool isAlive();

// Records that the reader had to drop a frame because the decoder queue was
// full. The reader task owns that condition, but the counter belongs with the
// other CAN statistics so the console and the diagnostic dashboard report one
// consistent set of numbers rather than each keeping its own.
void     noteDecodeQueueDrop();

// Tracks which IDs have been seen, for the Fase 0 survey. Capacity-limited.
void     noteId(uint32_t id);
uint16_t seenIdCount();

// True once the survey has run out of slots. Distinguishes "these are all the
// identifiers on the bus" from "these are the first 128 we happened to meet",
// which are very different statements to build a work plan on.
bool seenIdOverflow();
bool     seenIdAt(uint16_t index, uint32_t* id, uint32_t* count);

// Stops the driver. Used by factory reset and by OTA before a restart.
void end();

}  // namespace CanManager
