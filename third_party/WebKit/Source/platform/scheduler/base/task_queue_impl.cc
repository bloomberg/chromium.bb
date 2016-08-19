// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/task_queue_impl.h"

#include "base/trace_event/blame_context.h"
#include "platform/scheduler/base/task_queue_manager.h"
#include "platform/scheduler/base/task_queue_manager_delegate.h"
#include "platform/scheduler/base/time_domain.h"
#include "platform/scheduler/base/work_queue.h"

namespace blink {
namespace scheduler {

TaskQueue::TaskHandle::TaskHandle() : enqueue_order_(0), sequence_number_(0) {}

TaskQueue::TaskHandle::TaskHandle(TaskQueue* task_queue, uint64_t enqueue_order)
    : enqueue_order_(enqueue_order),
#if DCHECK_IS_ON()
      task_queue_(task_queue),
#endif
      sequence_number_(0) {
  DCHECK_GT(enqueue_order, 0ull);
}

TaskQueue::TaskHandle::TaskHandle(TaskQueue* task_queue,
                                  base::TimeTicks scheduled_run_time,
                                  int sequence_number)
    : enqueue_order_(0),
      scheduled_run_time_(scheduled_run_time),
#if DCHECK_IS_ON()
      task_queue_(task_queue),
#endif
      sequence_number_(sequence_number) {
  DCHECK(!scheduled_run_time.is_null());
}

TaskQueue::TaskHandle::operator bool() const {
  return !scheduled_run_time_.is_null() ||
         internal::EnqueueOrderGenerator::IsValidEnqueueOrder(enqueue_order_);
}

namespace internal {

TaskQueueImpl::TaskQueueImpl(
    TaskQueueManager* task_queue_manager,
    TimeDomain* time_domain,
    const Spec& spec,
    const char* disabled_by_default_tracing_category,
    const char* disabled_by_default_verbose_tracing_category)
    : thread_id_(base::PlatformThread::CurrentId()),
      any_thread_(task_queue_manager, spec.pump_policy, time_domain),
      name_(spec.name),
      disabled_by_default_tracing_category_(
          disabled_by_default_tracing_category),
      disabled_by_default_verbose_tracing_category_(
          disabled_by_default_verbose_tracing_category),
      main_thread_only_(task_queue_manager,
                        spec.pump_policy,
                        this,
                        time_domain),
      wakeup_policy_(spec.wakeup_policy),
      should_monitor_quiescence_(spec.should_monitor_quiescence),
      should_notify_observers_(spec.should_notify_observers),
      should_report_when_execution_blocked_(
          spec.should_report_when_execution_blocked) {
  DCHECK(time_domain);
  time_domain->RegisterQueue(this);
}

TaskQueueImpl::~TaskQueueImpl() {
#if DCHECK_IS_ON()
  base::AutoLock lock(any_thread_lock_);
  // NOTE this check shouldn't fire because |TaskQueueManager::queues_|
  // contains a strong reference to this TaskQueueImpl and the TaskQueueManager
  // destructor calls UnregisterTaskQueue on all task queues.
  DCHECK(any_thread().task_queue_manager == nullptr)
      << "UnregisterTaskQueue must be called first!";

#endif
}

TaskQueueImpl::Task::Task()
    : PendingTask(tracked_objects::Location(),
                  base::Closure(),
                  base::TimeTicks(),
                  true),
#ifndef NDEBUG
      enqueue_order_set_(false),
#endif
      enqueue_order_(0) {
  sequence_num = 0;
}

TaskQueueImpl::Task::Task(const tracked_objects::Location& posted_from,
                          const base::Closure& task,
                          base::TimeTicks desired_run_time,
                          EnqueueOrder sequence_number,
                          bool nestable)
    : PendingTask(posted_from, task, desired_run_time, nestable),
#ifndef NDEBUG
      enqueue_order_set_(false),
#endif
      enqueue_order_(0) {
  sequence_num = sequence_number;
}

TaskQueueImpl::Task::Task(const tracked_objects::Location& posted_from,
                          const base::Closure& task,
                          base::TimeTicks desired_run_time,
                          EnqueueOrder sequence_number,
                          bool nestable,
                          EnqueueOrder enqueue_order)
    : PendingTask(posted_from, task, desired_run_time, nestable),
#ifndef NDEBUG
      enqueue_order_set_(true),
#endif
      enqueue_order_(enqueue_order) {
  sequence_num = sequence_number;
}

// static
TaskQueueImpl::Task TaskQueueImpl::Task::CreateFakeTaskFromHandle(
    const TaskHandle& handle) {
  if (handle.scheduled_run_time_.is_null()) {
    // It's an immediate task.
    return Task(tracked_objects::Location(), base::Closure(),
                handle.scheduled_run_time_, handle.sequence_number_, false,
                handle.enqueue_order_);
  } else {
    // It's a delayed task.
    DCHECK_EQ(0ull, handle.enqueue_order_);
    return Task(tracked_objects::Location(), base::Closure(),
                handle.scheduled_run_time_, handle.sequence_number_, false);
  }
}

bool TaskQueueImpl::Task::DelayedRunTimeComparator::operator()(
    const Task& a,
    const Task& b) const {
  if (a.delayed_run_time < b.delayed_run_time)
    return true;

  if (a.delayed_run_time > b.delayed_run_time)
    return false;

  // If the times happen to match, then we use the sequence number to decide.
  // Compare the difference to support integer roll-over.
  return (a.sequence_num - b.sequence_num) < 0;
}

// static
bool TaskQueueImpl::Task::EnqueueOrderComparatorFn(const Task& a,
                                                   const Task& b) {
  return a.enqueue_order() < b.enqueue_order();
}

// static
bool TaskQueueImpl::Task::DelayedRunTimeComparatorFn(const Task& a,
                                                     const Task& b) {
  if (a.delayed_run_time < b.delayed_run_time)
    return true;

  if (a.delayed_run_time > b.delayed_run_time)
    return false;

  // If the times happen to match, then we use the sequence number to decide.
  // Compare the difference to support integer roll-over.
  return (a.sequence_num - b.sequence_num) < 0;
}

TaskQueueImpl::AnyThread::AnyThread(TaskQueueManager* task_queue_manager,
                                    PumpPolicy pump_policy,
                                    TimeDomain* time_domain)
    : task_queue_manager(task_queue_manager),
      pump_policy(pump_policy),
      time_domain(time_domain),
      immediate_incoming_queue(&TaskQueueImpl::Task::EnqueueOrderComparatorFn) {
}

TaskQueueImpl::AnyThread::~AnyThread() {}

TaskQueueImpl::MainThreadOnly::MainThreadOnly(
    TaskQueueManager* task_queue_manager,
    PumpPolicy pump_policy,
    TaskQueueImpl* task_queue,
    TimeDomain* time_domain)
    : task_queue_manager(task_queue_manager),
      pump_policy(pump_policy),
      time_domain(time_domain),
      delayed_work_queue(
          new WorkQueue(task_queue,
                        "delayed",
                        &TaskQueueImpl::Task::DelayedRunTimeComparatorFn)),
      immediate_work_queue(
          new WorkQueue(task_queue,
                        "immediate",
                        &TaskQueueImpl::Task::EnqueueOrderComparatorFn)),
      set_index(0),
      is_enabled(true),
      blame_context(nullptr) {}

TaskQueueImpl::MainThreadOnly::~MainThreadOnly() {}

void TaskQueueImpl::UnregisterTaskQueue() {
  base::AutoLock lock(any_thread_lock_);
  if (main_thread_only().time_domain)
    main_thread_only().time_domain->UnregisterQueue(this);
  if (!any_thread().task_queue_manager)
    return;
  any_thread().time_domain = nullptr;
  main_thread_only().time_domain = nullptr;
  any_thread().task_queue_manager->UnregisterTaskQueue(this);

  any_thread().task_queue_manager = nullptr;
  main_thread_only().task_queue_manager = nullptr;
  main_thread_only().delayed_incoming_queue.clear();
  any_thread().immediate_incoming_queue.clear();
  main_thread_only().immediate_work_queue.reset();
  main_thread_only().delayed_work_queue.reset();
}

bool TaskQueueImpl::RunsTasksOnCurrentThread() const {
  base::AutoLock lock(any_thread_lock_);
  return base::PlatformThread::CurrentId() == thread_id_;
}

TaskQueue::TaskHandle TaskQueueImpl::PostCancellableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  if (!main_thread_only().task_queue_manager)
    return TaskHandle();

