// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_WEB_TASK_RUNNER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_WEB_TASK_RUNNER_H_

#include <deque>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "platform/WebTaskRunner.h"

namespace blink {
namespace scheduler {

// A dummy WebTaskRunner for tests.
class FakeWebTaskRunner : public WebTaskRunner {
 public:
  FakeWebTaskRunner();

  void SetTime(base::TimeTicks new_time);
  void SetTime(double new_time) {
    SetTime(base::TimeTicks() + base::TimeDelta::FromSecondsD(new_time));
  }

  // base::SingleThreadTaskRunner implementation:
  bool RunsTasksInCurrentSequence() const override;

  // WebTaskRunner implementation:
  double MonotonicallyIncreasingVirtualTimeSeconds() const override;

  void RunUntilIdle();
  void AdvanceTimeAndRun(base::TimeDelta delta);
  void AdvanceTimeAndRun(double delta_seconds) {
    AdvanceTimeAndRun(base::TimeDelta::FromSecondsD(delta_seconds));
  }

  using PendingTask = std::pair<base::OnceClosure, base::TimeTicks>;
  std::deque<PendingTask> TakePendingTasksForTesting();

 protected:
  bool PostDelayedTask(const base::Location& location,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const base::Location&,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override;

 private:
  ~FakeWebTaskRunner() override;

  class Data;
  class BaseTaskRunner;
  scoped_refptr<Data> data_;

  explicit FakeWebTaskRunner(scoped_refptr<Data> data);

  DISALLOW_COPY_AND_ASSIGN(FakeWebTaskRunner);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_WEB_TASK_RUNNER_H_
