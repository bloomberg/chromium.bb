// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/test/test_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

void RunLoopUntil(base::RepeatingCallback<bool()> criteria) {
  if (criteria.Run())
    return;
  constexpr int kMaxIterations = 1000;
  int iteration = 0;
  base::RunLoop run_loop;
  base::RepeatingClosure check_conditions_callback;
  auto schedule_check = [&]() {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     check_conditions_callback);
  };
  check_conditions_callback = base::BindLambdaForTesting([&]() {
    if (criteria.Run() || ++iteration > kMaxIterations) {
      run_loop.QuitClosure().Run();
      ASSERT_LE(iteration, kMaxIterations)
          << "RunLoopUntil criteria still not true after max iteration count";
    } else {
      schedule_check();
    }
  });
  schedule_check();
  run_loop.Run();
}

}  // namespace feed
