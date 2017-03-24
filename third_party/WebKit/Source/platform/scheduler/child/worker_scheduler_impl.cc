// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/worker_scheduler_impl.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "platform/scheduler/child/scheduler_tqm_delegate.h"
#include "public/platform/scheduler/base/task_queue.h"

namespace blink {
namespace scheduler {

WorkerSchedulerImpl::WorkerSchedulerImpl(
    scoped_refptr<SchedulerTqmDelegate> main_task_runner)
    : helper_(main_task_runner,
              "worker.scheduler",
              TRACE_DISABLED_BY_DEFAULT("worker.scheduler"),
              TRACE_DISABLED_BY_DEFAULT("worker.scheduler.debug")),
      idle_helper_(&helper_,
                   this,
                   "worker.scheduler",
                   TRACE_DISABLED_BY_DEFAULT("worker.scheduler"),
                   "WorkerSchedulerIdlePeriod",
                   base::TimeDelta::FromMilliseconds(300)),
      idle_canceled_delayed_task_sweeper_("worker.scheduler",
                                          &helper_,
                                          idle_helper_.IdleTaskRunner()) {
  initialized_ = false;
  thread_start_time_ = helper_.scheduler_tqm_delegate()->NowTicks();
  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler"), "WorkerScheduler", this);
}

WorkerSchedulerImpl::~WorkerSchedulerImpl() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler"), "WorkerScheduler", this);
}

void WorkerSchedulerImpl::Init() {
  initialized_ = true;
  idle_helper_.EnableLongIdlePeriod();
}

scoped_refptr<TaskQueue> WorkerSchedulerImpl::DefaultTaskRunner() {
  DCHECK(initialized_);
  return helper_.DefaultTaskRunner();
}

scoped_refptr<SingleThreadIdleTaskRunner>
WorkerSchedulerImpl::IdleTaskRunner() {
  DCHECK(initialized_);
  return idle_helper_.IdleTaskRunner();
}

bool WorkerSchedulerImpl::CanExceedIdleDeadlineIfRequired() const {
  DCHECK(initialized_);
  return idle_helper_.CanExceedIdleDeadlineIfRequired();
}

bool WorkerSchedulerImpl::ShouldYieldForHighPriorityWork() {
  // We don't consider any work as being high priority on workers.
  return false;
}

void WorkerSchedulerImpl::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK(initialized_);
  helper_.AddTaskObserver(task_observer);
}

void WorkerSchedulerImpl::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK(initialized_);
  helper_.RemoveTaskObserver(task_observer);
}

void WorkerSchedulerImpl::Shutdown() {
  DCHECK(initialized_);
  base::TimeTicks end_time = helper_.scheduler_tqm_delegate()->NowTicks();
  base::TimeDelta delta = thread_start_time_ - end_time;

  // The lifetime could be radically different for different workers,
  // some workers could be short-lived (but last at least 1 sec in
  // Service Workers case) or could be around as long as the tab is open.
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "WorkerThread.Runtime", delta, base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromDays(1), 50 /* bucket count */);
  helper_.Shutdown();
}

SchedulerHelper* WorkerSchedulerImpl::GetSchedulerHelperForTesting() {
  return &helper_;
}

bool WorkerSchedulerImpl::CanEnterLongIdlePeriod(base::TimeTicks,
                                                 base::TimeDelta*) {
  return true;
}

base::TimeTicks WorkerSchedulerImpl::CurrentIdleTaskDeadlineForTesting() const {
  return idle_helper_.CurrentIdleTaskDeadline();
}

}  // namespace scheduler
}  // namespace blink
