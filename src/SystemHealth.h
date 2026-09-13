// =============================================================================
//  SystemHealth.h — is the ESP32 keeping up, or is it overworked?
//
//  "Overworked" means something concrete for this firmware: frames start going
//  missing, a task runs out of stack, or the heap fragments until an allocation
//  fails. So the numbers reported here are the ones that move BEFORE that
//  happens — how full the queues got, how close each stack came to its end,
//  how late the housekeeping loop woke up — rather than a single CPU figure.
//
//  CPU load is still reported, with a caveat worth stating. This framework
//  (arduino-esp32 2.0.17) ships without FreeRTOS run-time statistics in any
//  sdkconfig variant, so vTaskGetRunTimeStats is unavailable. Load is measured
//  instead with an idle hook per core, counting the ticks in which each idle
//  task got to run at least once. A tick where the core was busy for 99% and
//  idle for 1% counts as idle, so the figure UNDER-states load. Treat it as a
//  trend, and trust the queue and loop-lag numbers for the verdict.
// =============================================================================
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "JsonWriter.h"

namespace SystemHealth {

// Registers the idle hooks. Call once, early in setup().
void begin();

// A task whose remaining stack should be reported. `stackBytes` is the size it
// was created with; ESP-IDF reports the high-water mark in bytes, not words.
void watchTask(TaskHandle_t task, const char* name, uint32_t stackBytes, uint8_t core);

// The capture queue depth, called by its consumer just before each receive,
// which is the moment the queue is at its fullest.
void noteQueueDepth(uint32_t depth, uint32_t capacity);

// Called once per housekeeping loop. The loop sleeps LED_TICK_MS; a period far
// longer than that means core 1 had no time to schedule it.
void noteLoop();

// Turns the raw counters into rates. Call every HEALTH_SAMPLE_MS.
void sample();

// Appends `"health":{...}`.
void writeJson(JsonWriter& j);

}  // namespace SystemHealth
