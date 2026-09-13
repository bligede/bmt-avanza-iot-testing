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

bool buildState(JsonWriter& j, uint32_t requestsServed);
bool buildFrames(JsonWriter& j);
bool buildNotes(JsonWriter& j);
bool buildCaptures(JsonWriter& j);

}  // namespace StateJson
