// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/event_loop.h"

#include "platform/api/error.h"
#include "platform/api/logging.h"
#include "platform/api/socket.h"

namespace openscreen {
namespace platform {

ReceivedData::ReceivedData() = default;
ReceivedData::~ReceivedData() = default;

bool ReceiveDataFromEvent(const UdpSocketReadableEvent& read_event,
                          ReceivedData* data) {
  DCHECK(data);
  ssize_t len =
      ReceiveUdp(read_event.socket, &data->bytes[0], data->bytes.size(),
                 &data->source, &data->original_destination);
  if (len == -1) {
    LOG_ERROR << "recv() failed: " << GetLastErrorString();
    return false;
  } else if (len == 0) {
    LOG_WARN << "recv() = 0, closed?";
    return false;
  }
  DCHECK_LE(len, kUdpMaxPacketSize);
  data->length = len;
  data->socket = read_event.socket;
  return true;
}

std::vector<ReceivedData> HandleUdpSocketReadEvents(const Events& events) {
  std::vector<ReceivedData> data;
  for (const auto& read_event : events.udp_readable_events) {
    ReceivedData next_data;
    if (ReceiveDataFromEvent(read_event, &next_data)) {
      data.emplace_back(std::move(next_data));
    }
  }
  return data;
}

std::vector<ReceivedData> OnePlatformLoopIteration(EventWaiterPtr waiter) {
  Events events;
  if (!WaitForEvents(waiter, &events)) {
    return {};
  }
  return HandleUdpSocketReadEvents(events);
}

}  // namespace platform
}  // namespace openscreen
