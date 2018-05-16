// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_CHILD_WEB_SCHEDULER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_CHILD_WEB_SCHEDULER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/web_thread.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/base/task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {
namespace scheduler {

class SingleThreadIdleTaskRunner;
class TaskQueueWithTaskType;
class WebThreadScheduler;

class PLATFORM_EXPORT WebSchedulerImpl : public ThreadScheduler {
 public:
  WebSchedulerImpl(
      WebThreadScheduler* thread_scheduler,
      scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner,
      scoped_refptr<base::sequence_manager::TaskQueue> v8_task_runner);
  ~WebSchedulerImpl() override;

  // ThreadScheduler implementation:
  void Shutdown() override;
  bool ShouldYieldForHighPriorityWork() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  void PostIdleTask(const base::Location& location,
                    WebThread::IdleTask task) override;
  void PostNonNestableIdleTask(const base::Location& location,
                               WebThread::IdleTask task) override;
  scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  std::unique_ptr<PageScheduler> CreatePageScheduler(
      PageScheduler::Delegate*) override;
  std::unique_ptr<RendererPauseHandle> PauseScheduler() override
      WARN_UNUSED_RESULT;

  // Returns TimeTicks::Now() by default.
  base::TimeTicks MonotonicallyIncreasingVirtualTime() const override;

 private:
  static void RunIdleTask(WebThread::IdleTask task, base::TimeTicks deadline);

  WebThreadScheduler* thread_scheduler_;  // NOT OWNED
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
  scoped_refptr<TaskQueueWithTaskType> v8_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebSchedulerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_CHILD_WEB_SCHEDULER_IMPL_H_
