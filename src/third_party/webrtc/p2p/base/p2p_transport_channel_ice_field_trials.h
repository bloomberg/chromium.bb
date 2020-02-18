/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_P2P_TRANSPORT_CHANNEL_ICE_FIELD_TRIALS_H_
#define P2P_BASE_P2P_TRANSPORT_CHANNEL_ICE_FIELD_TRIALS_H_

#include "absl/types/optional.h"

namespace cricket {

// Field trials for P2PTransportChannel and friends,
// put in separate file so that they can be shared e.g
// with Connection.
struct IceFieldTrials {
  bool skip_relay_to_non_relay_connections = false;
  absl::optional<int> max_outstanding_pings;

  // Wait X ms before selecting a connection when having none.
  // This will make media slower, but will give us chance to find
  // a better connection before starting.
  absl::optional<int> initial_select_dampening;

  // If the connection has recevied a ping-request, delay by
  // maximum this delay. This will make media slower, but will
  // give us chance to find a better connection before starting.
  absl::optional<int> initial_select_dampening_ping_received;

  // Decay rate for RTT estimate using EventBasedExponentialMovingAverage
  // expressed as halving time.
  int rtt_estimate_halftime_ms = 500;
};

}  // namespace cricket

#endif  // P2P_BASE_P2P_TRANSPORT_CHANNEL_ICE_FIELD_TRIALS_H_
