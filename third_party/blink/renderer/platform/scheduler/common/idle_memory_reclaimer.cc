// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/idle_memory_reclaimer.h"

#include "base/bind.h"

namespace blink {
namespace scheduler {

namespace {
constexpr const int kReclaimMemoryIntervalSeconds = 30;
}

IdleMemoryReclaimer::IdleMemoryReclaimer(
    SchedulerHelper* scheduler_helper,
    scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner)
    : scheduler_helper_(scheduler_helper),
      idle_task_runner_(idle_task_runner),
      weak_factory_(this) {
  PostIdleTask();
}

void IdleMemoryReclaimer::PostIdleTask() {
  idle_task_runner_->PostDelayedIdleTask(
      FROM_HERE, base::TimeDelta::FromSeconds(kReclaimMemoryIntervalSeconds),
      base::BindOnce(&IdleMemoryReclaimer::IdleTask,
                     weak_factory_.GetWeakPtr()));
}

void IdleMemoryReclaimer::IdleTask(base::TimeTicks deadline) {
  TRACE_EVENT0("renderer.scheduler", "IdleMemoryReclaimer::IdleTask");
  scheduler_helper_->ReclaimMemory();
  PostIdleTask();
}

}  // namespace scheduler
}  // namespace blink
