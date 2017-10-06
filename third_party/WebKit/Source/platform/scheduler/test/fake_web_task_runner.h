// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_WEB_TASK_RUNNER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_WEB_TASK_RUNNER_H_

#include <deque>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "platform/WebTaskRunner.h"
#include "platform/wtf/RefPtr.h"

namespace blink {
namespace scheduler {

// A dummy WebTaskRunner for tests.
class FakeWebTaskRunner : public WebTaskRunner {
 public:
  FakeWebTaskRunner();

  void SetTime(double new_time);

  // WebTaskRunner implementation:
  bool RunsTasksInCurrentSequence() override;
  double VirtualTimeSeconds() const override;
  double MonotonicallyIncreasingVirtualTimeSeconds() const override;
  SingleThreadTaskRunnerRefPtr ToSingleThreadTaskRunner() override;

  void RunUntilIdle();
  void AdvanceTimeAndRun(double delta_seconds);
  std::deque<std::pair<base::OnceClosure, double>> TakePendingTasksForTesting();

 protected:
  bool PostDelayedTask(const base::Location& location,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;

 private:
  ~FakeWebTaskRunner() override;

  class Data;
  class BaseTaskRunner;
  RefPtr<Data> data_;
  scoped_refptr<BaseTaskRunner> base_task_runner_;

  FakeWebTaskRunner(RefPtr<Data> data,
                    scoped_refptr<BaseTaskRunner> base_task_runner);

  DISALLOW_COPY_AND_ASSIGN(FakeWebTaskRunner);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_WEB_TASK_RUNNER_H_
