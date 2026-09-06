// =============================================================================
//  WebDashboard.h — READ-ONLY diagnostic web view.
//
//  Serves one page plus a small JSON endpoint so a technician can watch CAN
//  frames arrive on a phone instead of holding a laptop in the passenger seat
//  of a moving car.
//
//  ###########################################################################
//  #  READ-ONLY. THERE ARE NO WRITE ENDPOINTS.                              #
//  #                                                                         #
//  #  Nothing here can enable CAN transmit or change the bitrate. Those are  #
//  #  not runtime properties and no HTTP route touches them. This module     #
//  #  never includes the TWAI driver header; it only reads the counters      #
//  #  CanManager publishes.                                                  #
//  ###########################################################################
//
//  Transport is plain HTTP polling, not WebSocket or SSE:
//    * no third-party library, so the build stays reproducible;
//    * each poll is an independent request, so a hotspot that drops for two
//      seconds costs two samples instead of a dead socket needing reconnect
//      logic — which matters in a moving vehicle;
//    * 500 ms is plainly realtime enough to watch frames scroll.
// =============================================================================
#pragma once

#include <Arduino.h>
#include "Config.h"
#include "CanManager.h"

namespace WebDashboard {

// Starts the HTTP server. Safe to call before WiFi is up; it simply has no
// clients until then.
void begin();

// Services pending clients. Runs in its own task on core 1.
void poll();

// Feeds the live-frame ring. Called from the storage task on core 1, never from
// the CAN reader on core 0 — the acquisition path does not pay for the
// dashboard.
void noteFrame(const CanFrame& frame);

bool     isEnabled();
uint32_t requestsServed();

}  // namespace WebDashboard
