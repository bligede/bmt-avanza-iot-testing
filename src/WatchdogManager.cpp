#include "WatchdogManager.h"

#include <esp_task_wdt.h>
#include <esp_system.h>
#include "Logger.h"

namespace {

const char* TAG = "WDT";
bool s_armed = false;
esp_reset_reason_t s_reset_reason = ESP_RST_UNKNOWN;

}  // namespace

namespace WatchdogManager {

void begin() {
    s_reset_reason = esp_reset_reason();

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    // Core 3.x takes a config struct and can already be initialised by the
    // Arduino core, in which case reconfiguring is the right call.
    esp_task_wdt_config_t cfg = {};
    cfg.timeout_ms    = WDT_TIMEOUT_MS;
    cfg.idle_core_mask = 0;          // the idle tasks are not supervised
    cfg.trigger_panic = WDT_PANIC_ON_TIMEOUT != 0;

    esp_err_t err = esp_task_wdt_init(&cfg);
    if (err == ESP_ERR_INVALID_STATE) {
        err = esp_task_wdt_reconfigure(&cfg);
    }
#else
    esp_err_t err = esp_task_wdt_init(WDT_TIMEOUT_MS / 1000, WDT_PANIC_ON_TIMEOUT != 0);
#endif

    if (err != ESP_OK) {
        LOG_E(TAG, "Init failed: %s — running unsupervised", esp_err_to_name(err));
        s_armed = false;
        return;
    }

    s_armed = true;
    LOG_I(TAG, "Armed at %lu ms, panic on timeout: %s",
          (unsigned long)WDT_TIMEOUT_MS, WDT_PANIC_ON_TIMEOUT ? "yes" : "no");

    if (lastResetWasFault()) {
        LOG_W(TAG, "Previous boot ended in %s — investigate the blocking call, "
                   "do not widen the timeout", lastResetReason());
    } else {
        LOG_I(TAG, "Previous reset: %s", lastResetReason());
    }
}

bool subscribeCurrentTask(const char* name) {
    if (!s_armed) return false;
    const esp_err_t err = esp_task_wdt_add(nullptr);
    if (err != ESP_OK && err != ESP_ERR_INVALID_ARG) {
        LOG_W(TAG, "Cannot supervise task %s: %s", name, esp_err_to_name(err));
        return false;
    }
    LOG_D(TAG, "Supervising %s", name);
    return true;
}

void feed() {
    if (s_armed) esp_task_wdt_reset();
}

void unsubscribeCurrentTask() {
    if (s_armed) esp_task_wdt_delete(nullptr);
}

bool isArmed() { return s_armed; }

const char* lastResetReason() {
    switch (s_reset_reason) {
        case ESP_RST_POWERON:   return "power on";
        case ESP_RST_EXT:       return "external reset";
        case ESP_RST_SW:        return "software restart";
        case ESP_RST_PANIC:     return "panic";
        case ESP_RST_INT_WDT:   return "interrupt watchdog";
        case ESP_RST_TASK_WDT:  return "task watchdog";
        case ESP_RST_WDT:       return "other watchdog";
        case ESP_RST_DEEPSLEEP: return "deep sleep wake";
        case ESP_RST_BROWNOUT:  return "brownout";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "unknown";
    }
}

bool lastResetWasFault() {
    return s_reset_reason == ESP_RST_PANIC ||
           s_reset_reason == ESP_RST_INT_WDT ||
           s_reset_reason == ESP_RST_TASK_WDT ||
           s_reset_reason == ESP_RST_WDT;
}

}  // namespace WatchdogManager
