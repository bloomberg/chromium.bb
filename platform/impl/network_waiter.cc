// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_waiter.h"

#include <algorithm>
#include <atomic>

#include "absl/algorithm/container.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

void NetworkWaiter::Subscribe(Subscriber* subscriber, SocketHandleRef handle) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (handle_mappings_.find(handle) == handle_mappings_.end()) {
    handle_mappings_.emplace(handle, subscriber);
  }
}

void NetworkWaiter::Unsubscribe(Subscriber* subscriber,
                                SocketHandleRef handle) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto iterator = handle_mappings_.find(handle);
  if (handle_mappings_.find(handle) != handle_mappings_.end()) {
    handle_mappings_.erase(iterator);
  }
}

void NetworkWaiter::UnsubscribeAll(Subscriber* subscriber) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = handle_mappings_.begin(); it != handle_mappings_.end();) {
    if (it->second == subscriber) {
      it = handle_mappings_.erase(it);
    } else {
      it++;
    }
  }
}

void NetworkWaiter::OnHandleDeletion(Subscriber* subscriber,
                                     SocketHandleRef handle,
                                     bool disable_locking_for_testing) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = handle_mappings_.find(handle);
  if (it != handle_mappings_.end()) {
    handle_mappings_.erase(it);
    if (!disable_locking_for_testing) {
      // This code will allow us to block completion of the socket destructor
      // (and subsequent invalidation of pointers to this socket) until we no
      // longer are waiting on a SELECT(...) call to it, since we only signal
      // this condition variable's wait(...) to proceed outside of SELECT(...).
      handle_deletion_block_.wait(lock);
    }
  }
}

void NetworkWaiter::ProcessReadyHandles(
    const std::vector<SocketHandleRef>& handles) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const SocketHandleRef& handle : handles) {
    auto iterator = handle_mappings_.find(handle);
    if (iterator == handle_mappings_.end()) {
      // This is OK: SocketHandle was deleted in the meantime.
      continue;
    }

    iterator->second->ProcessReadyHandle(handle);
  }
}

Error NetworkWaiter::ProcessHandles(const Clock::duration& timeout) {
  std::vector<SocketHandleRef> handles;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    handle_deletion_block_.notify_all();
    handles.reserve(handle_mappings_.size());
    for (const auto& pair : handle_mappings_) {
      handles.push_back(pair.first);
    }
  }

  ErrorOr<std::vector<SocketHandleRef>> changed_handles =
      AwaitSocketsReadable(handles, timeout);
  handle_deletion_block_.notify_all();
  if (changed_handles.is_error()) {
    return changed_handles.error();
  }

  ProcessReadyHandles(changed_handles.value());
  return Error::None();
}

}  // namespace platform
}  // namespace openscreen
