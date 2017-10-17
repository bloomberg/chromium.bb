// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/task_queue_impl.h"

#include <memory>
#include <utility>

#include "base/time/time.h"
#include "base/trace_event/blame_context.h"
#include "platform/scheduler/base/task_queue_manager.h"
#include "platform/scheduler/base/task_queue_manager_delegate.h"
#include "platform/scheduler/base/time_domain.h"
#include "platform/scheduler/base/work_queue.h"
#include "platform/scheduler/util/tracing_helper.h"
#include "platform/wtf/debug/CrashLogging.h"

namespace blink {
namespace scheduler {

// static
const char* TaskQueue::PriorityToString(TaskQueue::QueuePriority priority) {
  switch (priority) {
    case CONTROL_PRIORITY:
      return "control";
    case HIGH_PRIORITY:
      return "high";
    case NORMAL_PRIORITY:
      return "normal";
    case LOW_PRIORITY:
      return "low";
    case BEST_EFFORT_PRIORITY:
      return "best_effort";
    default:
      NOTREACHED();
      return nullptr;
  }
}

namespace internal {

TaskQueueImpl::TaskQueueImpl(TaskQueueManager* task_queue_manager,
                             TimeDomain* time_domain,
                             const TaskQueue::Spec& spec)
    : name_(spec.name),
      thread_id_(base::PlatformThread::CurrentId()),
      any_thread_(task_queue_manager, time_domain),
      main_thread_only_(task_queue_manager, this, time_domain),
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
    : TaskQueue::Task(TaskQueue::PostedTask(base::Closure(), base::Location()),
                      base::TimeTicks()),
#ifndef NDEBUG
      enqueue_order_set_(false),
#endif
      enqueue_order_(0) {
  sequence_num = 0;
}

TaskQueueImpl::Task::Task(TaskQueue::PostedTask task,
                          base::TimeTicks desired_run_time,
                          EnqueueOrder sequence_number)
    : TaskQueue::Task(std::move(task), desired_run_time),
#ifndef NDEBUG
      enqueue_order_set_(false),
#endif
      enqueue_order_(0) {
  sequence_num = sequence_number;
}

TaskQueueImpl::Task::Task(TaskQueue::PostedTask task,
                          base::TimeTicks desired_run_time,
                          EnqueueOrder sequence_number,
                          EnqueueOrder enqueue_order)
    : TaskQueue::Task(std::move(task), desired_run_time),
#ifndef NDEBUG
      enqueue_order_set_(true),
#endif
      enqueue_order_(enqueue_order) {
  sequence_num = sequence_number;
}

TaskQueueImpl::AnyThread::AnyThread(TaskQueueManager* task_queue_manager,
                                    TimeDomain* time_domain)
    : task_queue_manager(task_queue_manager), time_domain(time_domain) {}

TaskQueueImpl::AnyThread::~AnyThread() {}

TaskQueueImpl::MainThreadOnly::MainThreadOnly(
    TaskQueueManager* task_queue_manager,
    TaskQueueImpl* task_queue,
    TimeDomain* time_domain)
    : task_queue_manager(task_queue_manager),
      time_domain(time_domain),
      delayed_work_queue(
          new WorkQueue(task_queue, "delayed", WorkQueue::QueueType::DELAYED)),
      immediate_work_queue(new WorkQueue(task_queue,
                                         "immediate",
                                         WorkQueue::QueueType::IMMEDIATE)),
      set_index(0),
      is_enabled_refcount(0),
      voter_refcount(0),
      blame_context(nullptr),
      current_fence(0),
      is_enabled_for_test(true) {}

TaskQueueImpl::MainThreadOnly::~MainThreadOnly() {}

void TaskQueueImpl::UnregisterTaskQueue() {
  base::AutoLock lock(any_thread_lock_);
  base::AutoLock immediate_incoming_queue_lock(immediate_incoming_queue_lock_);
  if (main_thread_only().time_domain)
    main_thread_only().time_domain->UnregisterQueue(this);

  if (!any_thread().task_queue_manager)
    return;

  main_thread_only().on_task_completed_handler = OnTaskCompletedHandler();
  any_thread().time_domain = nullptr;
  main_thread_only().time_domain = nullptr;

  any_thread().task_queue_manager = nullptr;
  main_thread_only().task_queue_manager = nullptr;
  any_thread().on_next_wake_up_changed_callback = OnNextWakeUpChangedCallback();
  main_thread_only().on_next_wake_up_changed_callback =
      OnNextWakeUpChangedCallback();
  main_thread_only().delayed_incoming_queue = std::priority_queue<Task>();
  immediate_incoming_queue().clear();
  main_thread_only().immediate_work_queue.reset();
  main_thread_only().delayed_work_queue.reset();
}

const char* TaskQueueImpl::GetName() const {
  return name_;
}

bool TaskQueueImpl::RunsTasksInCurrentSequence() const {
  return base::PlatformThread::CurrentId() == thread_id_;
}

bool TaskQueueImpl::PostDelayedTask(TaskQueue::PostedTask task) {
  if (task.delay.is_zero())
    return PostImmediateTaskImpl(std::move(task));

  return PostDelayedTaskImpl(std::move(task));
}

bool TaskQueueImpl::PostImmediateTaskImpl(TaskQueue::PostedTask task) {
  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.callback);
  base::AutoLock lock(any_thread_lock_);
  if (!any_thread().task_queue_manager)
    return false;