  EnqueueOrder sequence_number =
      main_thread_only().task_queue_manager->GetNextSequenceNumber();

  if (delay.is_zero()) {
    base::AutoLock lock(any_thread_lock_);
    PushOntoImmediateIncomingQueueLocked(
        Task(from_here, task, base::TimeTicks(), sequence_number, true,
             sequence_number));

    return TaskHandle(this, sequence_number);
  } else {
    DCHECK_GT(delay, base::TimeDelta());
    base::TimeTicks time_domain_now = main_thread_only().time_domain->Now();
    base::TimeTicks time_domain_delayed_run_time = time_domain_now + delay;
    PushOntoDelayedIncomingQueueFromMainThread(
        Task(from_here, task, time_domain_delayed_run_time, sequence_number,
             true),
        time_domain_now);

    return TaskHandle(this, time_domain_delayed_run_time, sequence_number);
  }
}

bool TaskQueueImpl::CancelTask(const TaskQueue::TaskHandle& handle) {
  if (!handle)
    return false;

  // If the TaskQueueManager has gone away, pretend we have canceled the task
  // because this simplifies logic in TimerBase::stop.
  if (!main_thread_only().task_queue_manager)
    return true;

#if DCHECK_IS_ON()
  DCHECK_EQ(handle.task_queue_, this);
#endif

  Task fake_task = Task::CreateFakeTaskFromHandle(handle);
  if (handle.scheduled_run_time_.is_null()) {
    // It's an immediate task.
    if (main_thread_only().immediate_work_queue->CancelTask(fake_task))
      return true;

    base::AutoLock lock(any_thread_lock_);
    return any_thread().immediate_incoming_queue.erase(fake_task) > 0;
  } else {
    // It's a delayed task.
    DelayedRunTimeQueue::iterator it =
        main_thread_only().delayed_incoming_queue.find(fake_task);
    if (it != main_thread_only().delayed_incoming_queue.end()) {
      // It's safe to remove the wakeup from the TimeDomain only if this task's
      // scheduled run time was unique within the queue.
      bool can_cancel_timedomain_wakeup = true;
      if (it != main_thread_only().delayed_incoming_queue.begin()) {
        DelayedRunTimeQueue::iterator before = it;
        before--;
        if (before->delayed_run_time == handle.scheduled_run_time_)
          can_cancel_timedomain_wakeup = false;
      }
      if (can_cancel_timedomain_wakeup) {
        DelayedRunTimeQueue::iterator after = it;
        after++;
        if (after != main_thread_only().delayed_incoming_queue.end() &&
            after->delayed_run_time == handle.scheduled_run_time_) {
          can_cancel_timedomain_wakeup = false;
        }
      }
      if (can_cancel_timedomain_wakeup) {
        main_thread_only().time_domain->CancelDelayedWork(
            this, handle.scheduled_run_time_);
      }

      main_thread_only().delayed_incoming_queue.erase(it);
      return true;
    }

    return main_thread_only().delayed_work_queue->CancelTask(fake_task);
  }
}

bool TaskQueueImpl::IsTaskPending(const TaskQueue::TaskHandle& handle) const {
  if (!handle)
    return false;

  // If the TaskQueueManager has gone away the task got cancelled.
  if (!main_thread_only().task_queue_manager)
    return false;

#if DCHECK_IS_ON()
  DCHECK_EQ(handle.task_queue_, this);
#endif

  Task fake_task = Task::CreateFakeTaskFromHandle(handle);
  if (handle.scheduled_run_time_.is_null()) {
    // It's an immediate task.
    if (main_thread_only().immediate_work_queue->IsTaskPending(fake_task))
      return true;

    base::AutoLock lock(any_thread_lock_);
    return any_thread().immediate_incoming_queue.find(fake_task) !=
           any_thread().immediate_incoming_queue.end();
  } else {
    // It's a delayed task.
    DelayedRunTimeQueue::iterator it =
        main_thread_only().delayed_incoming_queue.find(fake_task);
    if (it != main_thread_only().delayed_incoming_queue.end())
      return true;

    return main_thread_only().delayed_work_queue->IsTaskPending(fake_task);
  }
}

bool TaskQueueImpl::PostDelayedTask(const tracked_objects::Location& from_here,
                                    const base::Closure& task,
                                    base::TimeDelta delay) {
  if (delay.is_zero())
    return PostImmediateTaskImpl(from_here, task, TaskType::NORMAL);

  return PostDelayedTaskImpl(from_here, task, delay, TaskType::NORMAL);
}

bool TaskQueueImpl::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  if (delay.is_zero())
    return PostImmediateTaskImpl(from_here, task, TaskType::NON_NESTABLE);

