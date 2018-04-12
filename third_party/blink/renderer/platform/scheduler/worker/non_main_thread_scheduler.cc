// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread_scheduler.h"

#include <utility>

#include "base/message_loop/message_loop.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {
namespace scheduler {

NonMainThreadScheduler::NonMainThreadScheduler(
    std::unique_ptr<WorkerSchedulerHelper> helper)
    : helper_(std::move(helper)) {}

NonMainThreadScheduler::~NonMainThreadScheduler() = default;

// static
std::unique_ptr<NonMainThreadScheduler> NonMainThreadScheduler::Create(
    WebThreadType thread_type,
    WorkerSchedulerProxy* proxy) {
  return std::make_unique<WorkerThreadScheduler>(
      thread_type, TaskQueueManager::TakeOverCurrentThread(), proxy);
}

scoped_refptr<WorkerTaskQueue> NonMainThreadScheduler::CreateTaskRunner() {
  helper_->CheckOnValidThread();
  return helper_->NewTaskQueue(TaskQueue::Spec("worker_tq")
                                   .SetShouldMonitorQuiescence(true)
                                   .SetTimeDomain(nullptr));
}

}  // namespace scheduler
}  // namespace blink
