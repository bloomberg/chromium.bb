// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/task_queue.h"

#include "platform/scheduler/base/task_queue_impl.h"

namespace blink {
namespace scheduler {

TaskQueue::TaskQueue(std::unique_ptr<internal::TaskQueueImpl> impl)
    : impl_(std::move(impl)) {}

TaskQueue::~TaskQueue() {}

void TaskQueue::UnregisterTaskQueue() {
  impl_->UnregisterTaskQueue(this);
}

bool TaskQueue::RunsTasksInCurrentSequence() const {
  return impl_->RunsTasksInCurrentSequence();
}

bool TaskQueue::PostDelayedTask(const tracked_objects::Location& from_here,
                                base::OnceClosure task,
                                base::TimeDelta delay) {
  return impl_->PostDelayedTask(from_here, std::move(task), delay);
}

bool TaskQueue::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  return impl_->PostNonNestableDelayedTask(from_here, std::move(task), delay);
}

std::unique_ptr<TaskQueue::QueueEnabledVoter>
TaskQueue::CreateQueueEnabledVoter() {
  return impl_->CreateQueueEnabledVoter(this);
}

bool TaskQueue::IsQueueEnabled() const {
  return impl_->IsQueueEnabled();
}

bool TaskQueue::IsEmpty() const {
  return impl_->IsEmpty();
}

size_t TaskQueue::GetNumberOfPendingTasks() const {
  return impl_->GetNumberOfPendingTasks();
}

bool TaskQueue::HasTaskToRunImmediately() const {
  return impl_->HasTaskToRunImmediately();
}

base::Optional<base::TimeTicks> TaskQueue::GetNextScheduledWakeUp() {
  return impl_->GetNextScheduledWakeUp();
}

void TaskQueue::SetQueuePriority(TaskQueue::QueuePriority priority) {
  impl_->SetQueuePriority(priority);
}

TaskQueue::QueuePriority TaskQueue::GetQueuePriority() const {
  return impl_->GetQueuePriority();
}

void TaskQueue::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  impl_->AddTaskObserver(task_observer);
}

void TaskQueue::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  impl_->RemoveTaskObserver(task_observer);
}

void TaskQueue::SetTimeDomain(TimeDomain* time_domain) {
  impl_->SetTimeDomain(time_domain);
}

TimeDomain* TaskQueue::GetTimeDomain() const {
  return impl_->GetTimeDomain();
}

void TaskQueue::SetBlameContext(
    base::trace_event::BlameContext* blame_context) {
  impl_->SetBlameContext(blame_context);
}

void TaskQueue::InsertFence(InsertFencePosition position) {
  impl_->InsertFence(position);
}

void TaskQueue::RemoveFence() {
  impl_->RemoveFence();
}

bool TaskQueue::HasFence() const {
  return impl_->HasFence();
}

bool TaskQueue::BlockedByFence() const {
  return impl_->BlockedByFence();
}

const char* TaskQueue::GetName() const {
  return impl_->GetName();
}

void TaskQueue::SetObserver(Observer* observer) {
  if (observer) {
    // Observer is guaranteed to outlive TaskQueue and TaskQueueImpl lifecycle
    // is controlled by |this|.
    impl_->SetOnNextWakeUpChangedCallback(
        base::Bind(&TaskQueue::Observer::OnQueueNextWakeUpChanged,
                   base::Unretained(observer), base::Unretained(this)));
  } else {
    impl_->SetOnNextWakeUpChangedCallback(
        base::Callback<void(base::TimeTicks)>());
  }
}

}  // namespace scheduler
}  // namespace blink
