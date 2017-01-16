// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_IDLE_CANCELED_DELAYED_TASK_SWEEPER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_IDLE_CANCELED_DELAYED_TASK_SWEEPER_H_

#include "base/macros.h"
#include "platform/scheduler/child/scheduler_helper.h"
#include "public/platform/scheduler/child/single_thread_idle_task_runner.h"

namespace blink {
namespace scheduler {

// This class periodically sweeps away canceled delayed tasks, which helps
// reduce memory consumption.
class BLINK_PLATFORM_EXPORT IdleCanceledDelayedTaskSweeper {
 public:
  IdleCanceledDelayedTaskSweeper(
      const char* tracing_category,
      SchedulerHelper* scheduler_helper,
      scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner);

 private:
  void PostIdleTask();
  void SweepIdleTask(base::TimeTicks deadline);

  const char* tracing_category_;       // NOT OWNED
  SchedulerHelper* scheduler_helper_;  // NOT OWNED
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
  base::WeakPtrFactory<IdleCanceledDelayedTaskSweeper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IdleCanceledDelayedTaskSweeper);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_IDLE_CANCELED_DELAYED_TASK_SWEEPER_H_
