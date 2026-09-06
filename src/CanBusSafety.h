// =============================================================================
//  CanBusSafety.h — makes "listen-only" a build-time property, not a promise.
//
//  This device is a monitoring device. It must never place a dominant bit on
//  the vehicle bus: no data frames, no remote frames, no ACK, no error frames.
//  Blueprint §4 ("MODE OPERASI PERANGKAT"), §12.2.1, §6.2; master prompt §2.
//
//  Three independent layers enforce that:
//
//    1. TWAI_MODE_LISTEN_ONLY is passed to twai_driver_install() and re-passed
//       on every reinstall/recovery. The controller physically cannot drive the
//       bus in this mode. CanManager latches the fact and refuses to start if
//       the mode word is anything else.
//    2. This header poisons the transmit entry points. Any translation unit
//       that includes it fails to COMPILE if twai_transmit() is referenced.
//    3. tools/check_listen_only.sh greps the tree for the same identifiers and
//       for this header's presence in every CAN-facing file. Run it in CI and
//       before every release build.
//
//  The poison is UNCONDITIONAL — deliberately. An earlier draft guarded it with
//  #if defined(ARDUINO), which meant the guard silently vanished in any file
//  that had not already included <Arduino.h>. A safety check that can switch
//  itself off is not a safety check. There is no configuration symbol that
//  disables this; the only way to remove it is to delete this file, and that
//  shows up in review.
//
//  USAGE: include this LAST in the include block of every .cpp that touches
//  CAN. It must come after driver/twai.h, because #pragma GCC poison also
//  rejects the identifier in a declaration, not just in a call.
// =============================================================================
#pragma once

// Anything that puts traffic on the bus, or arms something that would.
#pragma GCC poison twai_transmit
#pragma GCC poison twai_transmit_v2
#pragma GCC poison twai_clear_transmit_queue
