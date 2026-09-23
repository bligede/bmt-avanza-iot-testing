// =============================================================================
//  StateJson.h — the documents the dashboard reads.
//
//  Kept apart from WebDashboard.cpp so the HTTP layer only routes, and so the
//  shape of the data can be read in one place without scrolling past a web
//  page. Every builder writes into a caller-owned JsonWriter and returns false
//  when the document did not fit; the caller must not serve it in that case.
//
//      /api/state     everything the page shows, polled every 500 ms
//      /api/frames    the raw trace, newest first — a debug view, not the page
//      /api/notes     identifier notes, fetched on load and after each save
//      /api/captures  capture files on flash, for download and conversion
// =============================================================================
#pragma once

#include <Arduino.h>
#include "JsonWriter.h"

namespace StateJson {

// Which identifiers the state document carries.
//
// This filters what is SENT, never what is captured. The recorder still writes
// every frame on the bus, because an identifier nobody has named yet is exactly
// what the next mapping session needs. What it saves is real but second order:
// on a bus with 34 identifiers and 8 named ones, the document drops by roughly
// three quarters, and with it the hex conversion, the socket write, and the
// work the phone does parsing it.
//
// It is NOT a fix for lost frames. Frames are lost to flash writes stalling the
// CAN interrupt (docs/evidence/00-umum/frame-loss.md), and no amount of
// filtering the dashboard touches that.
enum class IdFilter : uint8_t {
    All,     // every identifier seen, which is what a mapping session needs
    Named,   // only identifiers that carry a note
    Tds      // only notes tagged NOTE_TDS_TAG
};

bool buildState(JsonWriter& j, uint32_t requestsServed,
                IdFilter filter = IdFilter::All);
bool buildFrames(JsonWriter& j);
bool buildNotes(JsonWriter& j);
bool buildCaptures(JsonWriter& j);

}  // namespace StateJson
