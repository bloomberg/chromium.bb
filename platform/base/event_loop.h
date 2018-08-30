// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_EVENT_LOOP_H_
#define PLATFORM_BASE_EVENT_LOOP_H_

#include <sys/types.h>

#include <array>
#include <cstdint>
#include <vector>

#include "base/ip_address.h"
#include "platform/api/event_waiter.h"

namespace openscreen {
namespace platform {

static constexpr int kUdpMaxPacketSize = 1 << 16;

struct ReceivedDataIPv4 {
  ReceivedDataIPv4();
  ~ReceivedDataIPv4();

  IPv4Endpoint source;
  IPv4Endpoint original_destination;
  std::array<uint8_t, kUdpMaxPacketSize> bytes;
  ssize_t length;
  UdpSocketIPv4Ptr socket;
};

struct ReceivedDataIPv6 {
  ReceivedDataIPv6();
  ~ReceivedDataIPv6();

  IPv6Endpoint source;
  IPv6Endpoint original_destination;
  std::array<uint8_t, kUdpMaxPacketSize> bytes;
  ssize_t length;
  UdpSocketIPv6Ptr socket;
};

struct ReceivedData {
  ReceivedData();
  ~ReceivedData();
  ReceivedData(ReceivedData&& o);
  ReceivedData& operator=(ReceivedData&& o);

  std::vector<ReceivedDataIPv4> v4_data;
  std::vector<ReceivedDataIPv6> v6_data;
};

bool ReceiveDataFromIPv4Event(const UdpSocketIPv4ReadableEvent& read_event,
                              ReceivedDataIPv4* data);
bool ReceiveDataFromIPv6Event(const UdpSocketIPv6ReadableEvent& read_event,
                              ReceivedDataIPv6* data);
ReceivedData HandleUdpSocketReadEvents(const Events& events);
ReceivedData OnePlatformLoopIteration(EventWaiterPtr waiter);

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_EVENT_LOOP_H_