  EnqueueOrder sequence_number =
      any_thread().task_queue_manager->GetNextSequenceNumber();

  PushOntoImmediateIncomingQueueLocked(Task(std::move(task),
                                            any_thread().time_domain->Now(),
                                            sequence_number, sequence_number));
  return true;
}

bool TaskQueueImpl::PostDelayedTaskImpl(TaskQueue::PostedTask task) {
  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.callback);
  DCHECK_GT(task.delay, base::TimeDelta());
  if (base::PlatformThread::CurrentId() == thread_id_) {
    // Lock-free fast path for delayed tasks posted from the main thread.
    if (!main_thread_only().task_queue_manager)
      return false;

    EnqueueOrder sequence_number =
        main_thread_only().task_queue_manager->GetNextSequenceNumber();

    base::TimeTicks time_domain_now = main_thread_only().time_domain->Now();
    base::TimeTicks time_domain_delayed_run_time = time_domain_now + task.delay;
    PushOntoDelayedIncomingQueueFromMainThread(
        Task(std::move(task), time_domain_delayed_run_time, sequence_number),
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
    base::TimeTicks time_domain_delayed_run_time = time_domain_now + task.delay;
    PushOntoDelayedIncomingQueueLocked(
        Task(std::move(task), time_domain_delayed_run_time, sequence_number));
  }
  return true;
}

void TaskQueueImpl::PushOntoDelayedIncomingQueueFromMainThread(
    Task pending_task, base::TimeTicks now) {
  DelayedWakeUp wake_up = pending_task.delayed_wake_up();
  main_thread_only().task_queue_manager->DidQueueTask(pending_task);
  main_thread_only().delayed_incoming_queue.push(std::move(pending_task));

  // If |pending_task| is at the head of the queue, then make sure a wake-up
  // is requested if the queue is enabled.  Note we still want to schedule a
  // wake-up even if blocked by a fence, because we'd break throttling logic
  // otherwise.
  DelayedWakeUp new_wake_up =
      main_thread_only().delayed_incoming_queue.top().delayed_wake_up();
  if (wake_up.time == new_wake_up.time &&
      wake_up.sequence_num == new_wake_up.sequence_num) {
    ScheduleDelayedWorkInTimeDomain(now);
  }

  TraceQueueSize();
}

void TaskQueueImpl::PushOntoDelayedIncomingQueueLocked(Task pending_task) {
  any_thread().task_queue_manager->DidQueueTask(pending_task);

  int thread_hop_task_sequence_number =
      any_thread().task_queue_manager->GetNextSequenceNumber();
  // TODO(altimin): Add a copy method to Task to capture metadata here.
  PushOntoImmediateIncomingQueueLocked(
      Task(TaskQueue::PostedTask(
               base::Bind(&TaskQueueImpl::ScheduleDelayedWorkTask,
                          base::Unretained(this), base::Passed(&pending_task)),
               FROM_HERE, base::TimeDelta(), base::Nestable::kNonNestable),
           base::TimeTicks(), thread_hop_task_sequence_number,
           thread_hop_task_sequence_number));
}

void TaskQueueImpl::ScheduleDelayedWorkTask(Task pending_task) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  base::TimeTicks delayed_run_time = pending_task.delayed_run_time;
  base::TimeTicks time_domain_now = main_thread_only().time_domain->Now();
  if (delayed_run_time <= time_domain_now) {
    // If |delayed_run_time| is in the past then push it onto the work queue
    // immediately. To ensure the right task ordering we need to temporarily
    // push it onto the |delayed_incoming_queue|.
    delayed_run_time = time_domain_now;
    pending_task.delayed_run_time = time_domain_now;
    main_thread_only().delayed_incoming_queue.push(std::move(pending_task));
    LazyNow lazy_now(time_domain_now);
    WakeUpForDelayedWork(&lazy_now);
  } else {
    // If |delayed_run_time| is in the future we can queue it as normal.
    PushOntoDelayedIncomingQueueFromMainThread(std::move(pending_task),
                                               time_domain_now);
  }
  TraceQueueSize();
}

void TaskQueueImpl::PushOntoImmediateIncomingQueueLocked(Task task) {
  // If the |immediate_incoming_queue| is empty we need a DoWork posted to make
  // it run.
  bool was_immediate_incoming_queue_empty;

  EnqueueOrder sequence_number = task.sequence_num;
  base::TimeTicks desired_run_time = task.delayed_run_time;

  {
    base::AutoLock lock(immediate_incoming_queue_lock_);
    was_immediate_incoming_queue_empty = immediate_incoming_queue().empty();
    immediate_incoming_queue().push_back(std::move(task));
    any_thread().task_queue_manager->DidQueueTask(
        immediate_incoming_queue().back());
  }

  if (was_immediate_incoming_queue_empty) {
    // However there's no point posting a DoWork for a blocked queue. NB we can
    // only tell if it's disabled from the main thread.
    bool queue_is_blocked =
        RunsTasksInCurrentSequence() &&
        (!IsQueueEnabled() || main_thread_only().current_fence);
    any_thread().task_queue_manager->OnQueueHasIncomingImmediateWork(
        this, sequence_number, queue_is_blocked);
    if (!any_thread().on_next_wake_up_changed_callback.is_null())
      any_thread().on_next_wake_up_changed_callback.Run(desired_run_time);
  }

  TraceQueueSize();
}

void TaskQueueImpl::ReloadImmediateWorkQueueIfEmpty() {
  if (!main_thread_only().immediate_work_queue->Empty())
    return;

  main_thread_only().immediate_work_queue->ReloadEmptyImmediateQueue();
}

TaskQueueImpl::TaskDeque TaskQueueImpl::TakeImmediateIncomingQueue() {
  base::AutoLock immediate_incoming_queue_lock(immediate_incoming_queue_lock_);
  TaskQueueImpl::TaskDeque queue;
  queue.Swap(immediate_incoming_queue());

  // Activate delayed fence if necessary. This is ideologically similar to
  // ActivateDelayedFenceIfNeeded, but due to immediate tasks being posted
  // from any thread we can't generate an enqueue order for the fence there,
  // so we have to check all immediate tasks and use their enqueue order for
  // a fence.
  if (main_thread_only().delayed_fence) {
    for (const Task& task : queue) {
      if (task.delayed_run_time >= main_thread_only().delayed_fence.value()) {
        main_thread_only().delayed_fence = base::nullopt;
        DCHECK_EQ(main_thread_only().current_fence,
                  static_cast<EnqueueOrder>(EnqueueOrderValues::NONE));
        bool task_unblocked = InsertFenceImpl(task.enqueue_order());
        DCHECK(!task_unblocked)
            << "Activating a delayed fence shouldn't unblock new work";
        break;
      }
    }
  }

  return queue;
}

bool TaskQueueImpl::IsEmpty() const {
  if (!main_thread_only().delayed_work_queue->Empty() ||
      !main_thread_only().delayed_incoming_queue.empty() ||
      !main_thread_only().immediate_work_queue->Empty()) {
    return false;
  }

  base::AutoLock lock(immediate_incoming_queue_lock_);
  return immediate_incoming_queue().empty();
}

size_t TaskQueueImpl::GetNumberOfPendingTasks() const {
  size_t task_count = 0;
  task_count += main_thread_only().delayed_work_queue->Size();
  task_count += main_thread_only().delayed_incoming_queue.size();
  task_count += main_thread_only().immediate_work_queue->Size();

  base::AutoLock lock(immediate_incoming_queue_lock_);
  task_count += immediate_incoming_queue().size();
  return task_count;
}

bool TaskQueueImpl::HasTaskToRunImmediately() const {
  // Any work queue tasks count as immediate work.
  if (!main_thread_only().delayed_work_queue->Empty() ||
      !main_thread_only().immediate_work_queue->Empty()) {
    return true;
  }

  // Tasks on |delayed_incoming_queue| that could run now, count as
  // immediate work.
  if (!main_thread_only().delayed_incoming_queue.empty() &&
      main_thread_only().delayed_incoming_queue.top().delayed_run_time <=
          main_thread_only().time_domain->CreateLazyNow().Now()) {
    return true;
  }

  // Finally tasks on |immediate_incoming_queue| count as immediate work.
  base::AutoLock lock(immediate_incoming_queue_lock_);
  return !immediate_incoming_queue().empty();
}

base::Optional<base::TimeTicks> TaskQueueImpl::GetNextScheduledWakeUp() {
  // Note we don't scheduled a wake-up for disabled queues.
  if (main_thread_only().delayed_incoming_queue.empty() || !IsQueueEnabled())
    return base::nullopt;

  return main_thread_only().delayed_incoming_queue.top().delayed_run_time;
}

base::Optional<TaskQueueImpl::DelayedWakeUp>
TaskQueueImpl::WakeUpForDelayedWork(LazyNow* lazy_now) {
  // Enqueue all delayed tasks that should be running now, skipping any that
  // have been canceled.
  while (!main_thread_only().delayed_incoming_queue.empty()) {
    Task& task =
        const_cast<Task&>(main_thread_only().delayed_incoming_queue.top());
    if (task.task.IsCancelled()) {
      main_thread_only().delayed_incoming_queue.pop();
      continue;
    }
    if (task.delayed_run_time > lazy_now->Now())
      break;
    ActivateDelayedFenceIfNeeded(task.delayed_run_time);
    task.set_enqueue_order(
        main_thread_only().task_queue_manager->GetNextSequenceNumber());
    main_thread_only().delayed_work_queue->Push(std::move(task));
    main_thread_only().delayed_incoming_queue.pop();
  }

  // Make sure the next wake up is scheduled.
  if (!main_thread_only().delayed_incoming_queue.empty()) {
    return main_thread_only().delayed_incoming_queue.top().delayed_wake_up();
  }

  return base::nullopt;
}

void TaskQueueImpl::TraceQueueSize() const {
  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), &is_tracing);
  if (!is_tracing)
    return;

  // It's only safe to access the work queues from the main thread.
  // TODO(alexclarke): We should find another way of tracing this
  if (base::PlatformThread::CurrentId() != thread_id_)
    return;

  base::AutoLock lock(immediate_incoming_queue_lock_);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), GetName(),
                 immediate_incoming_queue().size() +
                     main_thread_only().immediate_work_queue->Size() +
                     main_thread_only().delayed_work_queue->Size() +
                     main_thread_only().delayed_incoming_queue.size());
}

