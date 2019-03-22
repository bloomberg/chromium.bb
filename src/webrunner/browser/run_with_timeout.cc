// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_COMMON_TEST_RUN_WITH_TIMEOUT_H_
#define WEBRUNNER_COMMON_TEST_RUN_WITH_TIMEOUT_H_

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webrunner {

void CheckRunWithTimeout(base::RunLoop* run_loop,
                         const base::TimeDelta& timeout) {
  bool did_timeout = false;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](bool* did_timeout, base::OnceClosure quit_closure) {
            *did_timeout = true;
            std::move(quit_closure).Run();
          },
          base::Unretained(&did_timeout), run_loop->QuitClosure()),
      timeout);
  run_loop->Run();
  if (did_timeout)
    ADD_FAILURE() << "Timeout: RunLoop did not Quit() within "
                  << timeout.InSecondsF() << " seconds.";
}

}  // namespace webrunner

#endif  // WEBRUNNER_COMMON_RUNLOOP_WITH_DEADLINE_H_
