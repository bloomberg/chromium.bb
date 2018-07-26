// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_EVENT_WAITER_H_
#define PLATFORM_API_EVENT_WAITER_H_

#include <vector>

#include "platform/api/socket.h"
#include "platform/api/time.h"

namespace openscreen {
namespace platform {

struct EventWaiterPrivate;
using EventWaiterPtr = EventWaiterPrivate*;

struct UdpSocketIPv4ReadableEvent {
  UdpSocketIPv4Ptr socket;
};

struct UdpSocketIPv6ReadableEvent {
  UdpSocketIPv6Ptr socket;
};

struct UdpSocketIPv4WritableEvent {
  UdpSocketIPv4Ptr socket;
};

struct UdpSocketIPv6WritableEvent {
  UdpSocketIPv6Ptr socket;
};

// This struct represents a set of events associated with a particular
// EventWaiterPtr and is created by WaitForEvents.  Any combination and number
// of events may be present, depending on how the platform implements event
// waiting and what has occured since the last WaitForEvents call.
struct Events {
  Events();
  ~Events();
  Events(Events&& o);
  Events& operator=(Events&& o);

  std::vector<UdpSocketIPv4ReadableEvent> udpv4_readable_events;
  std::vector<UdpSocketIPv6ReadableEvent> udpv6_readable_events;
  std::vector<UdpSocketIPv4WritableEvent> udpv4_writable_events;
  std::vector<UdpSocketIPv6WritableEvent> udpv6_writable_events;
};

EventWaiterPtr CreateEventWaiter();
void DestroyEventWaiter(EventWaiterPtr waiter);

// Returns true if |socket| was successfully added to the set of watched
// sockets, false otherwise.  It will also return false if |socket| is already
// being watched.
bool WatchUdpSocketIPv4Readable(EventWaiterPtr waiter, UdpSocketIPv4Ptr socket);
bool WatchUdpSocketIPv6Readable(EventWaiterPtr waiter, UdpSocketIPv6Ptr socket);

// Returns true if |socket| was successfully removed from the set of watched
// sockets.
bool StopWatchingUdpSocketIPv4Readable(EventWaiterPtr waiter,
                                       UdpSocketIPv4Ptr socket);
bool StopWatchingUdpSocketIPv6Readable(EventWaiterPtr waiter,
                                       UdpSocketIPv6Ptr socket);

// Returns true if |socket| was successfully added to the set of watched
// sockets, false otherwise.  It will also return false if |socket| is already
// being watched.
bool WatchUdpSocketIPv4Writable(EventWaiterPtr waiter, UdpSocketIPv4Ptr socket);
bool WatchUdpSocketIPv6Writable(EventWaiterPtr waiter, UdpSocketIPv6Ptr socket);

// Returns true if |socket| was successfully removed from the set of watched
// sockets.
bool StopWatchingUdpSocketIPv4Writable(EventWaiterPtr waiter,
                                       UdpSocketIPv4Ptr socket);
bool StopWatchingUdpSocketIPv6Writable(EventWaiterPtr waiter,
                                       UdpSocketIPv6Ptr socket);

// Returns true if |waiter| successfully started monitoring network change
// events, false otherwise.  It will also return false if |waiter| is already
// monitoring network change events.
bool WatchNetworkChange(EventWaiterPtr waiter);

// Returns true if |waiter| successfully stopped monitoring network change
// events, false otherwise.  It will also return false if |waiter| wasn't
// monitoring network change events already.
bool StopWatchingNetworkChange(EventWaiterPtr waiter);

// Returns the number of events that were added to |events| if there were any, 0
// if there were no events, and -1 if an error occurred.
int WaitForEvents(EventWaiterPtr waiter, Events* events);

}  // namespace platform
}  // namespace openscreen

#endif