  return PostDelayedTaskImpl(from_here, task, delay, TaskType::NON_NESTABLE);
}

bool TaskQueueImpl::PostImmediateTaskImpl(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    TaskType task_type) {
  base::AutoLock lock(any_thread_lock_);
  if (!any_thread().task_queue_manager)
    return false;

  EnqueueOrder sequence_number =
      any_thread().task_queue_manager->GetNextSequenceNumber();

  PushOntoImmediateIncomingQueueLocked(
      Task(from_here, task, base::TimeTicks(), sequence_number,
           task_type != TaskType::NON_NESTABLE, sequence_number));
  return true;
}

bool TaskQueueImpl::PostDelayedTaskImpl(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay,
    TaskType task_type) {
  DCHECK_GT(delay, base::TimeDelta());
  if (base::PlatformThread::CurrentId() == thread_id_) {
    // Lock-free fast path for delayed tasks posted from the main thread.
    if (!main_thread_only().task_queue_manager)
      return false;

    EnqueueOrder sequence_number =
        main_thread_only().task_queue_manager->GetNextSequenceNumber();

    base::TimeTicks time_domain_now = main_thread_only().time_domain->Now();
    base::TimeTicks time_domain_delayed_run_time = time_domain_now + delay;
    PushOntoDelayedIncomingQueueFromMainThread(
        Task(from_here, task, time_domain_delayed_run_time, sequence_number,
             task_type != TaskType::NON_NESTABLE),
        time_domain_now);
  } else {
    // NOTE posting a delayed task from a different thread is not expected to
    // be common. This pathway is less optimal than perhaps it could be
    // because it causes two main thread tasks to be run.  Should this
    // assumption prove to be false in future, we may need to revisit this.
    base::AutoLock lock(any_thread_lock_);
    if (!any_thread().task_queue_manager)
      return false;

    EnqueueOrder sequence_number =
        any_thread().task_queue_manager->GetNextSequenceNumber();

    base::TimeTicks time_domain_now = any_thread().time_domain->Now();
    base::TimeTicks time_domain_delayed_run_time = time_domain_now + delay;
    PushOntoDelayedIncomingQueueLocked(
        Task(from_here, task, time_domain_delayed_run_time, sequence_number,
             task_type != TaskType::NON_NESTABLE));
  }
  return true;
}

void TaskQueueImpl::PushOntoDelayedIncomingQueueFromMainThread(
    Task pending_task,
    base::TimeTicks now) {
  main_thread_only().task_queue_manager->DidQueueTask(pending_task);

  // Schedule a later call to MoveReadyDelayedTasksToDelayedWorkQueue.
  base::TimeTicks delayed_run_time = pending_task.delayed_run_time;
  main_thread_only().delayed_incoming_queue.insert(std::move(pending_task));
  main_thread_only().time_domain->ScheduleDelayedWork(this, delayed_run_time,
                                                      now);
  TraceQueueSize(false);
}

void TaskQueueImpl::PushOntoDelayedIncomingQueueLocked(Task pending_task) {
  any_thread().task_queue_manager->DidQueueTask(pending_task);

  int thread_hop_task_sequence_number =
      any_thread().task_queue_manager->GetNextSequenceNumber();
  PushOntoImmediateIncomingQueueLocked(
      Task(FROM_HERE, base::Bind(&TaskQueueImpl::ScheduleDelayedWorkTask, this,
                                 base::Passed(&pending_task)),
           base::TimeTicks(), thread_hop_task_sequence_number, false,
           thread_hop_task_sequence_number));
}

void TaskQueueImpl::PushOntoImmediateIncomingQueueLocked(Task pending_task) {
  if (any_thread().immediate_incoming_queue.empty())
    any_thread().time_domain->RegisterAsUpdatableTaskQueue(this);
  if (any_thread().pump_policy == PumpPolicy::AUTO &&
      any_thread().immediate_incoming_queue.empty()) {
    any_thread().task_queue_manager->MaybeScheduleImmediateWork(FROM_HERE);
  }
  any_thread().task_queue_manager->DidQueueTask(pending_task);
  // We expect |pending_task| to be inserted at the end. Amoritized O(1).
  any_thread().immediate_incoming_queue.insert(
      any_thread().immediate_incoming_queue.end(),
      std::move(pending_task));
  DCHECK_EQ(pending_task.enqueue_order(),
            any_thread().immediate_incoming_queue.rbegin()->enqueue_order());
  TraceQueueSize(true);
}

void TaskQueueImpl::ScheduleDelayedWorkTask(Task pending_task) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  base::TimeTicks delayed_run_time = pending_task.delayed_run_time;
  base::TimeTicks time_domain_now = main_thread_only().time_domain->Now();
  // Make sure |delayed_run_time| isn't in the past.
  if (delayed_run_time < time_domain_now) {
    delayed_run_time = time_domain_now;
    pending_task.delayed_run_time = time_domain_now;
    main_thread_only().delayed_incoming_queue.insert(std::move(pending_task));
    LazyNow lazy_now(time_domain_now);
    MoveReadyDelayedTasksToDelayedWorkQueue(&lazy_now);
  } else {
    main_thread_only().delayed_incoming_queue.insert(std::move(pending_task));
    main_thread_only().time_domain->ScheduleDelayedWork(
        this, delayed_run_time, main_thread_only().time_domain->Now());
  }
  TraceQueueSize(false);
}

