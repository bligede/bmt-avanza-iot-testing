// =============================================================================
//  NotesStore.h — a person's note against a CAN identifier, typed during a run.
//
//  "0x1A6 moves with the brake pedal." The note is how a test session turns a
//  wall of hex into knowledge somebody else can pick up.
//
//  NOTES ARE INTERPRETATION, NOT EVIDENCE. They are deliberately kept out of the
//  capture files: raw evidence is immutable and separate from what an engineer
//  thinks it means (D-015). A MARK records that something happened at a moment;
//  a note records what somebody believes an identifier is. The two live in
//  different files so a wrong guess can be corrected without touching the run.
//
//  Two files under NOTES_DIR:
//    ids.tsv      the current note per identifier, rewritten on every change
//    journal.log  every change ever made, append-only, with time — so a note
//                 that was edited or cleared can still be traced to the run
//
//  Single-owner: only the web task calls into this module, so it needs no lock.
// =============================================================================
#pragma once

#include <Arduino.h>
#include "Config.h"
#include "JsonWriter.h"

namespace NotesStore {

// Loads ids.tsv. Pass whether LittleFS mounted; without it notes are refused
// rather than silently dropped.
void begin(bool filesystemMounted);

// Sets, replaces, or — with empty text — clears the note for one identifier.
// Text is trimmed, stripped of control characters, and cut to NOTE_TEXT_MAX
// bytes on a UTF-8 boundary. Returns false only if the table is full or the
// filesystem refused the write; `stored` receives the text actually kept.
bool set(uint32_t id, bool extended, const char* text,
         char* stored = nullptr, size_t storedLen = 0);

// Appends `"notes":[{"id":..,"x":..,"t":".."},...]`.
void writeJson(JsonWriter& j);

size_t count();
bool   available();     // false when the filesystem is not mounted

}  // namespace NotesStore
