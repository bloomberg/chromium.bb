// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/worker_scheduler_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "platform/Histogram.h"
#include "platform/scheduler/base/task_queue.h"
#include "platform/scheduler/base/task_queue_manager.h"
#include "platform/scheduler/child/default_params.h"
#include "platform/scheduler/child/worker_scheduler_helper.h"
#include "platform/scheduler/child/worker_scheduler_proxy.h"

namespace blink {
namespace scheduler {

namespace {
// Workers could be short-lived, set a shorter interval than
// the renderer thread.
constexpr base::TimeDelta kUnspecifiedWorkerThreadLoadTrackerReportingInterval =
    base::TimeDelta::FromSeconds(1);

void ReportWorkerTaskLoad(base::TimeTicks time, double load) {
  int load_percentage = static_cast<int>(load * 100);
  DCHECK_LE(load_percentage, 100);
  // TODO(kinuko): Maybe we also want to separately log when the associated
  // tab is in foreground and when not.
  UMA_HISTOGRAM_PERCENTAGE("WorkerScheduler.WorkerThreadLoad", load_percentage);
}

// TODO(scheduler-dev): Remove conversions when Blink starts using
// base::TimeTicks instead of doubles for time.
base::TimeTicks MonotonicTimeInSecondsToTimeTicks(
    double monotonic_time_in_seconds) {
  return base::TimeTicks() +
         base::TimeDelta::FromSecondsD(monotonic_time_in_seconds);
}

}  // namespace

WorkerSchedulerImpl::WorkerSchedulerImpl(
    std::unique_ptr<TaskQueueManager> task_queue_manager,
    WorkerSchedulerProxy* proxy)
    : WorkerScheduler(
          std::make_unique<WorkerSchedulerHelper>(std::move(task_queue_manager),
                                                  this)),
      idle_helper_(helper_.get(),
                   this,
                   "WorkerSchedulerIdlePeriod",
                   base::TimeDelta::FromMilliseconds(300),
                   helper_->NewTaskQueue(TaskQueue::Spec("worker_idle_tq"))),
      idle_canceled_delayed_task_sweeper_(helper_.get(),
                                          idle_helper_.IdleTaskRunner()),
      load_tracker_(helper_->NowTicks(),
                    base::BindRepeating(&ReportWorkerTaskLoad),
                    kUnspecifiedWorkerThreadLoadTrackerReportingInterval),
      throttling_state_(
          proxy ? proxy->throttling_state()
                : WebFrameScheduler::ThrottlingState::kNotThrottled),
      weak_factory_(this) {
  thread_start_time_ = helper_->NowTicks();
  load_tracker_.Resume(thread_start_time_);
  helper_->AddTaskTimeObserver(this);

  if (proxy) {
    worker_metrics_helper_.SetParentFrameType(proxy->parent_frame_type());
    proxy->OnWorkerSchedulerCreated(GetWeakPtr());
  }

  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler"), "WorkerScheduler", this);
}

WorkerSchedulerImpl::~WorkerSchedulerImpl() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler"), "WorkerScheduler", this);

  helper_->RemoveTaskTimeObserver(this);
}

scoped_refptr<base::SingleThreadTaskRunner>
WorkerSchedulerImpl::DefaultTaskRunner() {
  DCHECK(initialized_);
  return helper_->DefaultWorkerTaskQueue();
}

scoped_refptr<SingleThreadIdleTaskRunner>
WorkerSchedulerImpl::IdleTaskRunner() {
  DCHECK(initialized_);
  return idle_helper_.IdleTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
WorkerSchedulerImpl::IPCTaskRunner() {
  return base::ThreadTaskRunnerHandle::Get();
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
  helper_->AddTaskObserver(task_observer);
}

void WorkerSchedulerImpl::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK(initialized_);
  helper_->RemoveTaskObserver(task_observer);
}

void WorkerSchedulerImpl::Shutdown() {
  DCHECK(initialized_);
  load_tracker_.RecordIdle(helper_->NowTicks());
  base::TimeTicks end_time = helper_->NowTicks();
  base::TimeDelta delta = end_time - thread_start_time_;

  // The lifetime could be radically different for different workers,
  // some workers could be short-lived (but last at least 1 sec in
  // Service Workers case) or could be around as long as the tab is open.
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "WorkerThread.Runtime", delta, base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromDays(1), 50 /* bucket count */);
  helper_->Shutdown();
}

scoped_refptr<WorkerTaskQueue> WorkerSchedulerImpl::DefaultTaskQueue() {
  DCHECK(initialized_);
  return helper_->DefaultWorkerTaskQueue();
}

void WorkerSchedulerImpl::Init() {
  initialized_ = true;
  idle_helper_.EnableLongIdlePeriod();
}

void WorkerSchedulerImpl::OnTaskCompleted(
    WorkerTaskQueue* worker_task_queue,
    const TaskQueue::Task& task,
    base::TimeTicks start,
    base::TimeTicks end,
    base::Optional<base::TimeDelta> thread_time) {
  worker_metrics_helper_.RecordTaskMetrics(worker_task_queue, task, start, end,
                                           thread_time);
}

SchedulerHelper* WorkerSchedulerImpl::GetSchedulerHelperForTesting() {
  return helper_.get();
}

bool WorkerSchedulerImpl::CanEnterLongIdlePeriod(base::TimeTicks,
                                                 base::TimeDelta*) {
  return true;
}

base::TimeTicks WorkerSchedulerImpl::CurrentIdleTaskDeadlineForTesting() const {
  return idle_helper_.CurrentIdleTaskDeadline();
}

void WorkerSchedulerImpl::WillProcessTask(double start_time) {}

void WorkerSchedulerImpl::DidProcessTask(double start_time, double end_time) {
  base::TimeTicks start_time_ticks =
      MonotonicTimeInSecondsToTimeTicks(start_time);
  base::TimeTicks end_time_ticks = MonotonicTimeInSecondsToTimeTicks(end_time);

  load_tracker_.RecordTaskTime(start_time_ticks, end_time_ticks);
}

void WorkerSchedulerImpl::SetThreadType(WebThreadType thread_type) {
  DCHECK_NE(thread_type, WebThreadType::kMainThread);
  worker_metrics_helper_.SetThreadType(thread_type);
}

void WorkerSchedulerImpl::OnThrottlingStateChanged(
    WebFrameScheduler::ThrottlingState throttling_state) {
  throttling_state_ = throttling_state;
}

scoped_refptr<WorkerTaskQueue> WorkerSchedulerImpl::ControlTaskQueue() {
  return helper_->ControlWorkerTaskQueue();
}

base::WeakPtr<WorkerSchedulerImpl> WorkerSchedulerImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace scheduler
}  // namespace blink
