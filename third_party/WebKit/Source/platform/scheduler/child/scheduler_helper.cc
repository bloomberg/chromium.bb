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
    : task_queue_manager_delegate_(task_queue_manager_delegate),
      task_queue_manager_(new TaskQueueManager(task_queue_manager_delegate)),
      observer_(nullptr) {
  task_queue_manager_->SetWorkBatchSize(4);
}

void SchedulerHelper::InitDefaultQueues(
    scoped_refptr<TaskQueue> default_task_queue,
    scoped_refptr<TaskQueue> control_task_queue) {
  control_task_queue->SetQueuePriority(TaskQueue::CONTROL_PRIORITY);

  DCHECK(task_queue_manager_delegate_);
  task_queue_manager_delegate_->SetDefaultTaskRunner(default_task_queue);
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

void SchedulerHelper::OnTriedToExecuteBlockedTask() {
  if (observer_)
    observer_->OnTriedToExecuteBlockedTask();
}

void SchedulerHelper::OnBeginNestedRunLoop() {
  if (observer_)
    observer_->OnBeginNestedRunLoop();
}

}  // namespace scheduler
}  // namespace blink
