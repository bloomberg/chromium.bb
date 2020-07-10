// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "util/operation_loop.h"

#include <algorithm>

#include "util/logging.h"

namespace openscreen {

OperationLoop::OperationLoop(std::vector<OperationWithTimeout> operations,
                             Clock::duration timeout,
                             Clock::duration min_loop_execution_time)
    : perform_all_operations_min_execution_time_(min_loop_execution_time),
      operation_timeout_(timeout),
      operations_(operations) {
  OSP_DCHECK(operations_.size());
}

void OperationLoop::RunUntilStopped() {
  OSP_CHECK(!is_running_.exchange(true));

  while (is_running_.load()) {
    PerformAllOperations();
  }
}

void OperationLoop::RequestStopSoon() {
  {
    std::unique_lock<std::mutex> lock(wait_mutex_);
    is_running_.store(false);
  }

  perform_all_operations_waiter_.notify_all();
}

void OperationLoop::PerformAllOperations() {
  auto start_time = Clock::now();

  for (OperationWithTimeout operation : operations_) {
    if (is_running_.load()) {
      operation(operation_timeout_);
    }
  }

  const auto duration = Clock::now() - start_time;
  const auto remaining_duration =
      perform_all_operations_min_execution_time_ - duration;
  if (remaining_duration > Clock::duration{0} && is_running_.load()) {
    std::unique_lock<std::mutex> lock(wait_mutex_);
    perform_all_operations_waiter_.wait_for(
        lock, remaining_duration, [this]() { return !is_running_.load(); });
  }
}

}  // namespace openscreen