void TaskQueueImpl::SetQueueEnabled(bool enabled) {
  if (main_thread_only().is_enabled == enabled)
    return;
  main_thread_only().is_enabled = enabled;
  if (!main_thread_only().task_queue_manager)
    return;
  if (enabled) {
    main_thread_only().task_queue_manager->selector_.EnableQueue(this);
  } else {
    main_thread_only().task_queue_manager->selector_.DisableQueue(this);
  }
}

bool TaskQueueImpl::IsQueueEnabled() const {
  return main_thread_only().is_enabled;
}

bool TaskQueueImpl::IsEmpty() const {
  if (!main_thread_only().delayed_work_queue->Empty() ||
      !main_thread_only().immediate_work_queue->Empty()) {
    return false;
  }

  base::AutoLock lock(any_thread_lock_);
  return any_thread().immediate_incoming_queue.empty() &&
         main_thread_only().delayed_incoming_queue.empty();
}

bool TaskQueueImpl::HasPendingImmediateWork() const {
  if (!main_thread_only().delayed_work_queue->Empty() ||
      !main_thread_only().immediate_work_queue->Empty()) {
    return true;
  }

  return NeedsPumping();
}

bool TaskQueueImpl::NeedsPumping() const {
  if (!main_thread_only().immediate_work_queue->Empty())
    return false;

  base::AutoLock lock(any_thread_lock_);
  if (!any_thread().immediate_incoming_queue.empty())
    return true;

  // If there's no immediate Incoming work then we only need pumping if there
  // is a delayed task that should be running now.
  if (main_thread_only().delayed_incoming_queue.empty())
    return false;

  return main_thread_only().delayed_incoming_queue.begin()->delayed_run_time <=
         main_thread_only().time_domain->CreateLazyNow().Now();
}

