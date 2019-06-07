// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_EVENT_LOOP_H_
#define PLATFORM_BASE_EVENT_LOOP_H_

#include <sys/types.h>

#include <vector>

#include "osp_base/error.h"
#include "platform/api/event_waiter.h"
#include "platform/api/network_runner.h"

namespace openscreen {
namespace platform {

Error ReceiveDataFromEvent(const UdpSocketReadableEvent& read_event,
                           UdpReadCallback::Packet* data);
std::vector<UdpReadCallback::Packet> HandleUdpSocketReadEvents(
    const Events& events);
std::vector<UdpReadCallback::Packet> OnePlatformLoopIteration(
    EventWaiterPtr waiter);

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_EVENT_LOOP_H_