void TaskQueueImpl::SetQueuePriority(TaskQueue::QueuePriority priority) {
  if (!main_thread_only().task_queue_manager || priority == GetQueuePriority())
    return;
  main_thread_only().task_queue_manager->selector_.SetQueuePriority(this,
                                                                    priority);
}

TaskQueue::QueuePriority TaskQueueImpl::GetQueuePriority() const {
  size_t set_index = immediate_work_queue()->work_queue_set_index();
  DCHECK_EQ(set_index, delayed_work_queue()->work_queue_set_index());
  return static_cast<TaskQueue::QueuePriority>(set_index);
}

void TaskQueueImpl::AsValueInto(base::TimeTicks now,
                                base::trace_event::TracedValue* state) const {
  base::AutoLock lock(any_thread_lock_);
  base::AutoLock immediate_incoming_queue_lock(immediate_incoming_queue_lock_);
  state->BeginDictionary();
  state->SetString("name", GetName());
  if (!main_thread_only().task_queue_manager) {
    state->SetBoolean("unregistered", true);
    state->EndDictionary();
    return;
  }
  DCHECK(main_thread_only().time_domain);
  DCHECK(main_thread_only().delayed_work_queue);
  DCHECK(main_thread_only().immediate_work_queue);

  state->SetString("task_queue_id", PointerToString(this));
  state->SetBoolean("enabled", IsQueueEnabled());
  state->SetString("time_domain_name",
                   main_thread_only().time_domain->GetName());
  state->SetInteger("immediate_incoming_queue_size",
                    immediate_incoming_queue().size());
  state->SetInteger("delayed_incoming_queue_size",
                    main_thread_only().delayed_incoming_queue.size());
  state->SetInteger("immediate_work_queue_size",
                    main_thread_only().immediate_work_queue->Size());
  state->SetInteger("delayed_work_queue_size",
                    main_thread_only().delayed_work_queue->Size());
  if (!main_thread_only().delayed_incoming_queue.empty()) {
    base::TimeDelta delay_to_next_task =
        (main_thread_only().delayed_incoming_queue.top().delayed_run_time -
         main_thread_only().time_domain->CreateLazyNow().Now());
    state->SetDouble("delay_to_next_task_ms",
                     delay_to_next_task.InMillisecondsF());
  }
  if (main_thread_only().current_fence)
    state->SetInteger("current_fence", main_thread_only().current_fence);
  if (main_thread_only().delayed_fence) {
    state->SetDouble(
        "delayed_fence_seconds_from_now",
        (main_thread_only().delayed_fence.value() - now).InSecondsF());
  }
  if (AreVerboseSnapshotsEnabled()) {
    state->BeginArray("immediate_incoming_queue");
    QueueAsValueInto(immediate_incoming_queue(), now, state);
    state->EndArray();
    state->BeginArray("delayed_work_queue");
    main_thread_only().delayed_work_queue->AsValueInto(now, state);
    state->EndArray();
    state->BeginArray("immediate_work_queue");
    main_thread_only().immediate_work_queue->AsValueInto(now, state);
    state->EndArray();
    state->BeginArray("delayed_incoming_queue");
    QueueAsValueInto(main_thread_only().delayed_incoming_queue, now, state);
    state->EndArray();
  }
  state->SetString("priority", TaskQueue::PriorityToString(GetQueuePriority()));
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
  for (auto& observer : main_thread_only().task_observers)
    observer.WillProcessTask(pending_task);
}

