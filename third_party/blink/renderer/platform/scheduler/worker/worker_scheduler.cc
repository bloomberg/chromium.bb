// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/worker_scheduler.h"

#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/wake_up_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_proxy.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {
namespace scheduler {

WorkerScheduler::PauseHandle::PauseHandle(
    base::WeakPtr<WorkerScheduler> scheduler)
    : scheduler_(scheduler) {
  scheduler_->PauseImpl();
}

WorkerScheduler::PauseHandle::~PauseHandle() {
  if (scheduler_)
    scheduler_->ResumeImpl();
}

WorkerScheduler::WorkerScheduler(WorkerThreadScheduler* worker_thread_scheduler,
                                 WorkerSchedulerProxy* proxy)
    : throttleable_task_queue_(
          worker_thread_scheduler->CreateTaskQueue("worker_throttleable_tq")),
      pausable_task_queue_(
          worker_thread_scheduler->CreateTaskQueue("worker_pausable_tq")),
      unpausable_task_queue_(
          worker_thread_scheduler->CreateTaskQueue("worker_unpausable_tq")),
      thread_scheduler_(worker_thread_scheduler),
      weak_factory_(this) {
  thread_scheduler_->RegisterWorkerScheduler(this);

  SetUpThrottling();

  // |proxy| can be nullptr in unit tests.
  if (proxy)
    proxy->OnWorkerSchedulerCreated(GetWeakPtr());
}

base::WeakPtr<WorkerScheduler> WorkerScheduler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

WorkerScheduler::~WorkerScheduler() {
  DCHECK(is_disposed_);
  DCHECK_EQ(0u, paused_count_);
}

std::unique_ptr<WorkerScheduler::PauseHandle> WorkerScheduler::Pause() {
  thread_scheduler_->helper()->CheckOnValidThread();
  if (is_disposed_)
    return nullptr;
  return std::make_unique<PauseHandle>(GetWeakPtr());
}

void WorkerScheduler::PauseImpl() {
  thread_scheduler_->helper()->CheckOnValidThread();
  paused_count_++;
  if (paused_count_ == 1) {
    throttleable_task_queue_->SetPaused(true);
    pausable_task_queue_->SetPaused(true);
  }
}

void WorkerScheduler::ResumeImpl() {
  thread_scheduler_->helper()->CheckOnValidThread();
  paused_count_--;
  if (paused_count_ == 0 && !is_disposed_) {
    throttleable_task_queue_->SetPaused(false);
    pausable_task_queue_->SetPaused(false);
  }
}

void WorkerScheduler::SetUpThrottling() {
  if (!thread_scheduler_->task_queue_throttler())
    return;
  base::TimeTicks now = thread_scheduler_->GetTickClock()->NowTicks();

  WakeUpBudgetPool* wake_up_budget_pool =
      thread_scheduler_->wake_up_budget_pool();
  CPUTimeBudgetPool* cpu_time_budget_pool =
      thread_scheduler_->cpu_time_budget_pool();

  DCHECK(wake_up_budget_pool || cpu_time_budget_pool)
      << "At least one budget pool should be present";

  if (wake_up_budget_pool) {
    wake_up_budget_pool->AddQueue(now, throttleable_task_queue_.get());
  }
  if (cpu_time_budget_pool) {
    cpu_time_budget_pool->AddQueue(now, throttleable_task_queue_.get());
  }
}

std::unique_ptr<FrameOrWorkerScheduler::ActiveConnectionHandle>
WorkerScheduler::OnActiveConnectionCreated() {
  return nullptr;
}

SchedulingLifecycleState WorkerScheduler::CalculateLifecycleState(
    ObserverType) const {
  return thread_scheduler_->lifecycle_state();
}

void WorkerScheduler::Dispose() {
  if (TaskQueueThrottler* throttler =
          thread_scheduler_->task_queue_throttler()) {
    throttler->ShutdownTaskQueue(throttleable_task_queue_.get());
  }

  thread_scheduler_->UnregisterWorkerScheduler(this);

  unpausable_task_queue_->ShutdownTaskQueue();
  pausable_task_queue_->ShutdownTaskQueue();
  throttleable_task_queue_->ShutdownTaskQueue();

  is_disposed_ = true;
}

scoped_refptr<base::SingleThreadTaskRunner> WorkerScheduler::GetTaskRunner(
    TaskType type) const {
  switch (type) {
    case TaskType::kJavascriptTimer:
    case TaskType::kPostedMessage:
    case TaskType::kWorkerAnimation:
      return throttleable_task_queue_->CreateTaskRunner(type);
    case TaskType::kDOMManipulation:
    case TaskType::kUserInteraction:
    case TaskType::kNetworking:
    case TaskType::kNetworkingWithURLLoaderAnnotation:
    case TaskType::kNetworkingControl:
    case TaskType::kHistoryTraversal:
    case TaskType::kEmbed:
    case TaskType::kMediaElementEvent:
    case TaskType::kCanvasBlobSerialization:
    case TaskType::kMicrotask:
    case TaskType::kRemoteEvent:
    case TaskType::kWebSocket:
    case TaskType::kUnshippedPortMessage:
    case TaskType::kFileReading:
    case TaskType::kDatabaseAccess:
    case TaskType::kPresentation:
    case TaskType::kSensor:
    case TaskType::kPerformanceTimeline:
    case TaskType::kWebGL:
    case TaskType::kIdleTask:
    case TaskType::kMiscPlatformAPI:
    case TaskType::kFontLoading:
    case TaskType::kApplicationLifeCycle:
    case TaskType::kBackgroundFetch:
    case TaskType::kPermission:
    case TaskType::kInternalDefault:
    case TaskType::kInternalLoading:
    case TaskType::kInternalWebCrypto:
    case TaskType::kInternalIndexedDB:
    case TaskType::kInternalMedia:
    case TaskType::kInternalMediaRealTime:
    case TaskType::kInternalUserInteraction:
    case TaskType::kInternalIntersectionObserver:
      // UnthrottledTaskRunner is generally discouraged in future.
      // TODO(nhiroki): Identify which tasks can be throttled / suspendable and
      // move them into other task runners. See also comments in
      // Get(LocalFrame). (https://crbug.com/670534)
      return pausable_task_queue_->CreateTaskRunner(type);
    case TaskType::kDeprecatedNone:
    case TaskType::kInternalIPC:
    case TaskType::kInternalInspector:
    case TaskType::kInternalWorker:
    case TaskType::kInternalTest:
      // UnthrottledTaskRunner is generally discouraged in future.
      // TODO(nhiroki): Identify which tasks can be throttled / suspendable and
      // move them into other task runners. See also comments in
      // Get(LocalFrame). (https://crbug.com/670534)
      return unpausable_task_queue_->CreateTaskRunner(type);
    case TaskType::kMainThreadTaskQueueV8:
    case TaskType::kMainThreadTaskQueueCompositor:
    case TaskType::kMainThreadTaskQueueDefault:
    case TaskType::kMainThreadTaskQueueInput:
    case TaskType::kMainThreadTaskQueueIdle:
    case TaskType::kMainThreadTaskQueueIPC:
    case TaskType::kMainThreadTaskQueueControl:
    case TaskType::kMainThreadTaskQueueCleanup:
    case TaskType::kCompositorThreadTaskQueueDefault:
    case TaskType::kCompositorThreadTaskQueueInput:
    case TaskType::kWorkerThreadTaskQueueDefault:
    case TaskType::kWorkerThreadTaskQueueV8:
    case TaskType::kWorkerThreadTaskQueueCompositor:
    case TaskType::kExperimentalWebSchedulingUserInteraction:
    case TaskType::kExperimentalWebSchedulingBestEffort:
    case TaskType::kInternalTranslation:
    case TaskType::kCount:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return nullptr;
}

void WorkerScheduler::OnLifecycleStateChanged(
    SchedulingLifecycleState lifecycle_state) {
  if (lifecycle_state_ == lifecycle_state)
    return;
  lifecycle_state_ = lifecycle_state;
  thread_scheduler_->OnLifecycleStateChanged(lifecycle_state);

  if (TaskQueueThrottler* throttler =
          thread_scheduler_->task_queue_throttler()) {
    if (lifecycle_state_ == SchedulingLifecycleState::kThrottled) {
      throttler->IncreaseThrottleRefCount(throttleable_task_queue_.get());
    } else {
      throttler->DecreaseThrottleRefCount(throttleable_task_queue_.get());
    }
  }
  NotifyLifecycleObservers();
}

scoped_refptr<NonMainThreadTaskQueue> WorkerScheduler::UnpausableTaskQueue() {
  return unpausable_task_queue_.get();
}

scoped_refptr<NonMainThreadTaskQueue> WorkerScheduler::PausableTaskQueue() {
  return pausable_task_queue_.get();
}

scoped_refptr<NonMainThreadTaskQueue> WorkerScheduler::ThrottleableTaskQueue() {
  return throttleable_task_queue_.get();
}

}  // namespace scheduler
}  // namespace blink
