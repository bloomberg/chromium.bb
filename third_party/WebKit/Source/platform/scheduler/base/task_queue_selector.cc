// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/task_queue_selector.h"

#include "base/logging.h"
#include "base/trace_event/trace_event_argument.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/work_queue.h"

namespace blink {
namespace scheduler {
namespace internal {

TaskQueueSelector::TaskQueueSelector()
    : enabled_selector_(this, "enabled"),
      blocked_selector_(this, "blocked"),
      immediate_starvation_count_(0),
      normal_priority_starvation_score_(0),
      low_priority_starvation_score_(0),
      num_blocked_queues_to_report_(0),
      task_queue_selector_observer_(nullptr) {}

TaskQueueSelector::~TaskQueueSelector() = default;

void TaskQueueSelector::AddQueue(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(queue->IsQueueEnabled());
  enabled_selector_.AddQueue(queue, TaskQueue::kNormalPriority);
}

void TaskQueueSelector::RemoveQueue(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (queue->IsQueueEnabled()) {
    enabled_selector_.RemoveQueue(queue);
// The #if DCHECK_IS_ON() shouldn't be necessary but this doesn't compile on
// chromeos bots without it :(
#if DCHECK_IS_ON()
    DCHECK(!blocked_selector_.CheckContainsQueueForTest(queue));
#endif
  } else if (queue->should_report_when_execution_blocked()) {
    DCHECK_GT(num_blocked_queues_to_report_, 0u);
    num_blocked_queues_to_report_--;
    blocked_selector_.RemoveQueue(queue);
#if DCHECK_IS_ON()
    DCHECK(!enabled_selector_.CheckContainsQueueForTest(queue));
#endif
  }
}

void TaskQueueSelector::EnableQueue(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(queue->IsQueueEnabled());
  if (queue->should_report_when_execution_blocked()) {
    DCHECK_GT(num_blocked_queues_to_report_, 0u);
    num_blocked_queues_to_report_--;
    blocked_selector_.RemoveQueue(queue);
  }
  enabled_selector_.AddQueue(queue, queue->GetQueuePriority());
  if (task_queue_selector_observer_)
    task_queue_selector_observer_->OnTaskQueueEnabled(queue);
}

void TaskQueueSelector::DisableQueue(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(!queue->IsQueueEnabled());
  enabled_selector_.RemoveQueue(queue);
  if (queue->should_report_when_execution_blocked()) {
    blocked_selector_.AddQueue(queue, queue->GetQueuePriority());
    num_blocked_queues_to_report_++;
  }
}

void TaskQueueSelector::SetQueuePriority(internal::TaskQueueImpl* queue,
                                         TaskQueue::QueuePriority priority) {
  DCHECK_LT(priority, TaskQueue::kQueuePriorityCount);
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (queue->IsQueueEnabled()) {
    enabled_selector_.ChangeSetIndex(queue, priority);
  } else if (queue->should_report_when_execution_blocked()) {
    blocked_selector_.ChangeSetIndex(queue, priority);
  } else {
    // Normally blocked_selector_.ChangeSetIndex would assign the queue's
    // priority, however if |queue->should_report_when_execution_blocked()| is
    // false then the disabled queue is not in any set so we need to do it here.
    queue->delayed_work_queue()->AssignSetIndex(priority);
    queue->immediate_work_queue()->AssignSetIndex(priority);
  }
  DCHECK_EQ(priority, queue->GetQueuePriority());
}

TaskQueue::QueuePriority TaskQueueSelector::NextPriority(
    TaskQueue::QueuePriority priority) {
  DCHECK(priority < TaskQueue::kQueuePriorityCount);
  return static_cast<TaskQueue::QueuePriority>(static_cast<int>(priority) + 1);
}

TaskQueueSelector::PrioritizingSelector::PrioritizingSelector(
    TaskQueueSelector* task_queue_selector,
    const char* name)
    : task_queue_selector_(task_queue_selector),
      delayed_work_queue_sets_(TaskQueue::kQueuePriorityCount, name),
      immediate_work_queue_sets_(TaskQueue::kQueuePriorityCount, name) {}

void TaskQueueSelector::PrioritizingSelector::AddQueue(
    internal::TaskQueueImpl* queue,
    TaskQueue::QueuePriority priority) {
#if DCHECK_IS_ON()
  DCHECK(!CheckContainsQueueForTest(queue));
#endif
  delayed_work_queue_sets_.AddQueue(queue->delayed_work_queue(), priority);
  immediate_work_queue_sets_.AddQueue(queue->immediate_work_queue(), priority);
#if DCHECK_IS_ON()
  DCHECK(CheckContainsQueueForTest(queue));
#endif
}

void TaskQueueSelector::PrioritizingSelector::ChangeSetIndex(
    internal::TaskQueueImpl* queue,
    TaskQueue::QueuePriority priority) {
#if DCHECK_IS_ON()
  DCHECK(CheckContainsQueueForTest(queue));
#endif
  delayed_work_queue_sets_.ChangeSetIndex(queue->delayed_work_queue(),
                                          priority);
  immediate_work_queue_sets_.ChangeSetIndex(queue->immediate_work_queue(),
                                            priority);
#if DCHECK_IS_ON()
  DCHECK(CheckContainsQueueForTest(queue));
#endif
}

void TaskQueueSelector::PrioritizingSelector::RemoveQueue(
    internal::TaskQueueImpl* queue) {
#if DCHECK_IS_ON()
  DCHECK(CheckContainsQueueForTest(queue));
#endif
  delayed_work_queue_sets_.RemoveQueue(queue->delayed_work_queue());
  immediate_work_queue_sets_.RemoveQueue(queue->immediate_work_queue());

#if DCHECK_IS_ON()
  DCHECK(!CheckContainsQueueForTest(queue));
#endif
}

bool TaskQueueSelector::PrioritizingSelector::
    ChooseOldestImmediateTaskWithPriority(TaskQueue::QueuePriority priority,
                                          WorkQueue** out_work_queue) const {
  return immediate_work_queue_sets_.GetOldestQueueInSet(priority,
                                                        out_work_queue);
}

bool TaskQueueSelector::PrioritizingSelector::
    ChooseOldestDelayedTaskWithPriority(TaskQueue::QueuePriority priority,
                                        WorkQueue** out_work_queue) const {
  return delayed_work_queue_sets_.GetOldestQueueInSet(priority, out_work_queue);
}

bool TaskQueueSelector::PrioritizingSelector::
    ChooseOldestImmediateOrDelayedTaskWithPriority(
        TaskQueue::QueuePriority priority,
        bool* out_chose_delayed_over_immediate,
        WorkQueue** out_work_queue) const {
  WorkQueue* immediate_queue;
  DCHECK_EQ(*out_chose_delayed_over_immediate, false);
  EnqueueOrder immediate_enqueue_order;
  if (immediate_work_queue_sets_.GetOldestQueueAndEnqueueOrderInSet(
          priority, &immediate_queue, &immediate_enqueue_order)) {
    WorkQueue* delayed_queue;
    EnqueueOrder delayed_enqueue_order;
    if (delayed_work_queue_sets_.GetOldestQueueAndEnqueueOrderInSet(
            priority, &delayed_queue, &delayed_enqueue_order)) {
      if (immediate_enqueue_order < delayed_enqueue_order) {
        *out_work_queue = immediate_queue;
      } else {
        *out_chose_delayed_over_immediate = true;
        *out_work_queue = delayed_queue;
      }
    } else {
      *out_work_queue = immediate_queue;
    }
    return true;
  }
  return delayed_work_queue_sets_.GetOldestQueueInSet(priority, out_work_queue);
}

bool TaskQueueSelector::PrioritizingSelector::ChooseOldestWithPriority(
    TaskQueue::QueuePriority priority,
    bool* out_chose_delayed_over_immediate,
    WorkQueue** out_work_queue) const {
  // Select an immediate work queue if we are starving immediate tasks.
  if (task_queue_selector_->immediate_starvation_count_ >=
      kMaxDelayedStarvationTasks) {
    if (ChooseOldestImmediateTaskWithPriority(priority, out_work_queue))
      return true;
    return ChooseOldestDelayedTaskWithPriority(priority, out_work_queue);
  }
  return ChooseOldestImmediateOrDelayedTaskWithPriority(
      priority, out_chose_delayed_over_immediate, out_work_queue);
}

bool TaskQueueSelector::PrioritizingSelector::SelectWorkQueueToService(
    TaskQueue::QueuePriority max_priority,
    WorkQueue** out_work_queue,
    bool* out_chose_delayed_over_immediate) {
  DCHECK(task_queue_selector_->main_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(*out_chose_delayed_over_immediate, false);

  // Always service the control queue if it has any work.
  if (max_priority > TaskQueue::kControlPriority &&
      ChooseOldestWithPriority(TaskQueue::kControlPriority,
                               out_chose_delayed_over_immediate,
                               out_work_queue)) {
    return true;
  }

  // Select from the low priority queue if we are starving it.
  if (max_priority > TaskQueue::kLowPriority &&
      task_queue_selector_->low_priority_starvation_score_ >=
          kMaxLowPriorityStarvationScore &&
      ChooseOldestWithPriority(TaskQueue::kLowPriority,
                               out_chose_delayed_over_immediate,
                               out_work_queue)) {
    return true;
  }
  // Select from the normal priority queue if we are starving it.
  if (max_priority > TaskQueue::kNormalPriority &&
      task_queue_selector_->normal_priority_starvation_score_ >=
          kMaxNormalPriorityStarvationScore &&
      ChooseOldestWithPriority(TaskQueue::kNormalPriority,
                               out_chose_delayed_over_immediate,
                               out_work_queue)) {
    return true;
  }
  // Otherwise choose in priority order.
  for (TaskQueue::QueuePriority priority = TaskQueue::kHighPriority;
       priority < max_priority; priority = NextPriority(priority)) {
    if (ChooseOldestWithPriority(priority, out_chose_delayed_over_immediate,
                                 out_work_queue)) {
      return true;
    }
  }
  return false;
}

#if DCHECK_IS_ON() || !defined(NDEBUG)
bool TaskQueueSelector::PrioritizingSelector::CheckContainsQueueForTest(
    const internal::TaskQueueImpl* queue) const {
  bool contains_delayed_work_queue =
      delayed_work_queue_sets_.ContainsWorkQueueForTest(
          queue->delayed_work_queue());

  bool contains_immediate_work_queue =
      immediate_work_queue_sets_.ContainsWorkQueueForTest(
          queue->immediate_work_queue());

  DCHECK_EQ(contains_delayed_work_queue, contains_immediate_work_queue);
  return contains_delayed_work_queue;
}
#endif

bool TaskQueueSelector::SelectWorkQueueToService(WorkQueue** out_work_queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  bool chose_delayed_over_immediate = false;
  bool found_queue = enabled_selector_.SelectWorkQueueToService(
      TaskQueue::kQueuePriorityCount, out_work_queue,
      &chose_delayed_over_immediate);
  if (!found_queue) {
    TrySelectingBlockedQueue();
    return false;
  }

  TrySelectingBlockedQueueOverEnabledQueue(**out_work_queue);
  // We could use |(*out_work_queue)->task_queue()->GetQueuePriority()| here but
  // for re-queued non-nestable tasks |task_queue()| returns null.
  DidSelectQueueWithPriority(static_cast<TaskQueue::QueuePriority>(
                                 (*out_work_queue)->work_queue_set_index()),
                             chose_delayed_over_immediate);
  return true;
}

void TaskQueueSelector::TrySelectingBlockedQueue() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (!num_blocked_queues_to_report_ || !task_queue_selector_observer_)
    return;
  WorkQueue* chosen_blocked_queue;
  bool chose_delayed_over_immediate = false;
  // There was nothing unblocked to run, see if we could have run a blocked
  // task.
  if (blocked_selector_.SelectWorkQueueToService(
          TaskQueue::kQueuePriorityCount, &chosen_blocked_queue,
          &chose_delayed_over_immediate)) {
    task_queue_selector_observer_->OnTriedToSelectBlockedWorkQueue(
        chosen_blocked_queue);
  }
}

void TaskQueueSelector::TrySelectingBlockedQueueOverEnabledQueue(
    const WorkQueue& chosen_enabled_queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (!num_blocked_queues_to_report_ || !task_queue_selector_observer_)
    return;

  TaskQueue::QueuePriority max_priority =
      NextPriority(chosen_enabled_queue.task_queue()->GetQueuePriority());

  WorkQueue* chosen_blocked_queue;
  bool chose_delayed_over_immediate = false;
  bool found_queue = blocked_selector_.SelectWorkQueueToService(
      max_priority, &chosen_blocked_queue, &chose_delayed_over_immediate);
  if (!found_queue)
    return;

  // Check if the chosen blocked queue has a lower numerical priority than the
  // chosen enabled queue.  If so we would have chosen the blocked queue (since
  // zero is the highest priority).
  if (chosen_blocked_queue->task_queue()->GetQueuePriority() <
      chosen_enabled_queue.task_queue()->GetQueuePriority()) {
    task_queue_selector_observer_->OnTriedToSelectBlockedWorkQueue(
        chosen_blocked_queue);
    return;
  }
  DCHECK_EQ(chosen_blocked_queue->task_queue()->GetQueuePriority(),
            chosen_enabled_queue.task_queue()->GetQueuePriority());
  // Otherwise there was an enabled and a blocked task with the same priority.
  // The one with the older enqueue order wins.
  if (chosen_blocked_queue->ShouldRunBefore(&chosen_enabled_queue)) {
    task_queue_selector_observer_->OnTriedToSelectBlockedWorkQueue(
        chosen_blocked_queue);
  }
}

void TaskQueueSelector::DidSelectQueueWithPriority(
    TaskQueue::QueuePriority priority,
    bool chose_delayed_over_immediate) {
  switch (priority) {
    case TaskQueue::kControlPriority:
      break;
    case TaskQueue::kHighPriority:
      low_priority_starvation_score_ +=
          kSmallScoreIncrementForLowPriorityStarvation;
      normal_priority_starvation_score_ +=
          kSmallScoreIncrementForNormalPriorityStarvation;
      break;
    case TaskQueue::kNormalPriority:
      low_priority_starvation_score_ +=
          kLargeScoreIncrementForLowPriorityStarvation;
      normal_priority_starvation_score_ = 0;
      break;
    case TaskQueue::kLowPriority:
    case TaskQueue::kBestEffortPriority:
      low_priority_starvation_score_ = 0;
      normal_priority_starvation_score_ = 0;
      break;
    default:
      NOTREACHED();
  }
  if (chose_delayed_over_immediate) {
    immediate_starvation_count_++;
  } else {
    immediate_starvation_count_ = 0;
  }
}

void TaskQueueSelector::AsValueInto(
    base::trace_event::TracedValue* state) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  state->SetInteger("normal_priority_starvation_score",
                    normal_priority_starvation_score_);
  state->SetInteger("low_priority_starvation_score",
                    low_priority_starvation_score_);
  state->SetInteger("immediate_starvation_count", immediate_starvation_count_);
  state->SetInteger("num_blocked_queues_to_report",
                    num_blocked_queues_to_report_);
}

void TaskQueueSelector::SetTaskQueueSelectorObserver(Observer* observer) {
  task_queue_selector_observer_ = observer;
}

bool TaskQueueSelector::EnabledWorkQueuesEmpty() const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  for (TaskQueue::QueuePriority priority = TaskQueue::kControlPriority;
       priority < TaskQueue::kQueuePriorityCount;
       priority = NextPriority(priority)) {
    if (!enabled_selector_.delayed_work_queue_sets()->IsSetEmpty(priority) ||
        !enabled_selector_.immediate_work_queue_sets()->IsSetEmpty(priority)) {
      return false;
    }
  }
  return true;
}

void TaskQueueSelector::SetImmediateStarvationCountForTest(
    size_t immediate_starvation_count) {
  immediate_starvation_count_ = immediate_starvation_count;
}

}  // namespace internal
}  // namespace scheduler
}  // namespace blink