void TaskQueueImpl::NotifyDidProcessTask(
    const base::PendingTask& pending_task) {
  DCHECK(should_notify_observers_);
  for (auto& observer : main_thread_only().task_observers)
    observer.DidProcessTask(pending_task);
  if (main_thread_only().blame_context)
    main_thread_only().blame_context->Leave();
}

void TaskQueueImpl::SetTimeDomain(TimeDomain* time_domain) {
  {
    base::AutoLock lock(any_thread_lock_);
    DCHECK(time_domain);
    // NOTE this is similar to checking |any_thread().task_queue_manager| but
    // the TaskQueueSelectorTests constructs TaskQueueImpl directly with a null
    // task_queue_manager.  Instead we check |any_thread().time_domain| which is
    // another way of asserting that UnregisterTaskQueue has not been called.
    DCHECK(any_thread().time_domain);
    if (!any_thread().time_domain)
      return;
    DCHECK(main_thread_checker_.CalledOnValidThread());
    if (time_domain == main_thread_only().time_domain)
      return;

    any_thread().time_domain = time_domain;
  }

  main_thread_only().time_domain->UnregisterQueue(this);
  main_thread_only().time_domain = time_domain;
  time_domain->RegisterQueue(this);

  ScheduleDelayedWorkInTimeDomain(time_domain->Now());
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

void TaskQueueImpl::InsertFence(TaskQueue::InsertFencePosition position) {
  if (!main_thread_only().task_queue_manager)
    return;

  // Only one fence may be present at a time.
  main_thread_only().delayed_fence = base::nullopt;

  EnqueueOrder previous_fence = main_thread_only().current_fence;
  EnqueueOrder current_fence =
      position == TaskQueue::InsertFencePosition::NOW
          ? main_thread_only().task_queue_manager->GetNextSequenceNumber()
          : static_cast<EnqueueOrder>(EnqueueOrderValues::BLOCKING_FENCE);

  // Tasks posted after this point will have a strictly higher enqueue order
  // and will be blocked from running.
  bool task_unblocked = InsertFenceImpl(current_fence);

  if (!task_unblocked && previous_fence && previous_fence < current_fence) {
    base::AutoLock lock(immediate_incoming_queue_lock_);
    if (!immediate_incoming_queue().empty() &&
        immediate_incoming_queue().front().enqueue_order() > previous_fence &&
        immediate_incoming_queue().front().enqueue_order() < current_fence) {
      task_unblocked = true;
    }
  }

  if (IsQueueEnabled() && task_unblocked) {
    main_thread_only().task_queue_manager->MaybeScheduleImmediateWork(
        FROM_HERE);
  }
}

void TaskQueueImpl::InsertFenceAt(base::TimeTicks time) {
  // Task queue can have only one fence, delayed or not.
  RemoveFence();
  main_thread_only().delayed_fence = time;
}

void TaskQueueImpl::RemoveFence() {
  if (!main_thread_only().task_queue_manager)
    return;

  EnqueueOrder previous_fence = main_thread_only().current_fence;
  main_thread_only().current_fence = 0;
  main_thread_only().delayed_fence = base::nullopt;

  bool task_unblocked = main_thread_only().immediate_work_queue->RemoveFence();
  task_unblocked |= main_thread_only().delayed_work_queue->RemoveFence();

  if (!task_unblocked && previous_fence) {
    base::AutoLock lock(immediate_incoming_queue_lock_);
    if (!immediate_incoming_queue().empty() &&
        immediate_incoming_queue().front().enqueue_order() > previous_fence) {
      task_unblocked = true;
    }
  }

  if (IsQueueEnabled() && task_unblocked) {
    main_thread_only().task_queue_manager->MaybeScheduleImmediateWork(
        FROM_HERE);
  }
}

bool TaskQueueImpl::InsertFenceImpl(EnqueueOrder fence) {
  main_thread_only().current_fence = fence;
  bool task_unblocked =
      main_thread_only().immediate_work_queue->InsertFence(fence);
  task_unblocked |= main_thread_only().delayed_work_queue->InsertFence(fence);
  return task_unblocked;
}

bool TaskQueueImpl::BlockedByFence() const {
  if (!main_thread_only().current_fence)
    return false;

  if (!main_thread_only().immediate_work_queue->BlockedByFence() ||
      !main_thread_only().delayed_work_queue->BlockedByFence()) {
    return false;
  }

  base::AutoLock lock(immediate_incoming_queue_lock_);
  if (immediate_incoming_queue().empty())
    return true;

  return immediate_incoming_queue().front().enqueue_order() >
         main_thread_only().current_fence;
}

bool TaskQueueImpl::HasActiveFence() {
  if (main_thread_only().delayed_fence &&
      main_thread_only().time_domain->Now() >
          main_thread_only().delayed_fence.value()) {
    return true;
  }
  return !!main_thread_only().current_fence;
}

bool TaskQueueImpl::CouldTaskRun(EnqueueOrder enqueue_order) const {
  if (!IsQueueEnabled())
    return false;

  if (!main_thread_only().current_fence)
    return true;

  return enqueue_order < main_thread_only().current_fence;
}

EnqueueOrder TaskQueueImpl::GetFenceForTest() const {
  return main_thread_only().current_fence;
}

// static
void TaskQueueImpl::QueueAsValueInto(const TaskDeque& queue,
                                     base::TimeTicks now,
                                     base::trace_event::TracedValue* state) {
  for (const Task& task : queue) {
    TaskAsValueInto(task, now, state);
  }
}

// static
void TaskQueueImpl::QueueAsValueInto(const std::priority_queue<Task>& queue,
                                     base::TimeTicks now,
                                     base::trace_event::TracedValue* state) {
  // Remove const to search |queue| in the destructive manner. Restore the
  // content from |visited| later.
  std::priority_queue<Task>* mutable_queue =
      const_cast<std::priority_queue<Task>*>(&queue);
  std::priority_queue<Task> visited;
  while (!mutable_queue->empty()) {
    TaskAsValueInto(mutable_queue->top(), now, state);
    visited.push(std::move(const_cast<Task&>(mutable_queue->top())));
    mutable_queue->pop();
  }
  *mutable_queue = std::move(visited);
}

// static
void TaskQueueImpl::TaskAsValueInto(const Task& task,
                                    base::TimeTicks now,
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
  state->SetBoolean("nestable", task.nestable == base::Nestable::kNestable);
  state->SetBoolean("is_high_res", task.is_high_res);
  state->SetBoolean("is_cancelled", task.task.IsCancelled());
  state->SetDouble(
      "delayed_run_time",
      (task.delayed_run_time - base::TimeTicks()).InMillisecondsF());
  state->SetDouble("delayed_run_time_milliseconds_from_now",
                   (task.delayed_run_time - now).InMillisecondsF());
  state->EndDictionary();
}

TaskQueueImpl::QueueEnabledVoterImpl::QueueEnabledVoterImpl(
    scoped_refptr<TaskQueue> task_queue)
    : task_queue_(task_queue), enabled_(true) {}

TaskQueueImpl::QueueEnabledVoterImpl::~QueueEnabledVoterImpl() {
  if (task_queue_->GetTaskQueueImpl())
    task_queue_->GetTaskQueueImpl()->RemoveQueueEnabledVoter(this);
}

void TaskQueueImpl::QueueEnabledVoterImpl::SetQueueEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;

  task_queue_->GetTaskQueueImpl()->OnQueueEnabledVoteChanged(enabled);
  enabled_ = enabled;
}

