// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_NON_MAIN_THREAD_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_NON_MAIN_THREAD_SCHEDULER_H_

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "third_party/blink/public/platform/scheduler/single_thread_idle_task_runner.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_thread_type.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/base/task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_helper.h"
#include "third_party/blink/renderer/platform/scheduler/child/worker_task_queue.h"

namespace blink {
namespace scheduler {
class WorkerSchedulerProxy;

class PLATFORM_EXPORT NonMainThreadScheduler : public WebThreadScheduler {
 public:
  ~NonMainThreadScheduler() override;

  static std::unique_ptr<NonMainThreadScheduler> Create(
      WebThreadType thread_type,
      WorkerSchedulerProxy* proxy);

  // Blink should use NonMainThreadScheduler::DefaultTaskQueue instead of
  // WebThreadScheduler::DefaultTaskRunner.
  virtual scoped_refptr<WorkerTaskQueue> DefaultTaskQueue() = 0;

  // Must be called before the scheduler can be used. Does any post construction
  // initialization needed such as initializing idle period detection.
  virtual void Init() = 0;

  virtual void OnTaskCompleted(WorkerTaskQueue* worker_task_queue,
                               const TaskQueue::Task& task,
                               base::TimeTicks start,
                               base::TimeTicks end,
                               base::Optional<base::TimeDelta> thread_time) = 0;

  scoped_refptr<WorkerTaskQueue> CreateTaskRunner();

 protected:
  explicit NonMainThreadScheduler(
      std::unique_ptr<WorkerSchedulerHelper> helper);

  std::unique_ptr<WorkerSchedulerHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(NonMainThreadScheduler);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_NON_MAIN_THREAD_SCHEDULER_H_