bool TaskQueueImpl::TaskIsOlderThanQueuedImmediateTasksLocked(
    const Task* task) {
  // A null task is passed when UpdateQueue is called before any task is run.
  // In this case we don't want to pump an after_wakeup queue, so return true
  // here.
  if (!task)
    return true;

  // Return false if task is newer than the oldest immediate task.
  if (!any_thread().immediate_incoming_queue.empty() &&
      task->enqueue_order() >
          any_thread().immediate_incoming_queue.begin()->enqueue_order()) {
    return false;
  }
  return true;
}

bool TaskQueueImpl::TaskIsOlderThanQueuedDelayedTasks(const Task* task) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // A null task is passed when UpdateQueue is called before any task is run.
  // In this case we don't want to pump an after_wakeup queue, so return true
  // here.
  if (!task)
    return true;

  EnqueueOrder enqueue_order;
  if (!main_thread_only().delayed_work_queue->GetFrontTaskEnqueueOrder(
          &enqueue_order)) {
    return true;
  }

  return task->enqueue_order() < enqueue_order;
}

bool TaskQueueImpl::ShouldAutoPumpImmediateQueueLocked(
    bool should_trigger_wakeup,
    const Task* previous_task) {
  if (main_thread_only().pump_policy == PumpPolicy::MANUAL)
    return false;
  if (main_thread_only().pump_policy == PumpPolicy::AFTER_WAKEUP &&
      (!should_trigger_wakeup ||
       TaskIsOlderThanQueuedImmediateTasksLocked(previous_task)))
    return false;
  return true;
}