void TaskQueueImpl::RemoveQueueEnabledVoter(
    const QueueEnabledVoterImpl* voter) {
  // Bail out if we're being called from TaskQueueImpl::UnregisterTaskQueue.
  if (!main_thread_only().time_domain)
    return;

  bool was_enabled = IsQueueEnabled();
  if (voter->enabled_) {
    main_thread_only().is_enabled_refcount--;
    DCHECK_GE(main_thread_only().is_enabled_refcount, 0);
  }

  main_thread_only().voter_refcount--;
  DCHECK_GE(main_thread_only().voter_refcount, 0);

  bool is_enabled = IsQueueEnabled();
  if (was_enabled != is_enabled)
    EnableOrDisableWithSelector(is_enabled);
}

bool TaskQueueImpl::IsQueueEnabled() const {
  // By default is_enabled_refcount and voter_refcount both equal zero.
  return (main_thread_only().is_enabled_refcount ==
          main_thread_only().voter_refcount) &&
         main_thread_only().is_enabled_for_test;
}

void TaskQueueImpl::OnQueueEnabledVoteChanged(bool enabled) {
  bool was_enabled = IsQueueEnabled();
  if (enabled) {
    main_thread_only().is_enabled_refcount++;
    DCHECK_LE(main_thread_only().is_enabled_refcount,
              main_thread_only().voter_refcount);
  } else {
    main_thread_only().is_enabled_refcount--;
    DCHECK_GE(main_thread_only().is_enabled_refcount, 0);
  }

  bool is_enabled = IsQueueEnabled();
  if (was_enabled != is_enabled)
    EnableOrDisableWithSelector(is_enabled);
}

