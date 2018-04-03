// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_DELEGATE_FOR_TEST_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_DELEGATE_FOR_TEST_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "platform/scheduler/base/task_queue_manager_delegate.h"

namespace blink {
namespace scheduler {

class TaskQueueManagerDelegateForTest : public TaskQueueManagerDelegate {
 public:
  static scoped_refptr<TaskQueueManagerDelegateForTest> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::TickClock* time_source);

  // SingleThreadTaskRunner:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override;
  bool RunsTasksInCurrentSequence() const override;

  // TaskQueueManagerDelegate:
  bool IsNested() const override;
  void AddNestingObserver(base::RunLoop::NestingObserver* observer) override;
  void RemoveNestingObserver(base::RunLoop::NestingObserver* observer) override;

  // TickClock:
  base::TimeTicks NowTicks() const override;

 protected:
  ~TaskQueueManagerDelegateForTest() override;
  TaskQueueManagerDelegateForTest(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::TickClock* time_source);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const base::TickClock* time_source_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueManagerDelegateForTest);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_DELEGATE_FOR_TEST_H_