bool TaskQueueImpl::ShouldAutoPumpDelayedQueue(bool should_trigger_wakeup,
                                               const Task* previous_task) {
  if (main_thread_only().pump_policy == PumpPolicy::MANUAL)
    return false;
  if (main_thread_only().pump_policy == PumpPolicy::AFTER_WAKEUP &&
      (!should_trigger_wakeup ||
       TaskIsOlderThanQueuedDelayedTasks(previous_task)))
    return false;
  return true;
}

void TaskQueueImpl::MoveReadyDelayedTasksToDelayedWorkQueue(LazyNow* lazy_now) {
  // Enqueue all delayed tasks that should be running now.
  while (!main_thread_only().delayed_incoming_queue.empty()) {
    DelayedRunTimeQueue::iterator next_task =
        main_thread_only().delayed_incoming_queue.begin();
    if (next_task->delayed_run_time > lazy_now->Now())
      break;
    // TODO(alexclarke): Use extract() when C++17 is allowed.
    Task& task = const_cast<Task&>(*next_task);
    task.set_enqueue_order(
        main_thread_only().task_queue_manager->GetNextSequenceNumber());
    main_thread_only().delayed_work_queue->Push(std::move(task));
    main_thread_only().delayed_incoming_queue.erase(next_task);
  }
}

void TaskQueueImpl::UpdateDelayedWorkQueue(LazyNow* lazy_now,
                                           bool should_trigger_wakeup,
                                           const Task* previous_task) {
  if (!main_thread_only().task_queue_manager)
    return;
  if (!ShouldAutoPumpDelayedQueue(should_trigger_wakeup, previous_task))
    return;
  MoveReadyDelayedTasksToDelayedWorkQueue(lazy_now);
  TraceQueueSize(false);
}

void TaskQueueImpl::UpdateImmediateWorkQueue(bool should_trigger_wakeup,
                                             const Task* previous_task) {
  DCHECK(main_thread_only().immediate_work_queue->Empty());
  base::AutoLock lock(any_thread_lock_);
  if (!main_thread_only().task_queue_manager)
    return;
  if (!ShouldAutoPumpImmediateQueueLocked(should_trigger_wakeup, previous_task))
    return;

  main_thread_only().immediate_work_queue->SwapLocked(
      any_thread().immediate_incoming_queue);

  // |any_thread().immediate_incoming_queue| is now empty so
  // TimeDomain::UpdateQueues no longer needs to consider this queue for
  // reloading.
  main_thread_only().time_domain->UnregisterAsUpdatableTaskQueue(this);
}

void TaskQueueImpl::TraceQueueSize(bool is_locked) const {
  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(disabled_by_default_tracing_category_,
                                     &is_tracing);
  if (!is_tracing)
    return;

  // It's only safe to access the work queues from the main thread.
  // TODO(alexclarke): We should find another way of tracing this
  if (base::PlatformThread::CurrentId() != thread_id_)
    return;

  if (!is_locked)
    any_thread_lock_.Acquire();
  else
    any_thread_lock_.AssertAcquired();
  TRACE_COUNTER1(disabled_by_default_tracing_category_, GetName(),
                 any_thread().immediate_incoming_queue.size() +
                     main_thread_only().immediate_work_queue->Size() +
                     main_thread_only().delayed_work_queue->Size() +
                     main_thread_only().delayed_incoming_queue.size());
  if (!is_locked)
    any_thread_lock_.Release();
}

void TaskQueueImpl::SetPumpPolicy(PumpPolicy pump_policy) {
  base::AutoLock lock(any_thread_lock_);
  if (pump_policy == PumpPolicy::AUTO &&
      any_thread().pump_policy != PumpPolicy::AUTO) {
    LazyNow lazy_now(main_thread_only().time_domain->CreateLazyNow());
    PumpQueueLocked(&lazy_now, true);
  }
  any_thread().pump_policy = pump_policy;
  main_thread_only().pump_policy = pump_policy;
}

TaskQueue::PumpPolicy TaskQueueImpl::GetPumpPolicy() const {
  return main_thread_only().pump_policy;
}