void TaskQueueImpl::EnableOrDisableWithSelector(bool enable) {
  if (!main_thread_only().task_queue_manager)
    return;

  if (enable) {
    if (HasPendingImmediateWork() &&
        !main_thread_only().on_next_wake_up_changed_callback.is_null()) {
      // Delayed work notification will be issued via time domain.
      main_thread_only().on_next_wake_up_changed_callback.Run(
          base::TimeTicks());
    }

    ScheduleDelayedWorkInTimeDomain(main_thread_only().time_domain->Now());

    // Note the selector calls TaskQueueManager::OnTaskQueueEnabled which posts
    // a DoWork if needed.
    main_thread_only().task_queue_manager->selector_.EnableQueue(this);
  } else {
    if (!main_thread_only().delayed_incoming_queue.empty())
      main_thread_only().time_domain->CancelDelayedWork(this);
    main_thread_only().task_queue_manager->selector_.DisableQueue(this);
  }
}

std::unique_ptr<TaskQueue::QueueEnabledVoter>
TaskQueueImpl::CreateQueueEnabledVoter(scoped_refptr<TaskQueue> task_queue) {
  DCHECK_EQ(task_queue->GetTaskQueueImpl(), this);
  main_thread_only().voter_refcount++;
  main_thread_only().is_enabled_refcount++;
  return std::make_unique<QueueEnabledVoterImpl>(task_queue);
}

