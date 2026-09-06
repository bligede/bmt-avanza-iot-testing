// =============================================================================
//  WatchdogManager.h — 10 s task watchdog (Blueprint §6.1, master prompt §28).
//
//  Every long-lived task subscribes and must check in within the timeout.
//  A task that stops responding resets the device.
//
//  The watchdog is a last resort, not a design tool. Every task in this firmware
//  is written so that it returns to its loop quickly on its own: the modem is a
//  non-blocking state machine, LittleFS writes are batched, CAN receive uses a
//  bounded timeout. If the watchdog ever fires, the correct response is to find
//  the blocking call — not to widen the timeout (master prompt §28).
// =============================================================================
#pragma once

#include <Arduino.h>
#include "Config.h"

namespace WatchdogManager {

// Arms the task watchdog. Call after the tasks exist.
void begin();

// Each task calls this once from its own context.
bool subscribeCurrentTask(const char* name);

// The check-in. Call once per loop iteration.
void feed();

// Removes the calling task from supervision, before it deletes itself.
void unsubscribeCurrentTask();

bool isArmed();

// Reason the previous boot ended: "power on", "watchdog", "panic", ...
const char* lastResetReason();

// True when the previous boot ended in a watchdog or panic reset. Worth
// reporting, because it usually means a real bug rather than a power glitch.
bool lastResetWasFault();

}  // namespace WatchdogManager
