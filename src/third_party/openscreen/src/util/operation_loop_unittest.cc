// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/operation_loop.h"

#include <atomic>
#include <chrono>
#include <thread>

#include "gtest/gtest.h"

namespace openscreen {

TEST(OperationsLoopTest, PerformAllOperationsWaits) {
  constexpr Clock::duration kTimeout{0};
  constexpr Clock::duration kMinRuntime{500};

  const auto start_time = Clock::now();
  std::atomic<Clock::time_point> last_run{start_time};
  std::atomic<Clock::time_point> current_run{start_time};
  std::function<void(Clock::duration)> test_function =
      [last = &last_run, current = &current_run](Clock::duration timeout) {
        last->store(current->load());
        current->store(Clock::now());
      };
  OperationLoop loop({test_function}, kTimeout, kMinRuntime);

  std::thread run_loop([&loop]() { loop.RunUntilStopped(); });

  while (last_run.load() == start_time) {
  }

  loop.RequestStopSoon();
  run_loop.join();

  EXPECT_GE(
      std::chrono::nanoseconds(current_run.load() - last_run.load()).count(),
      std::chrono::nanoseconds(kMinRuntime).count());
}

}  // namespace openscreen
