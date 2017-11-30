// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/task_queue.h"

#include "base/bind_helpers.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/task_queue_manager.h"

namespace blink {
namespace scheduler {

TaskQueue::TaskQueue(std::unique_ptr<internal::TaskQueueImpl> impl,
                     const TaskQueue::Spec& spec)
    : impl_(std::move(impl)),
      thread_id_(base::PlatformThread::CurrentId()),
      task_queue_manager_(impl_ ? impl_->GetTaskQueueManagerWeakPtr()
                                : nullptr),
      graceful_queue_shutdown_helper_(
          impl_ ? impl_->GetGracefulQueueShutdownHelper() : nullptr) {}

TaskQueue::~TaskQueue() {
  // scoped_refptr guarantees us that this object isn't used.
  if (!impl_)
    return;
  if (impl_->IsUnregistered())
    return;
  graceful_queue_shutdown_helper_->GracefullyShutdownTaskQueue(
      TakeTaskQueueImpl());
}

TaskQueue::Task::Task(TaskQueue::PostedTask task,
                      base::TimeTicks desired_run_time)
    : PendingTask(task.posted_from,
                  std::move(task.callback),
                  desired_run_time,
                  task.nestable),
      task_type_(task.task_type) {}

TaskQueue::PostedTask::PostedTask(base::OnceClosure callback,
                                  base::Location posted_from,
                                  base::TimeDelta delay,
                                  base::Nestable nestable,
                                  base::Optional<TaskType> task_type)
    : callback(std::move(callback)),
      posted_from(posted_from),
      delay(delay),
      nestable(nestable),
      task_type(task_type) {}

void TaskQueue::ShutdownTaskQueue() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  base::AutoLock lock(impl_lock_);
  if (!impl_)
    return;
  if (!task_queue_manager_) {
    impl_.reset();
    return;
  }
  impl_->SetBlameContext(nullptr);
  task_queue_manager_->UnregisterTaskQueueImpl(TakeTaskQueueImpl());
}

bool TaskQueue::RunsTasksInCurrentSequence() const {
  return IsOnMainThread();
}

bool TaskQueue::PostDelayedTask(const base::Location& from_here,
                                base::OnceClosure task,
                                base::TimeDelta delay) {
  auto lock = AcquireImplReadLockIfNeeded();
  if (!impl_)
    return false;
  return impl_->PostDelayedTask(
      PostedTask(std::move(task), from_here, delay, base::Nestable::kNestable));
}

bool TaskQueue::PostNonNestableDelayedTask(const base::Location& from_here,
                                           base::OnceClosure task,
                                           base::TimeDelta delay) {
  auto lock = AcquireImplReadLockIfNeeded();
  if (!impl_)
    return false;
  return impl_->PostDelayedTask(PostedTask(std::move(task), from_here, delay,
                                           base::Nestable::kNonNestable));
}

bool TaskQueue::PostTaskWithMetadata(PostedTask task) {
  auto lock = AcquireImplReadLockIfNeeded();
  if (!impl_)
    return false;
  return impl_->PostDelayedTask(std::move(task));
}

std::unique_ptr<TaskQueue::QueueEnabledVoter>
TaskQueue::CreateQueueEnabledVoter() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return nullptr;
  return impl_->CreateQueueEnabledVoter(this);
}

bool TaskQueue::IsQueueEnabled() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return false;
  return impl_->IsQueueEnabled();
}

bool TaskQueue::IsEmpty() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return true;
  return impl_->IsEmpty();
}

size_t TaskQueue::GetNumberOfPendingTasks() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return 0;
  return impl_->GetNumberOfPendingTasks();
}

bool TaskQueue::HasTaskToRunImmediately() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return false;
  return impl_->HasTaskToRunImmediately();
}

base::Optional<base::TimeTicks> TaskQueue::GetNextScheduledWakeUp() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return base::nullopt;
  return impl_->GetNextScheduledWakeUp();
}

void TaskQueue::SetQueuePriority(TaskQueue::QueuePriority priority) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return;
  impl_->SetQueuePriority(priority);
}

TaskQueue::QueuePriority TaskQueue::GetQueuePriority() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return TaskQueue::QueuePriority::kLowPriority;
  return impl_->GetQueuePriority();
}

void TaskQueue::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return;
  impl_->AddTaskObserver(task_observer);
}

void TaskQueue::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return;
  impl_->RemoveTaskObserver(task_observer);
}

void TaskQueue::SetTimeDomain(TimeDomain* time_domain) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return;
  impl_->SetTimeDomain(time_domain);
}

TimeDomain* TaskQueue::GetTimeDomain() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return nullptr;
  return impl_->GetTimeDomain();
}

void TaskQueue::SetBlameContext(
    base::trace_event::BlameContext* blame_context) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return;
  impl_->SetBlameContext(blame_context);
}

void TaskQueue::InsertFence(InsertFencePosition position) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return;
  impl_->InsertFence(position);
}

void TaskQueue::InsertFenceAt(base::TimeTicks time) {
  impl_->InsertFenceAt(time);
}

void TaskQueue::RemoveFence() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return;
  impl_->RemoveFence();
}

bool TaskQueue::HasActiveFence() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return false;
  return impl_->HasActiveFence();
}

bool TaskQueue::BlockedByFence() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return false;
  return impl_->BlockedByFence();
}

const char* TaskQueue::GetName() const {
  auto lock = AcquireImplReadLockIfNeeded();
  if (!impl_)
    return "";
  return impl_->GetName();
}

void TaskQueue::SetObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (!impl_)
    return;
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

bool TaskQueue::IsOnMainThread() const {
  return thread_id_ == base::PlatformThread::CurrentId();
}

base::Optional<MoveableAutoLock> TaskQueue::AcquireImplReadLockIfNeeded()
    const {
  if (IsOnMainThread())
    return base::nullopt;
  return MoveableAutoLock(impl_lock_);
}

std::unique_ptr<internal::TaskQueueImpl> TaskQueue::TakeTaskQueueImpl() {
  DCHECK(impl_);
  impl_->SetOnTaskStartedHandler(
      internal::TaskQueueImpl::OnTaskStartedHandler());
  impl_->SetOnTaskCompletedHandler(
      internal::TaskQueueImpl::OnTaskCompletedHandler());
  return std::move(impl_);
}

}  // namespace scheduler
}  // namespace blink