void TaskQueueImpl::PumpQueueLocked(LazyNow* lazy_now, bool may_post_dowork) {
  TRACE_EVENT1(disabled_by_default_tracing_category_,
               "TaskQueueImpl::PumpQueueLocked", "queue", name_);
  TaskQueueManager* task_queue_manager = any_thread().task_queue_manager;
  if (!task_queue_manager)
    return;

  MoveReadyDelayedTasksToDelayedWorkQueue(lazy_now);

  while (!any_thread().immediate_incoming_queue.empty()) {
    ComparatorQueue::iterator it =
        any_thread().immediate_incoming_queue.begin();
    main_thread_only().immediate_work_queue->Push(
        std::move(const_cast<Task&>(*it)));
    any_thread().immediate_incoming_queue.erase(it);
  }

  // |immediate_incoming_queue| is now empty so TimeDomain::UpdateQueues no
  // longer needs to consider this queue for reloading.
  main_thread_only().time_domain->UnregisterAsUpdatableTaskQueue(this);

  if (main_thread_only().immediate_work_queue->Empty() &&
      main_thread_only().delayed_work_queue->Empty()) {
    return;
  }

  if (may_post_dowork)
    task_queue_manager->MaybeScheduleImmediateWork(FROM_HERE);
}

void TaskQueueImpl::PumpQueue(LazyNow* lazy_now, bool may_post_dowork) {
  base::AutoLock lock(any_thread_lock_);
  PumpQueueLocked(lazy_now, may_post_dowork);
}

const char* TaskQueueImpl::GetName() const {
  return name_;
}

void TaskQueueImpl::SetQueuePriority(QueuePriority priority) {
  if (!main_thread_only().task_queue_manager || priority == GetQueuePriority())
    return;
  main_thread_only().task_queue_manager->selector_.SetQueuePriority(this,
                                                                    priority);
}

TaskQueueImpl::QueuePriority TaskQueueImpl::GetQueuePriority() const {
  size_t set_index = immediate_work_queue()->work_queue_set_index();
  DCHECK_EQ(set_index, delayed_work_queue()->work_queue_set_index());
  return static_cast<TaskQueue::QueuePriority>(set_index);
}

