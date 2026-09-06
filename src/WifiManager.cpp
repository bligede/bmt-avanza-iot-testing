#include "WifiManager.h"

#include "Logger.h"

#if ENABLE_WIFI
  #include <WiFi.h>
#endif

namespace {

const char* TAG = "WIFI";

WifiState  s_state       = WifiState::Disabled;
WifiStats  s_stats       = {};
uint32_t   s_state_since = 0;

void setState(WifiState next) {
    if (next == s_state) return;
    LOG_I(TAG, "%s -> %s", WifiManager::stateName(s_state),
          WifiManager::stateName(next));
    s_state       = next;
    s_state_since = millis();
}

// Only the radio path uses these; declaring them unconditionally would leave
// unused-variable warnings in every production build.
#if ENABLE_WIFI
uint32_t s_retry_at_ms = 0;
uint32_t s_backoff_ms  = WIFI_RECONNECT_BACKOFF_MS;

uint32_t inState() { return millis() - s_state_since; }

void beginAttempt() {
    // Station only. This device never becomes an access point: an open AP in a
    // vehicle would be an unnecessary attack surface for a diagnostic tool.
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);          // latency over power; this is a bench tool
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    setState(WifiState::Connecting);
}
#endif

}  // namespace

namespace WifiManager {

const char* stateName(WifiState s) {
    switch (s) {
        case WifiState::Disabled:     return "DISABLED";
        case WifiState::Idle:         return "IDLE";
        case WifiState::Connecting:   return "CONNECTING";
        case WifiState::Connected:    return "CONNECTED";
        case WifiState::Reconnecting: return "RECONNECTING";
        case WifiState::Failed:       return "FAILED";
        default:                      return "?";
    }
}

bool isConfigured() {
#if ENABLE_WIFI
    return WIFI_SSID[0] != '\0';
#else
    return false;
#endif
}

void begin() {
    memset(&s_stats, 0, sizeof(s_stats));
    snprintf(s_stats.ip, sizeof(s_stats.ip), "0.0.0.0");

#if !ENABLE_WIFI
    s_state = WifiState::Disabled;
    LOG_I(TAG, "Disabled (ENABLE_WIFI = 0) — production uplink is 4G + MQTT/TLS");
    return;
#else
    snprintf(s_stats.ssid, sizeof(s_stats.ssid), "%s", WIFI_SSID);

    if (!isConfigured()) {
        LOG_W(TAG, "No WIFI_SSID configured — WiFi stays down");
        setState(WifiState::Failed);
        return;
    }

    LOG_W(TAG, "DIAGNOSTIC WiFi enabled. This is NOT the production uplink.");
    LOG_I(TAG, "Connecting to SSID '%s'", WIFI_SSID);
    setState(WifiState::Idle);
    beginAttempt();
#endif
}

void poll() {
#if !ENABLE_WIFI
    return;
#else
    switch (s_state) {

    case WifiState::Disabled:
    case WifiState::Failed:
        break;

    case WifiState::Idle:
        if (static_cast<int32_t>(millis() - s_retry_at_ms) >= 0) beginAttempt();
        break;

    case WifiState::Connecting:
    case WifiState::Reconnecting:
        if (WiFi.status() == WL_CONNECTED) {
            ++s_stats.connects;
            s_stats.connected_since_ms = millis();
            snprintf(s_stats.ip, sizeof(s_stats.ip), "%s",
                     WiFi.localIP().toString().c_str());
            s_backoff_ms = WIFI_RECONNECT_BACKOFF_MS;
            LOG_I(TAG, "Connected. IP %s, RSSI %d dBm",
                  s_stats.ip, (int)WiFi.RSSI());
            LOG_I(TAG, "Dashboard: http://%s/", s_stats.ip);
            setState(WifiState::Connected);
            break;
        }
        if (inState() > WIFI_CONNECT_TIMEOUT_MS) {
            ++s_stats.connect_failures;
            LOG_W(TAG, "Connect timed out (failure %lu) — retrying in %lu ms",
                  (unsigned long)s_stats.connect_failures,
                  (unsigned long)s_backoff_ms);
            WiFi.disconnect();
            s_retry_at_ms = millis() + s_backoff_ms;
            s_backoff_ms  = (s_backoff_ms * 2 > WIFI_RECONNECT_BACKOFF_MAX)
                          ? WIFI_RECONNECT_BACKOFF_MAX : s_backoff_ms * 2;
            setState(WifiState::Idle);
        }
        break;

    case WifiState::Connected:
        if (WiFi.status() != WL_CONNECTED) {
            ++s_stats.disconnects;
            snprintf(s_stats.ip, sizeof(s_stats.ip), "0.0.0.0");
            LOG_W(TAG, "Link lost — reconnecting");
            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASS);
            setState(WifiState::Reconnecting);
            break;
        }
        s_stats.rssi = WiFi.RSSI();
        break;
    }
#endif
}

bool isConnected() { return s_state == WifiState::Connected; }
WifiState state()  { return s_state; }

WifiStats stats() {
    WifiStats s = s_stats;
    if (s_state != WifiState::Connected) s.rssi = 0;
    return s;
}

void end() {
#if ENABLE_WIFI
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
#endif
    setState(WifiState::Disabled);
}

}  // namespace WifiManager
