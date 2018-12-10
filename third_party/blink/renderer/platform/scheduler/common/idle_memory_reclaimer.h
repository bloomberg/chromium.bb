// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_IDLE_MEMORY_RECLAIMER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_IDLE_MEMORY_RECLAIMER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/platform/scheduler/common/scheduler_helper.h"
#include "third_party/blink/renderer/platform/scheduler/common/single_thread_idle_task_runner.h"

namespace blink {
namespace scheduler {

// This class periodically sweeps away canceled delayed tasks and considers
// resizing to fit all internal queues, which helps reduce memory consumption.
class PLATFORM_EXPORT IdleMemoryReclaimer {
 public:
  IdleMemoryReclaimer(
      SchedulerHelper* scheduler_helper,
      scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner);

 private:
  void PostIdleTask();
  void IdleTask(base::TimeTicks deadline);

  SchedulerHelper* scheduler_helper_;  // NOT OWNED
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
  base::WeakPtrFactory<IdleMemoryReclaimer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IdleMemoryReclaimer);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_IDLE_MEMORY_RECLAIMER_H_
