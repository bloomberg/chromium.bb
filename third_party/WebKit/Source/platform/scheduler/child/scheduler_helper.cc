// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/scheduler_helper.h"

#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/child/scheduler_tqm_delegate.h"

namespace blink {
namespace scheduler {

SchedulerHelper::SchedulerHelper(
    scoped_refptr<SchedulerTqmDelegate> task_queue_manager_delegate)
    : SchedulerHelper(task_queue_manager_delegate,
                      TaskQueue::Spec(TaskQueue::QueueType::DEFAULT)
                          .SetShouldMonitorQuiescence(true)) {}

SchedulerHelper::SchedulerHelper(
    scoped_refptr<SchedulerTqmDelegate> task_queue_manager_delegate,
    TaskQueue::Spec default_task_queue_spec)
    : task_queue_manager_delegate_(task_queue_manager_delegate),
      task_queue_manager_(new TaskQueueManager(task_queue_manager_delegate)),
      control_task_queue_(
          NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::CONTROL)
                           .SetShouldNotifyObservers(false))),
      default_task_queue_(NewTaskQueue(default_task_queue_spec)),
      observer_(nullptr) {
  control_task_queue_->SetQueuePriority(TaskQueue::CONTROL_PRIORITY);

  task_queue_manager_->SetWorkBatchSize(4);

  DCHECK(task_queue_manager_delegate_);
  task_queue_manager_delegate_->SetDefaultTaskRunner(default_task_queue_.get());
}

SchedulerHelper::~SchedulerHelper() {
  Shutdown();
}

void SchedulerHelper::Shutdown() {
  CheckOnValidThread();
  if (task_queue_manager_)
    task_queue_manager_->SetObserver(nullptr);
  task_queue_manager_.reset();
  task_queue_manager_delegate_->RestoreDefaultTaskRunner();
}

void SchedulerHelper::SetRecordTaskDelayHistograms(
    bool record_task_delay_histograms) {
  if (!task_queue_manager_)
    return;

  task_queue_manager_->SetRecordTaskDelayHistograms(
      record_task_delay_histograms);
}

scoped_refptr<TaskQueue> SchedulerHelper::NewTaskQueue(
    const TaskQueue::Spec& spec) {
  DCHECK(task_queue_manager_.get());
  return task_queue_manager_->NewTaskQueue(spec);
}

scoped_refptr<TaskQueue> SchedulerHelper::DefaultTaskQueue() {
  CheckOnValidThread();
  return default_task_queue_;
}

scoped_refptr<TaskQueue> SchedulerHelper::ControlTaskQueue() {
  return control_task_queue_;
}

size_t SchedulerHelper::GetNumberOfPendingTasks() const {
  return task_queue_manager_->GetNumberOfPendingTasks();
}

void SchedulerHelper::SetWorkBatchSizeForTesting(size_t work_batch_size) {
  CheckOnValidThread();
  DCHECK(task_queue_manager_.get());
  task_queue_manager_->SetWorkBatchSize(work_batch_size);
}

TaskQueueManager* SchedulerHelper::GetTaskQueueManagerForTesting() {
  CheckOnValidThread();
  return task_queue_manager_.get();
}

const scoped_refptr<SchedulerTqmDelegate>&
SchedulerHelper::scheduler_tqm_delegate() const {
  return task_queue_manager_delegate_;
}

bool SchedulerHelper::GetAndClearSystemIsQuiescentBit() {
  CheckOnValidThread();
  DCHECK(task_queue_manager_.get());
  return task_queue_manager_->GetAndClearSystemIsQuiescentBit();
}

void SchedulerHelper::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  CheckOnValidThread();
  if (task_queue_manager_)
    task_queue_manager_->AddTaskObserver(task_observer);
}

void SchedulerHelper::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  CheckOnValidThread();
  if (task_queue_manager_)
    task_queue_manager_->RemoveTaskObserver(task_observer);
}

void SchedulerHelper::AddTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  if (task_queue_manager_)
    task_queue_manager_->AddTaskTimeObserver(task_time_observer);
}

void SchedulerHelper::RemoveTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  if (task_queue_manager_)
    task_queue_manager_->RemoveTaskTimeObserver(task_time_observer);
}

void SchedulerHelper::SetObserver(Observer* observer) {
  CheckOnValidThread();
  observer_ = observer;
  DCHECK(task_queue_manager_);
  task_queue_manager_->SetObserver(this);
}

void SchedulerHelper::SweepCanceledDelayedTasks() {
  CheckOnValidThread();
  DCHECK(task_queue_manager_);
  task_queue_manager_->SweepCanceledDelayedTasks();
}

RealTimeDomain* SchedulerHelper::real_time_domain() const {
  CheckOnValidThread();
  DCHECK(task_queue_manager_);
  return task_queue_manager_->real_time_domain();
}

void SchedulerHelper::RegisterTimeDomain(TimeDomain* time_domain) {
  CheckOnValidThread();
  DCHECK(task_queue_manager_);
  task_queue_manager_->RegisterTimeDomain(time_domain);
}

void SchedulerHelper::UnregisterTimeDomain(TimeDomain* time_domain) {
  CheckOnValidThread();
  if (task_queue_manager_)
    task_queue_manager_->UnregisterTimeDomain(time_domain);
}

void SchedulerHelper::OnUnregisterTaskQueue(
    const scoped_refptr<TaskQueue>& queue) {
  if (observer_)
    observer_->OnUnregisterTaskQueue(queue);
}

void SchedulerHelper::OnTriedToExecuteBlockedTask(
    const TaskQueue& queue,
    const base::PendingTask& task) {
  if (observer_)
    observer_->OnTriedToExecuteBlockedTask(queue, task);
}

TaskQueue* SchedulerHelper::CurrentlyExecutingTaskQueue() const {
  if (!task_queue_manager_)
    return nullptr;
  return task_queue_manager_->currently_executing_task_queue();
}

}  // namespace scheduler
}  // namespace blink
