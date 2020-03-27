// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/socket_handle_waiter.h"

#include <algorithm>
#include <atomic>

#include "absl/algorithm/container.h"
#include "util/logging.h"

namespace openscreen {

SocketHandleWaiter::SocketHandleWaiter(ClockNowFunctionPtr now_function)
    : now_function_(now_function) {}

void SocketHandleWaiter::Subscribe(Subscriber* subscriber,
                                   SocketHandleRef handle) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (handle_mappings_.find(handle) == handle_mappings_.end()) {
    handle_mappings_.emplace(handle, subscriber);
  }
}

void SocketHandleWaiter::Unsubscribe(Subscriber* subscriber,
                                     SocketHandleRef handle) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto iterator = handle_mappings_.find(handle);
  if (handle_mappings_.find(handle) != handle_mappings_.end()) {
    handle_mappings_.erase(iterator);
  }
}

void SocketHandleWaiter::UnsubscribeAll(Subscriber* subscriber) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = handle_mappings_.begin(); it != handle_mappings_.end();) {
    if (it->second == subscriber) {
      it = handle_mappings_.erase(it);
    } else {
      it++;
    }
  }
}

void SocketHandleWaiter::OnHandleDeletion(Subscriber* subscriber,
                                          SocketHandleRef handle,
                                          bool disable_locking_for_testing) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = handle_mappings_.find(handle);
  if (it != handle_mappings_.end()) {
    handle_mappings_.erase(it);
    if (!disable_locking_for_testing) {
      handles_being_deleted_.push_back(handle);

      OSP_DVLOG << "Starting to block for handle deletion";
      // This code will allow us to block completion of the socket destructor
      // (and subsequent invalidation of pointers to this socket) until we no
      // longer are waiting on a SELECT(...) call to it, since we only signal
      // this condition variable's wait(...) to proceed outside of SELECT(...).
      handle_deletion_block_.wait(lock, [this, handle]() {
        return std::find(handles_being_deleted_.begin(),
                         handles_being_deleted_.end(),
                         handle) == handles_being_deleted_.end();
      });
      OSP_DVLOG << "\tDone blocking for handle deletion!";
    }
  }
}

void SocketHandleWaiter::ProcessReadyHandles(
    const std::vector<HandleWithSubscriber>& handles,
    Clock::duration timeout) {
  Clock::time_point start_time = now_function_();
  bool processed_one = false;
  // TODO(btolsch): Track explicit or implicit time since last handled on each
  // watched handle so we can sort by it here for better fairness.
  for (const HandleWithSubscriber& handle : handles) {
    Clock::time_point current_time = now_function_();
    if (processed_one && (current_time - start_time) > timeout) {
      return;
    }

    processed_one = true;
    handle.subscriber->ProcessReadyHandle(handle.handle);

    current_time = now_function_();
    if ((current_time - start_time) > timeout) {
      return;
    }
  }
}

Error SocketHandleWaiter::ProcessHandles(Clock::duration timeout) {
  Clock::time_point start_time = now_function_();
  std::vector<SocketHandleRef> handles;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    handles_being_deleted_.clear();
    handle_deletion_block_.notify_all();
    handles.reserve(handle_mappings_.size());
    for (const auto& pair : handle_mappings_) {
      handles.push_back(pair.first);
    }
  }

  Clock::time_point current_time = now_function_();
  Clock::duration remaining_timeout = timeout - (current_time - start_time);
  ErrorOr<std::vector<SocketHandleRef>> changed_handles =
      AwaitSocketsReadable(handles, remaining_timeout);

  std::vector<HandleWithSubscriber> ready_handles;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    handles_being_deleted_.clear();
    handle_deletion_block_.notify_all();
    if (changed_handles) {
      auto& ch = changed_handles.value();
      ready_handles.reserve(ch.size());
      for (const auto& handle : ch) {
        auto mapping_it = handle_mappings_.find(handle);
        if (mapping_it != handle_mappings_.end()) {
          ready_handles.push_back(
              HandleWithSubscriber{handle, mapping_it->second});
        }
      }
    }

    if (changed_handles.is_error()) {
      return changed_handles.error();
    }

    current_time = now_function_();
    remaining_timeout = timeout - (current_time - start_time);
    ProcessReadyHandles(ready_handles, remaining_timeout);
  }
  return Error::None();
}

}  // namespace openscreen
