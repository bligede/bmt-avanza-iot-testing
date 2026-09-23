#include "SystemHealth.h"

#include <esp_freertos_hooks.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <LittleFS.h>
#include "Config.h"
#include "CanManager.h"
#include "Logger.h"

namespace {

const char* TAG = "HEALTH";
constexpr uint8_t MAX_TASKS = 8;

struct WatchedTask {
    TaskHandle_t handle;
    const char*  name;
    uint32_t     stack_bytes;
    uint8_t      core;
};

WatchedTask s_tasks[MAX_TASKS];
uint8_t     s_task_count = 0;

// ---- CPU, via idle hooks -----------------------------------------------------
// Written by each core's idle task, read by housekeeping. Single writer per
// element and word-sized, so no lock is needed.
volatile uint32_t   s_idle_ticks[2]     = {0, 0};
volatile TickType_t s_idle_last_tick[2] = {0, 0};

bool idleHookCore0() {
    const TickType_t now = xTaskGetTickCount();
    if (now != s_idle_last_tick[0]) { s_idle_last_tick[0] = now; ++s_idle_ticks[0]; }
    return true;        // let the core wait for the next interrupt
}
bool idleHookCore1() {
    const TickType_t now = xTaskGetTickCount();
    if (now != s_idle_last_tick[1]) { s_idle_last_tick[1] = now; ++s_idle_ticks[1]; }
    return true;
}

uint32_t   s_prev_idle[2]   = {0, 0};
TickType_t s_prev_sample    = 0;
int16_t    s_cpu_permille[2] = {-1, -1};   // -1 until the first full window

// ---- queue and loop ------------------------------------------------------------
uint32_t s_q_depth = 0, s_q_peak = 0, s_q_cap = 0;

uint32_t s_loop_last_ms   = 0;
uint32_t s_loop_window_max = 0;   // longest period in the current window
uint32_t s_loop_shown_max  = 0;   // the previous window's, what the page sees
uint32_t s_loop_ever_max   = 0;

// ---- slow-moving figures, refreshed rarely -------------------------------------
uint32_t s_fs_total = 0, s_fs_used = 0;
uint32_t s_app_used = 0, s_app_size = 0;
uint32_t s_slow_last_ms = 0;
bool     s_app_known = false;

}  // namespace

