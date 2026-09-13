// =============================================================================
//  FrameRing.h — the last WEB_FRAME_RING frames, for the raw trace endpoint.
//
//  This is the "separate debug view" for raw hex. The dashboard itself shows
//  the latest payload per identifier (CanManager's survey), which is what a
//  person watching for changing bytes needs. The ring answers a different
//  question — what arrived, in what order — and is served at /api/frames.
//
//  Written by the storage task, read by the web task, guarded by a mutex that
//  the writer will not wait on: a busy reader costs a trace entry, never a
//  capture write.
// =============================================================================
#pragma once

#include <Arduino.h>
#include "CanManager.h"
#include "JsonWriter.h"

namespace FrameRing {

void begin();
void push(const CanFrame& frame);

// Appends `"frames":[...]`, newest first.
void writeJson(JsonWriter& j);

}  // namespace FrameRing
