// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/child/web_scheduler_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/child/task_queue_with_task_type.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"

namespace blink {
namespace scheduler {

WebSchedulerImpl::WebSchedulerImpl(
    WebThreadScheduler* thread_scheduler,
    scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner,
    scoped_refptr<base::sequence_manager::TaskQueue> v8_task_runner)
    : thread_scheduler_(thread_scheduler),
      idle_task_runner_(idle_task_runner),
      v8_task_runner_(
          TaskQueueWithTaskType::Create(std::move(v8_task_runner),
                                        TaskType::kMainThreadTaskQueueV8)) {}

WebSchedulerImpl::~WebSchedulerImpl() = default;

void WebSchedulerImpl::Shutdown() {
  thread_scheduler_->Shutdown();
}

bool WebSchedulerImpl::ShouldYieldForHighPriorityWork() {
  return thread_scheduler_->ShouldYieldForHighPriorityWork();
}

bool WebSchedulerImpl::CanExceedIdleDeadlineIfRequired() const {
  return thread_scheduler_->CanExceedIdleDeadlineIfRequired();
}

void WebSchedulerImpl::RunIdleTask(blink::WebThread::IdleTask task,
                                   base::TimeTicks deadline) {
  std::move(task).Run((deadline - base::TimeTicks()).InSecondsF());
}

void WebSchedulerImpl::PostIdleTask(const base::Location& location,
                                    blink::WebThread::IdleTask task) {
  DCHECK(idle_task_runner_);
  idle_task_runner_->PostIdleTask(
      location,
      base::BindOnce(&WebSchedulerImpl::RunIdleTask, std::move(task)));
}

void WebSchedulerImpl::PostNonNestableIdleTask(
    const base::Location& location,
    blink::WebThread::IdleTask task) {
  DCHECK(idle_task_runner_);
  idle_task_runner_->PostNonNestableIdleTask(
      location,
      base::BindOnce(&WebSchedulerImpl::RunIdleTask, std::move(task)));
}

scoped_refptr<base::SingleThreadTaskRunner> WebSchedulerImpl::V8TaskRunner() {
  return v8_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
WebSchedulerImpl::CompositorTaskRunner() {
  return nullptr;
}

std::unique_ptr<blink::PageScheduler> WebSchedulerImpl::CreatePageScheduler(
    PageScheduler::Delegate* delegate) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<WebSchedulerImpl::RendererPauseHandle>
WebSchedulerImpl::PauseScheduler() {
  return nullptr;
}

base::TimeTicks WebSchedulerImpl::MonotonicallyIncreasingVirtualTime() const {
  return base::TimeTicks::Now();
}

}  // namespace scheduler
}  // namespace blink
