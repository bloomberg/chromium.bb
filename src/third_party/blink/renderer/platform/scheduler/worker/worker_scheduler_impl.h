// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_WORKER_SCHEDULER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_WORKER_SCHEDULER_IMPL_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/common/back_forward_cache_disabling_feature_tracker.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
namespace scheduler {

class NonMainThreadTaskQueue;
class WorkerSchedulerProxy;
class WorkerThreadScheduler;

// A scheduler provides per-global-scope task queues. This is constructed when a
// global scope is created and destructed when it's closed.
//
// Unless stated otherwise, all methods must be called on the worker thread.
class PLATFORM_EXPORT WorkerSchedulerImpl : public WorkerScheduler {
 public:
  WorkerSchedulerImpl(WorkerThreadScheduler* worker_thread_scheduler,
                      WorkerSchedulerProxy* proxy);
  ~WorkerSchedulerImpl() override;

  class PauseHandleImpl : public PauseHandle {
    USING_FAST_MALLOC(PauseHandleImpl);

   public:
    explicit PauseHandleImpl(base::WeakPtr<WorkerSchedulerImpl>);
    ~PauseHandleImpl() override;

   private:
    base::WeakPtr<WorkerSchedulerImpl> scheduler_;
  };

  // WorkerScheduler implementation:
  void Dispose() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      TaskType) const override;
  WorkerThreadScheduler* GetWorkerThreadScheduler() const override;
  void OnLifecycleStateChanged(
      SchedulingLifecycleState lifecycle_state) override;
  std::unique_ptr<PauseHandle> Pause() override;
  void InitializeOnWorkerThread(Delegate* delegate) override;

  // FrameOrWorkerScheduler implementation:
  SchedulingLifecycleState CalculateLifecycleState(ObserverType) const override;
  void OnStartedUsingFeature(SchedulingPolicy::Feature feature,
                             const SchedulingPolicy& policy) override;
  void OnStoppedUsingFeature(SchedulingPolicy::Feature feature,
                             const SchedulingPolicy& policy) override;
  base::WeakPtr<FrameOrWorkerScheduler> GetSchedulingAffectingFeatureWeakPtr()
      override;
  void SetPreemptedForCooperativeScheduling(Preempted) override {}
  std::unique_ptr<WebSchedulingTaskQueue> CreateWebSchedulingTaskQueue(
      WebSchedulingPriority) override;

 protected:
  scoped_refptr<NonMainThreadTaskQueue> ThrottleableTaskQueue();
  scoped_refptr<NonMainThreadTaskQueue> UnpausableTaskQueue();
  scoped_refptr<NonMainThreadTaskQueue> PausableTaskQueue();

 private:
  void SetUpThrottling();
  void PauseImpl();
  void ResumeImpl();

  base::WeakPtr<WorkerSchedulerImpl> GetWeakPtr();

  TraceableVariableController tracing_controller_;

  // The tasks runners below are listed in increasing QoS order.
  // - throttleable task queue. Designed for custom user-provided javascript
  //   tasks. Lowest guarantees. Can be paused, blocked during user gesture,
  //   throttled when backgrounded or stopped completely after some time in
  //   background.
  // - pausable task queue. Default queue for high-priority javascript tasks.
  //   They can be paused according to the spec during devtools debugging.
  //   Otherwise scheduler does not tamper with their execution.
  // - unpausable task queue. Should be used for control tasks which should
  //   run when the context is paused. Usage should be extremely rare.
  //   Please consult scheduler-dev@ before using it. Running javascript
  //   on it is strictly verboten and can lead to hard-to-diagnose errors.
  scoped_refptr<NonMainThreadTaskQueue> throttleable_task_queue_;
  scoped_refptr<NonMainThreadTaskQueue> pausable_task_queue_;
  scoped_refptr<NonMainThreadTaskQueue> unpausable_task_queue_;

  using TaskQueueVoterMap = std::map<
      scoped_refptr<NonMainThreadTaskQueue>,
      std::unique_ptr<base::sequence_manager::TaskQueue::QueueEnabledVoter>>;

  TaskQueueVoterMap task_runners_;

  SchedulingLifecycleState lifecycle_state_ =
      SchedulingLifecycleState::kNotThrottled;

  WorkerThreadScheduler* thread_scheduler_;  // NOT OWNED

  bool is_disposed_ = false;
  uint32_t paused_count_ = 0;

  BackForwardCacheDisablingFeatureTracker
      back_forward_cache_disabling_feature_tracker_;

  base::WeakPtrFactory<WorkerSchedulerImpl> weak_factory_{this};
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_WORKER_SCHEDULER_IMPL_H_
