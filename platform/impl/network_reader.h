// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_NETWORK_READER_H_
#define PLATFORM_IMPL_NETWORK_READER_H_

#include <map>
#include <mutex>  // NOLINT
#include <vector>

#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "platform/api/udp_socket.h"
#include "platform/impl/network_waiter.h"
#include "platform/impl/socket_handle.h"

namespace openscreen {
namespace platform {

// This is the class responsible for watching sockets for readable data, then
// calling the function associated with these sockets once that data is read.
// NOTE: This class will only function as intended while its RunUntilStopped
// method is running.
// TODO(rwkeane): Rename this class NetworkReaderPosix.
class NetworkReader : public UdpSocket::LifetimeObserver,
                      NetworkWaiter::Subscriber {
 public:
  using SocketHandleRef = NetworkWaiter::SocketHandleRef;

  // Creates a new instance of this object.
  // NOTE: The provided NetworkWaiter must outlive this object.
  explicit NetworkReader(NetworkWaiter* waiter);
  virtual ~NetworkReader();

  // UdpSocket::LifetimeObserver overrides.
  // Waits for |socket| to be readable and then calls the socket's
  // RecieveMessage(...) method to process the available packet.
  // NOTE: The first read on any newly watched socket may be delayed up to 50
  // ms.
  void OnCreate(UdpSocket* socket) override;

  // Cancels any pending wait on reading |socket|. Following this call, any
  // pending reads will proceed but their associated callbacks will not fire.
  // NOTE: This method will block until a delete is safe.
  // NOTE: If a socket callback is removed in the middle of a wait call, data
  // may be read on this socket and but the callback may not be called. If a
  // socket callback is added in the middle of a wait call, the new socket may
  // not be watched until after this wait call ends.
  void OnDestroy(UdpSocket* socket) override;

  // NetworkWaiter::Subscriber overrides.
  void ProcessReadyHandle(SocketHandleRef handle) override;

 protected:
  bool IsMappedReadForTesting(UdpSocket* socket) const {
    return std::find(sockets_.begin(), sockets_.end(), socket) !=
           sockets_.end();
  }

 private:
  // Helper method to allow for OnDestroy calls without blocking.
  void OnDelete(UdpSocket* socket, bool disable_locking_for_testing = false);

  // The set of all sockets that are being read from
  // TODO(rwkeane): Change to std::vector<UdpSocketPosix*>
  std::vector<UdpSocket*> sockets_;

  // Mutex to protect against concurrent modification of socket info.
  std::mutex mutex_;

  // NetworkWaiter watching this NetworkReader.
  NetworkWaiter* const waiter_;

  friend class TestingNetworkReader;

  OSP_DISALLOW_COPY_AND_ASSIGN(NetworkReader);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_NETWORK_READER_H_
