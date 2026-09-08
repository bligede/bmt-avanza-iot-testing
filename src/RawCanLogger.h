// =============================================================================
//  RawCanLogger.h — Blueprint Fase 0, "Bukti data".
//
//  Acceptance for Fase 0 is a raw capture: frames arriving at 500 or 250 kbps,
//  recorded for at least ten minutes. This module produces exactly that, in the
//  format the master prompt §9 specifies:
//
//      123456 | ID: 0x123 | DLC: 8 | 00 30 39 00 00 00 00 00
//
//  The same format feeds tools/can_find_value.py, which implements the
//  known-value search from Blueprint §9.1, and MockCanSource, which replays a
//  capture on the bench. So a capture taken on a vehicle is directly reusable
//  as a test fixture — which is why no CAN ID ever has to be invented.
//
//  File writing happens on core 1. Core 0 only enqueues, so a slow flash erase
//  can never stall CAN acquisition (master prompt §36).
// =============================================================================
#pragma once

#include <Arduino.h>
#include "CanManager.h"

namespace RawCanLogger {

void begin(uint8_t sink = RAWLOG_DEFAULT_SINK);

// Called from the storage task on core 1, with frames handed over by the
// reader task through a queue.
void write(const CanFrame& frame);

// Renders one frame in the capture format. Exposed so the serial console and
// the file sink share a single implementation.
size_t format(const CanFrame& frame, char* out, size_t outLen);

// Sink control, used by the serial console during a capture session.
void    setSink(uint8_t sink);
uint8_t sink();
bool    isCapturing();

// Starts a new capture file. Returns false if the filesystem is unavailable.
bool startFileCapture();
void stopFileCapture();

uint32_t bytesWritten();
uint32_t framesWritten();

// Frames the CAN task could not hand over because the writer queue was full.
// Missing from the FILE only — the bus counters still saw every one of them.
// Counted here rather than in main.cpp so the dashboard can show it; a drop
// counter nobody can read is not an instrument.
void     noteQueueDrop();
uint32_t queueDrops();

// Writes a marked record into the capture at this instant. The event marker in
// the dashboard uses it so a physical action in the car lands in the same file
// as the frames, at the same clock, instead of on a paper run sheet that has to
// be aligned afterwards by guessing an offset.
void mark(const char* label);
uint32_t markCount();
const char* currentPath();

// Deletes every capture file. Frees space for the telemetry buffer.
bool clearCaptures();

}  // namespace RawCanLogger
