// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/web_scheduler_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "platform/scheduler/child/web_task_runner_impl.h"
#include "platform/scheduler/child/worker_scheduler.h"
#include "platform/scheduler/renderer/web_view_scheduler.h"

namespace blink {
namespace scheduler {

WebSchedulerImpl::WebSchedulerImpl(
    ChildScheduler* child_scheduler,
    scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner,
    scoped_refptr<TaskQueue> timer_task_runner,
    scoped_refptr<TaskQueue> v8_task_runner)
    : child_scheduler_(child_scheduler),
      idle_task_runner_(idle_task_runner),
      timer_web_task_runner_(
          WebTaskRunnerImpl::Create(timer_task_runner, base::nullopt)),
      v8_web_task_runner_(
          WebTaskRunnerImpl::Create(v8_task_runner, base::nullopt)) {}

WebSchedulerImpl::~WebSchedulerImpl() = default;

void WebSchedulerImpl::Shutdown() {
  child_scheduler_->Shutdown();
}

bool WebSchedulerImpl::ShouldYieldForHighPriorityWork() {
  return child_scheduler_->ShouldYieldForHighPriorityWork();
}

bool WebSchedulerImpl::CanExceedIdleDeadlineIfRequired() {
  return child_scheduler_->CanExceedIdleDeadlineIfRequired();
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

blink::WebTaskRunner* WebSchedulerImpl::TimerTaskRunner() {
  return timer_web_task_runner_.get();
}

blink::WebTaskRunner* WebSchedulerImpl::V8TaskRunner() {
  return v8_web_task_runner_.get();
}

blink::WebTaskRunner* WebSchedulerImpl::CompositorTaskRunner() {
  return nullptr;
}

std::unique_ptr<blink::WebViewScheduler>
WebSchedulerImpl::CreateWebViewScheduler(
    InterventionReporter*,
    WebViewScheduler::WebViewSchedulerDelegate*) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<WebSchedulerImpl::RendererPauseHandle>
WebSchedulerImpl::PauseScheduler() {
  return nullptr;
}

}  // namespace scheduler
}  // namespace blink
