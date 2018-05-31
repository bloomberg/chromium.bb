// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_impl.h"

#include <utility>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/child/task_queue_with_task_type.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/wake_up_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {
namespace scheduler {

NonMainThreadSchedulerImpl::NonMainThreadSchedulerImpl(
    std::unique_ptr<NonMainThreadSchedulerHelper> helper)
    : helper_(std::move(helper)) {}

NonMainThreadSchedulerImpl::~NonMainThreadSchedulerImpl() {
  DCHECK(worker_schedulers_.empty());
}

// static
std::unique_ptr<NonMainThreadSchedulerImpl> NonMainThreadSchedulerImpl::Create(
    WebThreadType thread_type,
    WorkerSchedulerProxy* proxy) {
  return std::make_unique<WorkerThreadScheduler>(
      thread_type,
      base::sequence_manager::TaskQueueManager::TakeOverCurrentThread(), proxy);
}

void NonMainThreadSchedulerImpl::Init() {
  InitImpl();
}

scoped_refptr<WorkerTaskQueue> NonMainThreadSchedulerImpl::CreateTaskRunner() {
  helper_->CheckOnValidThread();
  return helper_->NewTaskQueue(
      base::sequence_manager::TaskQueue::Spec("worker_tq")
          .SetShouldMonitorQuiescence(true)
          .SetTimeDomain(nullptr));
}

void NonMainThreadSchedulerImpl::RunIdleTask(blink::WebThread::IdleTask task,
                                             base::TimeTicks deadline) {
  std::move(task).Run((deadline - base::TimeTicks()).InSecondsF());
}

void NonMainThreadSchedulerImpl::PostIdleTask(const base::Location& location,
                                              blink::WebThread::IdleTask task) {
  IdleTaskRunner()->PostIdleTask(
      location, base::BindOnce(&NonMainThreadSchedulerImpl::RunIdleTask,
                               std::move(task)));
}

void NonMainThreadSchedulerImpl::PostNonNestableIdleTask(
    const base::Location& location,
    blink::WebThread::IdleTask task) {
  IdleTaskRunner()->PostNonNestableIdleTask(
      location, base::BindOnce(&NonMainThreadSchedulerImpl::RunIdleTask,
                               std::move(task)));
}

std::unique_ptr<blink::PageScheduler>
NonMainThreadSchedulerImpl::CreatePageScheduler(
    PageScheduler::Delegate* delegate) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<NonMainThreadSchedulerImpl::RendererPauseHandle>
NonMainThreadSchedulerImpl::PauseScheduler() {
  return nullptr;
}

base::TimeTicks
NonMainThreadSchedulerImpl::MonotonicallyIncreasingVirtualTime() {
  return base::TimeTicks::Now();
}

void NonMainThreadSchedulerImpl::RegisterWorkerScheduler(
    WorkerScheduler* worker_scheduler) {
  worker_schedulers_.insert(worker_scheduler);
}

void NonMainThreadSchedulerImpl::UnregisterWorkerScheduler(
    WorkerScheduler* worker_scheduler) {
  DCHECK(worker_schedulers_.find(worker_scheduler) != worker_schedulers_.end());
  worker_schedulers_.erase(worker_scheduler);
}

scoped_refptr<base::SingleThreadTaskRunner>
NonMainThreadSchedulerImpl::ControlTaskRunner() {
  return helper_->ControlWorkerTaskQueue();
}

void NonMainThreadSchedulerImpl::RegisterTimeDomain(
    base::sequence_manager::TimeDomain* time_domain) {
  return helper_->RegisterTimeDomain(time_domain);
}

void NonMainThreadSchedulerImpl::UnregisterTimeDomain(
    base::sequence_manager::TimeDomain* time_domain) {
  return helper_->UnregisterTimeDomain(time_domain);
}

base::sequence_manager::TimeDomain*
NonMainThreadSchedulerImpl::GetActiveTimeDomain() {
  return helper_->real_time_domain();
}

const base::TickClock* NonMainThreadSchedulerImpl::GetTickClock() {
  return helper_->GetClock();
}

}  // namespace scheduler
}  // namespace blink
