// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/web_scheduler_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "platform/scheduler/child/web_task_runner_impl.h"
#include "platform/scheduler/child/worker_scheduler.h"
#include "public/platform/WebTraceLocation.h"
#include "public/platform/WebViewScheduler.h"

namespace blink {
namespace scheduler {

WebSchedulerImpl::WebSchedulerImpl(
    ChildScheduler* child_scheduler,
    scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner,
    scoped_refptr<TaskQueue> loading_task_runner,
    scoped_refptr<TaskQueue> timer_task_runner)
    : child_scheduler_(child_scheduler),
      idle_task_runner_(idle_task_runner),
      timer_task_runner_(timer_task_runner),
      loading_web_task_runner_(WebTaskRunnerImpl::Create(loading_task_runner)),
      timer_web_task_runner_(WebTaskRunnerImpl::Create(timer_task_runner)) {}

WebSchedulerImpl::~WebSchedulerImpl() {}

void WebSchedulerImpl::Shutdown() {
  child_scheduler_->Shutdown();
}

bool WebSchedulerImpl::ShouldYieldForHighPriorityWork() {
  return child_scheduler_->ShouldYieldForHighPriorityWork();
}

bool WebSchedulerImpl::CanExceedIdleDeadlineIfRequired() {
  return child_scheduler_->CanExceedIdleDeadlineIfRequired();
}

void WebSchedulerImpl::RunIdleTask(
    std::unique_ptr<blink::WebThread::IdleTask> task,
    base::TimeTicks deadline) {
  task->Run((deadline - base::TimeTicks()).InSecondsF());
}

void WebSchedulerImpl::PostIdleTask(const blink::WebTraceLocation& location,
                                    blink::WebThread::IdleTask* task) {
  DCHECK(idle_task_runner_);
  idle_task_runner_->PostIdleTask(
      location, base::Bind(&WebSchedulerImpl::RunIdleTask,
                           base::Passed(base::WrapUnique(task))));
}

void WebSchedulerImpl::PostNonNestableIdleTask(
    const blink::WebTraceLocation& location,
    blink::WebThread::IdleTask* task) {
  DCHECK(idle_task_runner_);
  idle_task_runner_->PostNonNestableIdleTask(
      location, base::Bind(&WebSchedulerImpl::RunIdleTask,
                           base::Passed(base::WrapUnique(task))));
}

blink::WebTaskRunner* WebSchedulerImpl::LoadingTaskRunner() {
  return loading_web_task_runner_.Get();
}

blink::WebTaskRunner* WebSchedulerImpl::TimerTaskRunner() {
  return timer_web_task_runner_.Get();
}

std::unique_ptr<blink::WebViewScheduler>
WebSchedulerImpl::CreateWebViewScheduler(
    InterventionReporter*,
    WebViewScheduler::WebViewSchedulerSettings*) {
  NOTREACHED();
  return nullptr;
}

}  // namespace scheduler
}  // namespace blink