void TaskQueueImpl::SweepCanceledDelayedTasks(base::TimeTicks now) {
  if (main_thread_only().delayed_incoming_queue.empty())
    return;

  base::TimeTicks first_task_runtime =
      main_thread_only().delayed_incoming_queue.top().delayed_run_time;

  // Remove canceled tasks.
  std::priority_queue<Task> remaining_tasks;
  while (!main_thread_only().delayed_incoming_queue.empty()) {
    if (!main_thread_only().delayed_incoming_queue.top().task.IsCancelled()) {
      remaining_tasks.push(std::move(
          const_cast<Task&>(main_thread_only().delayed_incoming_queue.top())));
    }
    main_thread_only().delayed_incoming_queue.pop();
  }

  main_thread_only().delayed_incoming_queue = std::move(remaining_tasks);

  // Re-schedule delayed call to WakeUpForDelayedWork if needed.
  if (main_thread_only().delayed_incoming_queue.empty()) {
    main_thread_only().time_domain->CancelDelayedWork(this);
  } else if (first_task_runtime !=
             main_thread_only().delayed_incoming_queue.top().delayed_run_time) {
    ScheduleDelayedWorkInTimeDomain(main_thread_only().time_domain->Now());
  }
}

void TaskQueueImpl::PushImmediateIncomingTaskForTest(
    TaskQueueImpl::Task&& task) {
  base::AutoLock lock(immediate_incoming_queue_lock_);
  immediate_incoming_queue().push_back(std::move(task));
}

