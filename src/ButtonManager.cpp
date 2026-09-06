#include "ButtonManager.h"
#include "Logger.h"

namespace {

const char* TAG = "BUTTON";

bool     s_stable      = false;   // debounced state, true = pressed
bool     s_raw         = false;
uint32_t s_last_edge   = 0;
uint32_t s_press_start = 0;
uint32_t s_presses     = 0;
bool     s_long_fired  = false;

}  // namespace

namespace ButtonManager {

void begin() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);   // active low
    s_stable    = false;
    s_raw       = false;
    s_last_edge = millis();
    LOG_I(TAG, "GPIO%d ready (RESERVED — status display only)", BUTTON_PIN);
}

ButtonEvent poll() {
    const bool raw = (digitalRead(BUTTON_PIN) == LOW);
    const uint32_t now = millis();

    if (raw != s_raw) {
        s_raw = raw;
        s_last_edge = now;
        return ButtonEvent::None;
    }
    if (now - s_last_edge < BUTTON_DEBOUNCE_MS) {
        return ButtonEvent::None;
    }

    if (raw && !s_stable) {
        s_stable      = true;
        s_press_start = now;
        s_long_fired  = false;
        ++s_presses;
        return ButtonEvent::None;
    }

    if (raw && s_stable && !s_long_fired &&
        (now - s_press_start) >= BUTTON_LONG_PRESS_MS) {
        s_long_fired = true;
        LOG_I(TAG, "Long press (no action bound)");
        return ButtonEvent::LongPress;
    }

    if (!raw && s_stable) {
        s_stable = false;
        if (!s_long_fired) return ButtonEvent::ShortPress;
    }

    return ButtonEvent::None;
}

bool isPressed()      { return s_stable; }
uint32_t pressCount() { return s_presses; }

}  // namespace ButtonManager
