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

namespace openscreen {
namespace platform {

// This is the class responsible for watching sockets for readable data, then
// calling the function associated with these sockets once that data is read.
// NOTE: This class will only function as intended while its RunUntilStopped
// method is running.
// TODO(rwkeane): Rename this class NetworkReaderPosix.
class NetworkReader : public UdpSocket::LifetimeObserver {
 public:
  // Create a type for readability
  using Callback = std::function<void(UdpPacket)>;

  // Creates a new instance of this object.
  NetworkReader();
  virtual ~NetworkReader();

  // Runs the Wait function in a loop until the below RequestStopSoon function
  // is called.
  void RunUntilStopped();

  // Signals for the RunUntilStopped loop to cease running.
  void RequestStopSoon();

  // UdpSocket::LifetimeObserver overrides.
  // Waits for |socket| to be readable and then calls the socket's
  // RecieveMessage(...) method to process the available packet.
  // NOTE: The first read on any newly watched socket may be delayed up to 50
  // ms.
  void OnCreate(UdpSocket* socket) override;

  // Cancels any pending wait on reading |socket|. Following this call, any
  // pending reads will proceed but their associated callbacks will not fire.
  // NOTE: This method will block until a delete is safe.
  void OnDestroy(UdpSocket* socket) override;

 protected:
  // Creates a new instance of this object.
  explicit NetworkReader(std::unique_ptr<NetworkWaiter> waiter);

  // Waits for any writes to occur or for timeout to pass, whichever is sooner.
  // If an error occurs when calling WaitAndRead, then no callbacks will have
  // been called during the method's execution, but it is still safe to
  // immediately call again.
  // NOTE: Must be protected rather than private for UT purposes.
  // NOTE: If a socket callback is removed in the middle of a wait call, data
  // may be read on this socket and but the callback may not be called. If a
  // socket callback is added in the middle of a wait call, the new socket may
  // not be watched until after this wait call ends.
  Error WaitAndRead(Clock::duration timeout);

  // Helper method to allow for OnDestroy calls without blocking.
  void OnDelete(UdpSocket* socket, bool disable_locking_for_testing = false);

  // The set of all sockets that are being read from
  std::vector<UdpSocket*> sockets_;

 private:
  // Abstractions around socket handling to ensure platform independence.
  std::unique_ptr<NetworkWaiter> waiter_;

  // Mutex to protect against concurrent modification of socket info.
  std::mutex mutex_;

  // Atomic so that we can perform atomic exchanges.
  std::atomic_bool is_running_;

  // Blocks deletion of sockets until they are no longer being watched.
  std::condition_variable socket_deletion_block_;

  OSP_DISALLOW_COPY_AND_ASSIGN(NetworkReader);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_NETWORK_READER_H_