void TaskQueueImpl::SetOnNextWakeUpChangedCallback(
    TaskQueueImpl::OnNextWakeUpChangedCallback callback) {
#if DCHECK_IS_ON()
  if (callback) {
    DCHECK(main_thread_only().on_next_wake_up_changed_callback.is_null())
        << "Can't assign two different observers to "
           "blink::scheduler::TaskQueue";
  }
#endif
  base::AutoLock lock(any_thread_lock_);
  any_thread().on_next_wake_up_changed_callback = callback;
  main_thread_only().on_next_wake_up_changed_callback = callback;
}

void TaskQueueImpl::ScheduleDelayedWorkInTimeDomain(base::TimeTicks now) {
  if (!IsQueueEnabled())
    return;
  if (main_thread_only().delayed_incoming_queue.empty())
    return;

  main_thread_only().time_domain->ScheduleDelayedWork(
      this, main_thread_only().delayed_incoming_queue.top().delayed_wake_up(),
      now);
}

void TaskQueueImpl::SetScheduledTimeDomainWakeUp(
    base::Optional<base::TimeTicks> scheduled_time_domain_wake_up) {
  main_thread_only().scheduled_time_domain_wake_up =
      scheduled_time_domain_wake_up;

  // If queue has immediate work an appropriate notification has already
  // been issued.
  if (!scheduled_time_domain_wake_up ||
      main_thread_only().on_next_wake_up_changed_callback.is_null() ||
      HasPendingImmediateWork())
    return;

  main_thread_only().on_next_wake_up_changed_callback.Run(
      scheduled_time_domain_wake_up.value());
}

bool TaskQueueImpl::HasPendingImmediateWork() {
  // Any work queue tasks count as immediate work.
  if (!main_thread_only().delayed_work_queue->Empty() ||
      !main_thread_only().immediate_work_queue->Empty()) {
    return true;
  }

  // Finally tasks on |immediate_incoming_queue| count as immediate work.
  base::AutoLock lock(immediate_incoming_queue_lock_);
  return !immediate_incoming_queue().empty();
}

void TaskQueueImpl::SetOnTaskStartedHandler(
    TaskQueueImpl::OnTaskStartedHandler handler) {
  main_thread_only().on_task_started_handler = std::move(handler);
}

void TaskQueueImpl::OnTaskStarted(const TaskQueue::Task& task,
                                  base::TimeTicks start) {
  if (!main_thread_only().on_task_started_handler.is_null())
    main_thread_only().on_task_started_handler.Run(task, start);
}

void TaskQueueImpl::SetOnTaskCompletedHandler(
    TaskQueueImpl::OnTaskCompletedHandler handler) {
  main_thread_only().on_task_completed_handler = std::move(handler);
}

void TaskQueueImpl::OnTaskCompleted(const TaskQueue::Task& task,
                                    base::TimeTicks start,
                                    base::TimeTicks end) {
  if (!main_thread_only().on_task_completed_handler.is_null())
    main_thread_only().on_task_completed_handler.Run(task, start, end);
}

bool TaskQueueImpl::RequiresTaskTiming() const {
  return !main_thread_only().on_task_started_handler.is_null() ||
         !main_thread_only().on_task_completed_handler.is_null();
}

bool TaskQueueImpl::IsUnregistered() const {
  base::AutoLock lock(any_thread_lock_);
  return !any_thread().task_queue_manager;
}

base::WeakPtr<TaskQueueManager> TaskQueueImpl::GetTaskQueueManagerWeakPtr() {
  return main_thread_only().task_queue_manager->GetWeakPtr();
}

void TaskQueueImpl::SetQueueEnabledForTest(bool enabled) {
  main_thread_only().is_enabled_for_test = enabled;
  EnableOrDisableWithSelector(IsQueueEnabled());
}

void TaskQueueImpl::ActivateDelayedFenceIfNeeded(base::TimeTicks now) {
  if (!main_thread_only().delayed_fence)
    return;
  if (main_thread_only().delayed_fence.value() > now)
    return;
  InsertFence(TaskQueue::InsertFencePosition::NOW);
  main_thread_only().delayed_fence = base::nullopt;
}

}  // namespace internal
}  // namespace scheduler
}  // namespace blink
