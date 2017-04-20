// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/worker_scheduler.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "platform/scheduler/child/scheduler_tqm_delegate.h"
#include "platform/scheduler/child/worker_scheduler_impl.h"

namespace blink {
namespace scheduler {

WorkerScheduler::WorkerScheduler(std::unique_ptr<SchedulerHelper> helper)
    : helper_(std::move(helper)) {}

WorkerScheduler::~WorkerScheduler() {}

// static
std::unique_ptr<WorkerScheduler> WorkerScheduler::Create(
    scoped_refptr<SchedulerTqmDelegate> main_task_runner) {
  return base::WrapUnique(new WorkerSchedulerImpl(std::move(main_task_runner)));
}

scoped_refptr<TaskQueue> WorkerScheduler::CreateUnthrottledTaskRunner(
    TaskQueue::QueueType queue_type) {
  helper_->CheckOnValidThread();
  scoped_refptr<TaskQueue> unthrottled_task_queue(
      helper_->NewTaskQueue(TaskQueue::Spec(queue_type)
                                .SetShouldMonitorQuiescence(true)
                                .SetTimeDomain(nullptr)));
  return unthrottled_task_queue;
}

}  // namespace scheduler
}  // namespace blink
