// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/event_waiter.h"

#include <sys/select.h>

#include <algorithm>
#include <vector>

#include "platform/api/logging.h"
#include "platform/posix/socket.h"

namespace openscreen {
namespace platform {
namespace {

template <typename T>
bool WatchUdpSocket(std::vector<T>* watched_sockets, T socket) {
  for (const auto* s : *watched_sockets) {
    if (s->fd == socket->fd)
      return false;
  }
  watched_sockets->push_back(socket);
  return true;
}

template <typename T>
bool StopWatchingUdpSocket(std::vector<T>* watched_sockets, T socket) {
  const auto it = std::find_if(watched_sockets->begin(), watched_sockets->end(),
                               [socket](T s) { return s->fd == socket->fd; });
  if (it == watched_sockets->end())
    return false;

  watched_sockets->erase(it);
  return true;
}

}  // namespace

struct EventWaiterPrivate {
  std::vector<UdpSocketPtr> read_sockets;
  std::vector<UdpSocketPtr> write_sockets;
};

EventWaiterPtr CreateEventWaiter() {
  return new EventWaiterPrivate;
}

void DestroyEventWaiter(EventWaiterPtr waiter) {
  delete waiter;
}

bool WatchUdpSocketReadable(EventWaiterPtr waiter, UdpSocketPtr socket) {
  return WatchUdpSocket(&waiter->read_sockets, socket);
}

bool StopWatchingUdpSocketReadable(EventWaiterPtr waiter, UdpSocketPtr socket) {
  return StopWatchingUdpSocket(&waiter->read_sockets, socket);
}

bool WatchUdpSocketWritable(EventWaiterPtr waiter, UdpSocketPtr socket) {
  return WatchUdpSocket(&waiter->write_sockets, socket);
}

bool StopWatchingUdpSocketWritable(EventWaiterPtr waiter, UdpSocketPtr socket) {
  return StopWatchingUdpSocket(&waiter->write_sockets, socket);
}

bool WatchNetworkChange(EventWaiterPtr waiter) {
  // TODO(btolsch): Implement network change watching.
  UNIMPLEMENTED();
  return false;
}

bool StopWatchingNetworkChange(EventWaiterPtr waiter) {
  // TODO(btolsch): Implement stop network change watching.
  UNIMPLEMENTED();
  return false;
}

int WaitForEvents(EventWaiterPtr waiter, Events* events) {
  int nfds = -1;
  fd_set readfds;
  fd_set writefds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  for (const auto* read_socket : waiter->read_sockets) {
    FD_SET(read_socket->fd, &readfds);
    nfds = std::max(nfds, read_socket->fd);
  }
  for (const auto* write_socket : waiter->write_sockets) {
    FD_SET(write_socket->fd, &writefds);
    nfds = std::max(nfds, write_socket->fd);
  }
  if (nfds == -1)
    return 0;

  struct timeval tv = {};
  const int rv = select(nfds, &readfds, &writefds, nullptr, &tv);
  if (rv == -1 || rv == 0)
    return rv;

  for (auto* read_socket : waiter->read_sockets) {
    if (FD_ISSET(read_socket->fd, &readfds))
      events->udp_readable_events.push_back({read_socket});
  }
  for (auto* write_socket : waiter->write_sockets) {
    if (FD_ISSET(write_socket->fd, &writefds))
      events->udp_writable_events.push_back({write_socket});
  }
  return rv;
}

}  // namespace platform
}  // namespace openscreen