// static
const char* TaskQueueImpl::PumpPolicyToString(
    TaskQueue::PumpPolicy pump_policy) {
  switch (pump_policy) {
    case TaskQueue::PumpPolicy::AUTO:
      return "auto";
    case TaskQueue::PumpPolicy::AFTER_WAKEUP:
      return "after_wakeup";
    case TaskQueue::PumpPolicy::MANUAL:
      return "manual";
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
const char* TaskQueueImpl::WakeupPolicyToString(
    TaskQueue::WakeupPolicy wakeup_policy) {
  switch (wakeup_policy) {
    case TaskQueue::WakeupPolicy::CAN_WAKE_OTHER_QUEUES:
      return "can_wake_other_queues";
    case TaskQueue::WakeupPolicy::DONT_WAKE_OTHER_QUEUES:
      return "dont_wake_other_queues";
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
const char* TaskQueueImpl::PriorityToString(QueuePriority priority) {
  switch (priority) {
    case CONTROL_PRIORITY:
      return "control";
    case HIGH_PRIORITY:
      return "high";
    case NORMAL_PRIORITY:
      return "normal";
    case BEST_EFFORT_PRIORITY:
      return "best_effort";
    default:
      NOTREACHED();
      return nullptr;
  }
}

void TaskQueueImpl::AsValueInto(base::trace_event::TracedValue* state) const {
  base::AutoLock lock(any_thread_lock_);
  state->BeginDictionary();
  state->SetString("name", GetName());
  state->SetBoolean("enabled", main_thread_only().is_enabled);
  state->SetString("time_domain_name",
                   main_thread_only().time_domain->GetName());
  state->SetString("pump_policy", PumpPolicyToString(any_thread().pump_policy));
  state->SetString("wakeup_policy", WakeupPolicyToString(wakeup_policy_));
  bool verbose_tracing_enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      disabled_by_default_verbose_tracing_category_, &verbose_tracing_enabled);
  state->SetInteger("immediate_incoming_queue_size",
                    any_thread().immediate_incoming_queue.size());
  state->SetInteger("delayed_incoming_queue_size",
                    main_thread_only().delayed_incoming_queue.size());
  state->SetInteger("immediate_work_queue_size",
                    main_thread_only().immediate_work_queue->Size());
  state->SetInteger("delayed_work_queue_size",
                    main_thread_only().delayed_work_queue->Size());
  if (!main_thread_only().delayed_incoming_queue.empty()) {
    base::TimeDelta delay_to_next_task =
        (main_thread_only().delayed_incoming_queue.begin()->delayed_run_time -
         main_thread_only().time_domain->CreateLazyNow().Now());
    state->SetDouble("delay_to_next_task_ms",
                     delay_to_next_task.InMillisecondsF());
  }
  if (verbose_tracing_enabled) {
    state->BeginArray("immediate_incoming_queue");
    QueueAsValueInto(any_thread().immediate_incoming_queue, state);
    state->EndArray();
    state->BeginArray("delayed_work_queue");
    main_thread_only().delayed_work_queue->AsValueInto(state);
    state->EndArray();
    state->BeginArray("immediate_work_queue");
    main_thread_only().immediate_work_queue->AsValueInto(state);
    state->EndArray();
    state->BeginArray("delayed_incoming_queue");
    QueueAsValueInto(main_thread_only().delayed_incoming_queue, state);
    state->EndArray();
  }
  state->SetString("priority", PriorityToString(GetQueuePriority()));
  state->EndDictionary();
}

void TaskQueueImpl::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  main_thread_only().task_observers.AddObserver(task_observer);
}

void TaskQueueImpl::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  main_thread_only().task_observers.RemoveObserver(task_observer);
}

void TaskQueueImpl::NotifyWillProcessTask(
    const base::PendingTask& pending_task) {
  DCHECK(should_notify_observers_);
  if (main_thread_only().blame_context)
    main_thread_only().blame_context->Enter();
  FOR_EACH_OBSERVER(base::MessageLoop::TaskObserver,
                    main_thread_only().task_observers,
                    WillProcessTask(pending_task));
}

void TaskQueueImpl::NotifyDidProcessTask(
    const base::PendingTask& pending_task) {
  DCHECK(should_notify_observers_);
  FOR_EACH_OBSERVER(base::MessageLoop::TaskObserver,
                    main_thread_only().task_observers,
                    DidProcessTask(pending_task));
  if (main_thread_only().blame_context)
    main_thread_only().blame_context->Leave();
}

void TaskQueueImpl::SetTimeDomain(TimeDomain* time_domain) {
  base::AutoLock lock(any_thread_lock_);
  DCHECK(time_domain);
  // NOTE this is similar to checking |any_thread().task_queue_manager| but the
  // TaskQueueSelectorTests constructs TaskQueueImpl directly with a null
  // task_queue_manager.  Instead we check |any_thread().time_domain| which is
  // another way of asserting that UnregisterTaskQueue has not been called.
  DCHECK(any_thread().time_domain);
  if (!any_thread().time_domain)
    return;
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (time_domain == main_thread_only().time_domain)
    return;

  main_thread_only().time_domain->MigrateQueue(this, time_domain);
  main_thread_only().time_domain = time_domain;
  any_thread().time_domain = time_domain;
}

TimeDomain* TaskQueueImpl::GetTimeDomain() const {
  if (base::PlatformThread::CurrentId() == thread_id_)
    return main_thread_only().time_domain;

  base::AutoLock lock(any_thread_lock_);
  return any_thread().time_domain;
}

void TaskQueueImpl::SetBlameContext(
    base::trace_event::BlameContext* blame_context) {
  main_thread_only().blame_context = blame_context;
}

// static
void TaskQueueImpl::QueueAsValueInto(const ComparatorQueue& queue,
                                     base::trace_event::TracedValue* state) {
  for (const Task& task : queue) {
    TaskAsValueInto(task, state);
  }
}

// static
void TaskQueueImpl::QueueAsValueInto(const DelayedRunTimeQueue& queue,
                                     base::trace_event::TracedValue* state) {
  for (const Task& task : queue) {
    TaskAsValueInto(task, state);
  }
}

// static
void TaskQueueImpl::TaskAsValueInto(const Task& task,
                                    base::trace_event::TracedValue* state) {
  state->BeginDictionary();
  state->SetString("posted_from", task.posted_from.ToString());
#ifndef NDEBUG
  if (task.enqueue_order_set())
    state->SetInteger("enqueue_order", task.enqueue_order());
#else
  state->SetInteger("enqueue_order", task.enqueue_order());
#endif
  state->SetInteger("sequence_num", task.sequence_num);
  state->SetBoolean("nestable", task.nestable);
  state->SetBoolean("is_high_res", task.is_high_res);
  state->SetDouble(
      "delayed_run_time",
      (task.delayed_run_time - base::TimeTicks()).InMicroseconds() / 1000.0L);
  state->EndDictionary();
}

}  // namespace internal
}  // namespace scheduler
}  // namespace blink
