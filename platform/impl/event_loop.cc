// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/event_loop.h"

#include <utility>

#include "platform/api/logging.h"
#include "platform/api/udp_socket.h"

namespace openscreen {
namespace platform {

std::vector<UdpPacket> HandleUdpSocketReadEvents(const Events& events) {
  std::vector<UdpPacket> packets(events.udp_readable_events.size());
  for (const auto& read_event : events.udp_readable_events) {
    ErrorOr<UdpPacket> result = read_event.socket->ReceiveMessage();
    if (result) {
      packets.emplace_back(result.MoveValue());
    } else {
      OSP_LOG_ERROR << "ReceiveMessage() on socket failed: "
                    << result.error().message();
    }
  }
  return packets;
}

std::vector<UdpPacket> OnePlatformLoopIteration(EventWaiterPtr waiter) {
  ErrorOr<Events> events = WaitForEvents(waiter);
  if (!events)
    return {};

  return HandleUdpSocketReadEvents(events.value());
}

}  // namespace platform
}  // namespace openscreen
