// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_NON_MAIN_THREAD_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_NON_MAIN_THREAD_SCHEDULER_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/public/platform/scheduler/single_thread_idle_task_runner.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_thread_type.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/base/task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/child/worker_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_helper.h"

namespace blink {
namespace scheduler {
class TaskQueueWithTaskType;
class WorkerSchedulerProxy;
class WorkerScheduler;

// TODO(yutak): Remove the dependency to WebThreadScheduler. We want to
// separate interfaces to Chromium (in blink/public/platform/scheduler) from
// interfaces to Blink (in blink/renderer/platform/scheduler/public).
class PLATFORM_EXPORT NonMainThreadScheduler : public WebThreadScheduler,
                                               public ThreadScheduler {
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
  void Init();

  virtual void OnTaskCompleted(
      WorkerTaskQueue* worker_task_queue,
      const base::sequence_manager::TaskQueue::Task& task,
      base::TimeTicks start,
      base::TimeTicks end,
      base::Optional<base::TimeDelta> thread_time) = 0;

  // ThreadScheduler implementation.
  // TODO(yutak): Some functions are only meaningful in main thread. Move them
  // to MainThreadScheduler.
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

  // The following virtual methods are defined in *both* WebThreadScheduler
  // and ThreadScheduler, with identical interfaces and semantics. They are
  // overriden in a subclass, effectively implementing the virtual methods
  // in both classes at the same time. This is allowed in C++, as long as
  // there is only one final overrider (i.e. definitions in base classes are
  // not used in instantiated objects, since otherwise they may have multiple
  // definitions of the virtual function in question).
  //
  // virtual void Shutdown();
  // virtual bool ShouldYieldForHighPriorityWork();
  // virtual bool CanExceedIdleDeadlineIfRequired() const;

  scoped_refptr<WorkerTaskQueue> CreateTaskRunner();

 protected:
  explicit NonMainThreadScheduler(
      std::unique_ptr<NonMainThreadSchedulerHelper> helper);

  // Called during Init() for delayed initialization for subclasses.
  virtual void InitImpl() = 0;

  std::unique_ptr<NonMainThreadSchedulerHelper> helper_;

  // Worker schedulers associated with this thread.
  std::unordered_set<WorkerScheduler*> worker_schedulers_;

 private:
  friend class WorkerScheduler;

  // Each WorkerScheduler should notify NonMainThreadScheduler when it is
  // created or destroyed.
  void RegisterWorkerScheduler(WorkerScheduler* worker_scheduler);
  void UnregisterWorkerScheduler(WorkerScheduler* worker_scheduler);

  static void RunIdleTask(WebThread::IdleTask task, base::TimeTicks deadline);
  scoped_refptr<TaskQueueWithTaskType> v8_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(NonMainThreadScheduler);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_NON_MAIN_THREAD_SCHEDULER_H_
