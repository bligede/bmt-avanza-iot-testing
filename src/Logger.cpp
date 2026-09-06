#include "Logger.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdarg.h>

namespace {

SemaphoreHandle_t s_mutex = nullptr;
char s_line[LOG_LINE_MAX];
char s_redacted[33];

const char* levelName(int level) {
    switch (level) {
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_WARN:  return "WARN ";
        case LOG_LEVEL_INFO:  return "INFO ";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        default:              return "?????";
    }
}

}  // namespace

namespace Logger {

void begin(unsigned long baud) {
    if (s_mutex == nullptr) {
        s_mutex = xSemaphoreCreateMutex();
    }
    Serial.begin(baud);
    // Do not block forever waiting for a host: the device runs headless in a
    // vehicle and CAN acquisition must not depend on a serial monitor.
    const uint32_t deadline = millis() + 1500;
    while (!Serial && millis() < deadline) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void printf(int level, const char* tag, const char* fmt, ...) {
    if (s_mutex != nullptr && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return;  // never let logging stall a caller; drop the line instead
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(s_line, sizeof(s_line), fmt, args);
    va_end(args);

    // "  12345 ms [INFO ] [CAN] Bitrate: 500 kbps"
    Serial.printf("%8lu ms [%s] [%s] %s\r\n",
                  static_cast<unsigned long>(millis()), levelName(level), tag, s_line);

    if (s_mutex != nullptr) {
        xSemaphoreGive(s_mutex);
    }
}

void formatBytes(char* out, size_t outLen, const uint8_t* data, uint8_t len) {
    if (out == nullptr || outLen == 0) return;
    out[0] = '\0';
    size_t pos = 0;
    for (uint8_t i = 0; i < len; ++i) {
        // 3 chars per byte plus the terminator.
        if (pos + 4 > outLen) break;
        pos += snprintf(out + pos, outLen - pos, "%s%02X", (i == 0) ? "" : " ", data[i]);
    }
}

const char* redact(const char* secret) {
    if (secret == nullptr) return "(null)";
    size_t n = strlen(secret);
    if (n > sizeof(s_redacted) - 1) n = sizeof(s_redacted) - 1;
    memset(s_redacted, '*', n);
    s_redacted[n] = '\0';
    return s_redacted;
}

}  // namespace Logger