namespace SystemHealth {

void begin() {
    const bool ok0 = esp_register_freertos_idle_hook_for_cpu(idleHookCore0, 0) == ESP_OK;
    const bool ok1 = esp_register_freertos_idle_hook_for_cpu(idleHookCore1, 1) == ESP_OK;
    s_prev_sample = xTaskGetTickCount();
    if (!ok0 || !ok1) LOG_W(TAG, "Idle hook registration failed — CPU load unavailable");
}

void watchTask(TaskHandle_t task, const char* name, uint32_t stackBytes, uint8_t core) {
    if (task == nullptr || s_task_count >= MAX_TASKS) return;
    s_tasks[s_task_count++] = {task, name, stackBytes, core};
}

void noteQueueDepth(uint32_t depth, uint32_t capacity) {
    s_q_depth = depth;
    s_q_cap   = capacity;
    if (depth > s_q_peak) s_q_peak = depth;
}

void noteLoop() {
    const uint32_t now = millis();
    if (s_loop_last_ms != 0) {
        const uint32_t period = now - s_loop_last_ms;
        if (period > s_loop_window_max) s_loop_window_max = period;
        if (period > s_loop_ever_max)   s_loop_ever_max = period;
    }
    s_loop_last_ms = now;
}

void sample() {
    const TickType_t now = xTaskGetTickCount();
    const TickType_t dt  = now - s_prev_sample;
    if (dt > 0) {
        for (uint8_t c = 0; c < 2; ++c) {
            const uint32_t idle = s_idle_ticks[c] - s_prev_idle[c];
            s_prev_idle[c] = s_idle_ticks[c];
            const uint32_t idle_pm = (idle >= dt) ? 1000u : (idle * 1000u) / dt;
            s_cpu_permille[c] = static_cast<int16_t>(1000u - idle_pm);
        }
    }
    s_prev_sample = now;

    s_loop_shown_max  = s_loop_window_max;
    s_loop_window_max = 0;

    // Filesystem usage walks the directory tree; ten seconds is plenty.
    if (s_slow_last_ms == 0 || millis() - s_slow_last_ms >= 10000) {
        s_slow_last_ms = millis();
        s_fs_total = LittleFS.totalBytes();
        s_fs_used  = LittleFS.usedBytes();
    }
}

void writeJson(JsonWriter& j) {
    // The image size verifies the whole app partition on first call, which is
    // slow. Do it once, on the web task, not during boot.
    if (!s_app_known) {
        s_app_used = ESP.getSketchSize();           // verifies the image: once
        // The partition, asked directly. getFreeSketchSpace() answers "how much
        // room is there for an OTA image", and this build has no second app
        // slot (partitions/avanza_test_16mb.csv drops it to give LittleFS the
        // room), so it returns 0 and the bar read 100 % full at every size.
        const esp_partition_t* run = esp_ota_get_running_partition();
        s_app_size = run ? run->size : s_app_used;
        s_app_known = true;
    }

    const size_t heap_total   = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    const size_t heap_free    = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    const size_t heap_min     = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    const size_t heap_largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    const size_t psram_total  = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    const size_t psram_free   = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const CanDriverCounters d = CanManager::driverCounters();

    j.add("\"health\":{");
    j.add("\"heap\":{\"total\":%u,\"free\":%u,\"min\":%u,\"largest\":%u},",
          (unsigned)heap_total, (unsigned)heap_free, (unsigned)heap_min,
          (unsigned)heap_largest);
    j.add("\"psram\":{\"total\":%u,\"free\":%u},", (unsigned)psram_total,
          (unsigned)psram_free);
    j.add("\"cpu\":[%d,%d],", s_cpu_permille[0], s_cpu_permille[1]);
    j.add("\"loop\":{\"expect\":%u,\"max\":%lu,\"ever\":%lu},", (unsigned)LED_TICK_MS,
          (unsigned long)s_loop_shown_max, (unsigned long)s_loop_ever_max);
    j.add("\"q\":{\"depth\":%lu,\"peak\":%lu,\"cap\":%lu},", (unsigned long)s_q_depth,
          (unsigned long)s_q_peak, (unsigned long)s_q_cap);
    j.add("\"drv\":{\"backlog\":%lu,\"peak\":%lu,\"cap\":%lu,\"missed\":%lu,"
          "\"overrun\":%lu,\"berr\":%lu,\"rec\":%lu},",
          (unsigned long)d.rx_backlog, (unsigned long)d.rx_backlog_peak,
          (unsigned long)d.rx_queue_len, (unsigned long)d.rx_missed,
          (unsigned long)d.rx_overrun, (unsigned long)d.bus_errors,
          (unsigned long)d.rx_error_counter);
    j.add("\"fs\":{\"total\":%lu,\"used\":%lu},", (unsigned long)s_fs_total,
          (unsigned long)s_fs_used);
    j.add("\"app\":{\"used\":%lu,\"size\":%lu},", (unsigned long)s_app_used,
          (unsigned long)s_app_size);
    j.add("\"tasks\":[");
    for (uint8_t i = 0; i < s_task_count; ++i) {
        const UBaseType_t free_min = uxTaskGetStackHighWaterMark(s_tasks[i].handle);
        j.add("%s{\"n\":\"%s\",\"c\":%u,\"s\":%lu,\"f\":%lu}", i ? "," : "",
              s_tasks[i].name, (unsigned)s_tasks[i].core,
              (unsigned long)s_tasks[i].stack_bytes, (unsigned long)free_min);
    }
    j.add("]}");
}

}  // namespace SystemHealth
