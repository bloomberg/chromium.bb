// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/work_queue.h"

#include "platform/scheduler/base/work_queue_sets.h"

namespace blink {
namespace scheduler {
namespace internal {

WorkQueue::WorkQueue(TaskQueueImpl* task_queue,
                     const char* name,
                     TaskQueueImpl::Task::ComparatorFn comparator)
    : work_queue_(comparator),
      work_queue_sets_(nullptr),
      task_queue_(task_queue),
      work_queue_set_index_(0),
      name_(name) {}

void WorkQueue::AsValueInto(base::trace_event::TracedValue* state) const {
  for (const TaskQueueImpl::Task& task : work_queue_) {
    TaskQueueImpl::TaskAsValueInto(task, state);
  }
}

WorkQueue::~WorkQueue() {
  DCHECK(!work_queue_sets_) << task_queue_->GetName() << " : "
                            << work_queue_sets_->name() << " : " << name_;
}

const TaskQueueImpl::Task* WorkQueue::GetFrontTask() const {
  if (work_queue_.empty())
    return nullptr;
  return &*work_queue_.begin();
}

bool WorkQueue::GetFrontTaskEnqueueOrder(EnqueueOrder* enqueue_order) const {
  if (work_queue_.empty())
    return false;
  // Quick sanity check.
  DCHECK_LE(work_queue_.begin()->enqueue_order(),
            work_queue_.rbegin()->enqueue_order())
      << task_queue_->GetName() << " : "
      << work_queue_sets_->name() << " : " << name_;
  *enqueue_order = work_queue_.begin()->enqueue_order();
  return true;
}

void WorkQueue::Push(TaskQueueImpl::Task task) {
  bool was_empty = work_queue_.empty();
#ifndef NDEBUG
  DCHECK(task.enqueue_order_set());
#endif

  // We expect |task| to be inserted at the end. Amoritized O(1).
  work_queue_.insert(work_queue_.end(), std::move(task));
  DCHECK_EQ(task.enqueue_order(), work_queue_.rbegin()->enqueue_order())
      << task_queue_->GetName() << " : "
      << work_queue_sets_->name() << " : " << name_
      << "task [scheduled_run_time_" << task.delayed_run_time << ", "
      << task.sequence_num <<
      "] rbegin() [" << work_queue_.rbegin()->delayed_run_time << ", "
      << work_queue_.rbegin()->sequence_num << "]";

  if (was_empty && work_queue_sets_) {
    work_queue_sets_->OnPushQueue(this);
  }
}

bool WorkQueue::CancelTask(const TaskQueueImpl::Task& key) {
  TaskQueueImpl::ComparatorQueue::iterator it = work_queue_.find(key);
  if (it == work_queue_.end())
    return false;

  if (it == work_queue_.begin()) {
    EnqueueOrder erased_task_enqueue_order = it->enqueue_order();
    work_queue_.erase(it);
    // We can't guarantee this WorkQueue is the lowest value in the WorkQueueSet
    // so we need to use OnQueueHeadChanged instead of OnPopQueue for
    // correctness.
    work_queue_sets_->OnQueueHeadChanged(this, erased_task_enqueue_order);
  } else {
    work_queue_.erase(it);
  }
  task_queue_->TraceQueueSize(false);
  return true;
}

bool WorkQueue::IsTaskPending(const TaskQueueImpl::Task& key) const {
  return work_queue_.find(key) != work_queue_.end();
}

void WorkQueue::PopTaskForTest() {
  if (work_queue_.empty())
    return;
  work_queue_.erase(work_queue_.begin());
}

void WorkQueue::SwapLocked(TaskQueueImpl::ComparatorQueue& incoming_queue) {
  DCHECK(work_queue_.empty());
  std::swap(work_queue_, incoming_queue);
  if (!work_queue_.empty() && work_queue_sets_)
    work_queue_sets_->OnPushQueue(this);
  task_queue_->TraceQueueSize(true);
}

TaskQueueImpl::Task WorkQueue::TakeTaskFromWorkQueue() {
  DCHECK(work_queue_sets_);
  DCHECK(!work_queue_.empty());
  TaskQueueImpl::ComparatorQueue::iterator it = work_queue_.begin();
  TaskQueueImpl::Task pending_task =
      std::move(const_cast<TaskQueueImpl::Task&>(*it));
  work_queue_.erase(it);
  work_queue_sets_->OnPopQueue(this);
  task_queue_->TraceQueueSize(false);
  return pending_task;
}

void WorkQueue::AssignToWorkQueueSets(WorkQueueSets* work_queue_sets) {
  work_queue_sets_ = work_queue_sets;
}

void WorkQueue::AssignSetIndex(size_t work_queue_set_index) {
  work_queue_set_index_ = work_queue_set_index;
}

bool WorkQueue::ShouldRunBefore(const WorkQueue* other_queue) const {
  DCHECK(!work_queue_.empty());
  DCHECK(!other_queue->work_queue_.empty());
  EnqueueOrder enqueue_order = 0;
  EnqueueOrder other_enqueue_order = 0;
  bool have_task = GetFrontTaskEnqueueOrder(&enqueue_order);
  bool have_other_task =
      other_queue->GetFrontTaskEnqueueOrder(&other_enqueue_order);
  DCHECK(have_task);
  DCHECK(have_other_task);
  return enqueue_order < other_enqueue_order;
}

}  // namespace internal
}  // namespace scheduler
}  // namespace blink
