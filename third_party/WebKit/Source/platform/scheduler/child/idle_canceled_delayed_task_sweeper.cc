// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/idle_canceled_delayed_task_sweeper.h"

#include "base/bind.h"

namespace blink {
namespace scheduler {

namespace {
const int kDelayedTaskSweepIntervalSeconds = 30;
}

IdleCanceledDelayedTaskSweeper::IdleCanceledDelayedTaskSweeper(
    const char* tracing_category,
    SchedulerHelper* scheduler_helper,
    scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner)
    : tracing_category_(tracing_category),
      scheduler_helper_(scheduler_helper),
      idle_task_runner_(idle_task_runner),
      weak_factory_(this) {
  PostIdleTask();
}

void IdleCanceledDelayedTaskSweeper::PostIdleTask() {
  idle_task_runner_->PostDelayedIdleTask(
      FROM_HERE, base::TimeDelta::FromSeconds(kDelayedTaskSweepIntervalSeconds),
      base::Bind(&IdleCanceledDelayedTaskSweeper::SweepIdleTask,
                 weak_factory_.GetWeakPtr()));
}

void IdleCanceledDelayedTaskSweeper::SweepIdleTask(base::TimeTicks deadline) {
  TRACE_EVENT0(tracing_category_,
               "IdleCanceledDelayedTaskSweeper::SweepIdleTask");
  scheduler_helper_->SweepCanceledDelayedTasks();
  PostIdleTask();
}

}  // namespace scheduler
}  // namespace blink
