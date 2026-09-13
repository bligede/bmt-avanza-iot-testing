#include "FrameRing.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "Logger.h"

namespace {

SemaphoreHandle_t s_mutex = nullptr;
CanFrame          s_ring[WEB_FRAME_RING];
uint16_t          s_head  = 0;
uint32_t          s_total = 0;

bool lock(TickType_t wait) {
    return s_mutex != nullptr && xSemaphoreTake(s_mutex, wait) == pdTRUE;
}
void unlock() { if (s_mutex != nullptr) xSemaphoreGive(s_mutex); }

}  // namespace

namespace FrameRing {

void begin() {
    if (s_mutex == nullptr) s_mutex = xSemaphoreCreateMutex();
    s_head  = 0;
    s_total = 0;
}

void push(const CanFrame& frame) {
    if (!lock(0)) return;               // never stall the storage task
    s_ring[s_head] = frame;
    s_head = static_cast<uint16_t>((s_head + 1) % WEB_FRAME_RING);
    ++s_total;
    unlock();
}

void writeJson(JsonWriter& j) {
    j.add("\"frames\":[");
    if (lock(pdMS_TO_TICKS(20))) {
        const uint16_t have = (s_total < WEB_FRAME_RING)
                                  ? static_cast<uint16_t>(s_total)
                                  : static_cast<uint16_t>(WEB_FRAME_RING);
        for (uint16_t k = 0; k < have && !j.overflow(); ++k) {
            const uint16_t idx = static_cast<uint16_t>(
                (s_head + WEB_FRAME_RING - 1 - k) % WEB_FRAME_RING);
            const CanFrame& f = s_ring[idx];
            char bytes[32];
            Logger::formatBytes(bytes, sizeof(bytes), f.data, f.dlc);
            j.add("%s{\"t\":%lu,\"id\":%lu,\"dlc\":%u,\"ext\":%s,\"d\":\"%s\"}",
                  k ? "," : "", (unsigned long)f.rx_millis, (unsigned long)f.id,
                  (unsigned)f.dlc, f.extended ? "true" : "false", bytes);
        }
        unlock();
    }
    j.add("]");
}

}  // namespace FrameRing
