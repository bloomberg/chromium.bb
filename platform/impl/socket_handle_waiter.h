// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_SOCKET_HANDLE_WAITER_H_
#define PLATFORM_IMPL_SOCKET_HANDLE_WAITER_H_

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "platform/api/time.h"
#include "platform/base/error.h"
#include "platform/base/macros.h"
#include "platform/impl/socket_handle.h"

namespace openscreen {

// The class responsible for calling platform-level method to watch UDP sockets
// for available read data. Reading from these sockets is handled at a higher
// layer.
class SocketHandleWaiter {
 public:
  using SocketHandleRef = std::reference_wrapper<const SocketHandle>;

  class Subscriber {
   public:
    virtual ~Subscriber() = default;

    // Provides a socket handle to the subscriber which has data waiting to be
    // processed.
    virtual void ProcessReadyHandle(SocketHandleRef handle) = 0;
  };

  explicit SocketHandleWaiter(ClockNowFunctionPtr now_function);
  virtual ~SocketHandleWaiter() = default;

  // Start notifying |subscriber| whenever |handle| has an event. May be called
  // multiple times, to be notified for multiple handles, but should not be
  // called multiple times for the same handle.
  void Subscribe(Subscriber* subscriber, SocketHandleRef handle);

  // Stop receiving notifications for one of the handles currently subscribed
  // to.
  void Unsubscribe(Subscriber* subscriber, SocketHandleRef handle);

  // Stop receiving notifications for all handles currently subscribed to, or
  // no-op if there are no subscriptions.
  void UnsubscribeAll(Subscriber* subscriber);

  // Called when a handle will be deleted to ensure that deletion can proceed
  // safely.
  void OnHandleDeletion(Subscriber* subscriber,
                        SocketHandleRef handle,
                        bool disable_locking_for_testing = false);

  OSP_DISALLOW_COPY_AND_ASSIGN(SocketHandleWaiter);

  // Gets all socket handles to process, checks them for readable data, and
  // handles any changes that have occured.
  Error ProcessHandles(Clock::duration timeout);

 protected:
  // Waits until data is available in one of the provided sockets or the
  // provided timeout has passed - whichever is first. If any sockets have data
  // available, they are returned.
  virtual ErrorOr<std::vector<SocketHandleRef>> AwaitSocketsReadable(
      const std::vector<SocketHandleRef>& socket_fds,
      const Clock::duration& timeout) = 0;

 private:
  struct HandleWithSubscriber {
    SocketHandleRef handle;
    Subscriber* subscriber;
  };

  // Call the subscriber associated with each changed handle.  Handles are only
  // processed until |timeout| is exceeded.  Must be called with |mutex_| held.
  void ProcessReadyHandles(const std::vector<HandleWithSubscriber>& handles,
                           Clock::duration timeout);

  // Guards against concurrent access to all other class data members.
  std::mutex mutex_;

  // Blocks deletion of handles until they are no longer being watched.
  std::condition_variable handle_deletion_block_;

  // Set of handles currently being deleted, for ensuring handle_deletion_block_
  // does not exit prematurely.
  std::vector<SocketHandleRef> handles_being_deleted_;

  // Set of all socket handles currently being watched, mapped to the subscriber
  // that is watching them.
  std::unordered_map<SocketHandleRef, Subscriber*, SocketHandleHash>
      handle_mappings_;

  const ClockNowFunctionPtr now_function_;
};

}  // namespace openscreen

#endif  // PLATFORM_IMPL_SOCKET_HANDLE_WAITER_H_
