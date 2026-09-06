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
const char* currentPath();

// Deletes every capture file. Frees space for the telemetry buffer.
bool clearCaptures();

}  // namespace RawCanLogger
