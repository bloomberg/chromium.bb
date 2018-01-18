// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/task_queue_manager.h"

#include <memory>
#include <queue>
#include <set>

#include "base/bind.h"
#include "base/bit_cast.h"
#include "base/debug/crash_logging.h"
#include "base/rand_util.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/task_queue_selector.h"
#include "platform/scheduler/base/task_time_observer.h"
#include "platform/scheduler/base/thread_controller.h"
#include "platform/scheduler/base/thread_controller_impl.h"
#include "platform/scheduler/base/work_queue.h"
#include "platform/scheduler/base/work_queue_sets.h"

namespace blink {
namespace scheduler {

namespace {

const double kLongTaskTraceEventThreshold = 0.05;
const double kSamplingRateForRecordingCPUTime = 0.01;

double MonotonicTimeInSeconds(base::TimeTicks time_ticks) {
  return (time_ticks - base::TimeTicks()).InSecondsF();
}

// Magic value to protect against memory corruption and bail out
// early when detected.
constexpr int kMemoryCorruptionSentinelValue = 0xdeadbeef;

void SweepCanceledDelayedTasksInQueue(
    internal::TaskQueueImpl* queue,
    std::map<TimeDomain*, base::TimeTicks>* time_domain_now) {
  TimeDomain* time_domain = queue->GetTimeDomain();
  if (time_domain_now->find(time_domain) == time_domain_now->end())
    time_domain_now->insert(std::make_pair(time_domain, time_domain->Now()));
  queue->SweepCanceledDelayedTasks(time_domain_now->at(time_domain));
}

}  // namespace

TaskQueueManager::TaskQueueManager(
    std::unique_ptr<internal::ThreadController> controller)
    : real_time_domain_(new RealTimeDomain()),
      graceful_shutdown_helper_(new internal::GracefulQueueShutdownHelper()),
      controller_(std::move(controller)),
      random_generator_(base::RandUint64()),
      uniform_distribution_(0.0, 1.0),
      memory_corruption_sentinel_(kMemoryCorruptionSentinelValue),
      weak_factory_(this) {
  // TODO(altimin): Create a sequence checker here.
  DCHECK(controller_->RunsTasksInCurrentSequence());
  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "TaskQueueManager",
      this);
  selector_.SetTaskQueueSelectorObserver(this);

  // TODO(alexclarke): Change this to be a parameter that's passed in.
  RegisterTimeDomain(real_time_domain_.get());

  controller_->SetSequence(this);
  controller_->AddNestingObserver(this);
}

TaskQueueManager::~TaskQueueManager() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "TaskQueueManager",
      this);

  // TODO(altimin): restore default task runner automatically when
  // ThreadController is destroyed.
  controller_->RestoreDefaultTaskRunner();

  for (internal::TaskQueueImpl* queue : active_queues_) {
    selector_.RemoveQueue(queue);
    queue->UnregisterTaskQueue();
  }

  active_queues_.clear();
  queues_to_gracefully_shutdown_.clear();

  graceful_shutdown_helper_->OnTaskQueueManagerDeleted();

  selector_.SetTaskQueueSelectorObserver(nullptr);

  controller_->RemoveNestingObserver(this);
}

std::unique_ptr<TaskQueueManager> TaskQueueManager::TakeOverCurrentThread() {
  return std::unique_ptr<TaskQueueManager>(
      new TaskQueueManager(internal::ThreadControllerImpl::Create(
          base::MessageLoop::current(),
          base::DefaultTickClock::GetInstance())));
}

void TaskQueueManager::RegisterTimeDomain(TimeDomain* time_domain) {
  time_domains_.insert(time_domain);
  time_domain->OnRegisterWithTaskQueueManager(this);
}

void TaskQueueManager::UnregisterTimeDomain(TimeDomain* time_domain) {
  time_domains_.erase(time_domain);
}

std::unique_ptr<internal::TaskQueueImpl> TaskQueueManager::CreateTaskQueueImpl(
    const TaskQueue::Spec& spec) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  TimeDomain* time_domain =
      spec.time_domain ? spec.time_domain : real_time_domain_.get();
  DCHECK(time_domains_.find(time_domain) != time_domains_.end());
  std::unique_ptr<internal::TaskQueueImpl> task_queue =
      std::make_unique<internal::TaskQueueImpl>(this, time_domain, spec);
  active_queues_.insert(task_queue.get());
  selector_.AddQueue(task_queue.get());
  return task_queue;
}

void TaskQueueManager::SetObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  observer_ = observer;
}

void TaskQueueManager::UnregisterTaskQueueImpl(
    std::unique_ptr<internal::TaskQueueImpl> task_queue) {
  TRACE_EVENT1("renderer.scheduler", "TaskQueueManager::UnregisterTaskQueue",
               "queue_name", task_queue->GetName());
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  selector_.RemoveQueue(task_queue.get());

  {
    base::AutoLock lock(any_thread_lock_);
    any_thread().has_incoming_immediate_work.erase(task_queue.get());
  }

  task_queue->UnregisterTaskQueue();

  // Add |task_queue| to |queues_to_delete_| so we can prevent it from being
  // freed while any of our structures hold hold a raw pointer to it.
  active_queues_.erase(task_queue.get());
  queues_to_delete_[task_queue.get()] = std::move(task_queue);
}

void TaskQueueManager::ReloadEmptyWorkQueues(
    const IncomingImmediateWorkMap& queues_to_reload) const {
  // There are two cases where a queue needs reloading.  First, it might be
  // completely empty and we've just posted a task (this method handles that
  // case). Secondly if the work queue becomes empty in when calling
  // WorkQueue::TakeTaskFromWorkQueue (handled there).
  for (const auto& pair : queues_to_reload) {
    pair.first->ReloadImmediateWorkQueueIfEmpty();
  }
}

void TaskQueueManager::WakeUpReadyDelayedQueues(LazyNow* lazy_now) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "TaskQueueManager::WakeUpReadyDelayedQueues");

  for (TimeDomain* time_domain : time_domains_) {
    if (time_domain == real_time_domain_.get()) {
      time_domain->WakeUpReadyDelayedQueues(lazy_now);
    } else {
      LazyNow time_domain_lazy_now = time_domain->CreateLazyNow();
      time_domain->WakeUpReadyDelayedQueues(&time_domain_lazy_now);
    }
  }
}

void TaskQueueManager::OnBeginNestedRunLoop() {
  // We just entered a nested run loop, make sure there's a DoWork posted or
  // the system will grind to a halt.
  {
    base::AutoLock lock(any_thread_lock_);
    any_thread().immediate_do_work_posted_count++;
    any_thread().nesting_depth++;
  }
  main_thread_only().nesting_depth++;
  if (observer_ && main_thread_only().nesting_depth == 1)
    observer_->OnBeginNestedRunLoop();

  controller_->ScheduleWork();
}

void TaskQueueManager::OnExitNestedRunLoop() {
  {
    base::AutoLock lock(any_thread_lock_);
    any_thread().nesting_depth--;
  }
  main_thread_only().nesting_depth--;
  DCHECK_GE(main_thread_only().nesting_depth, 0);
  if (main_thread_only().nesting_depth == 0) {
    // While we were nested some non-nestable tasks may have become eligible to
    // run. We push them back onto the front of their original work queues.
    while (!main_thread_only().non_nestable_task_queue.empty()) {
      NonNestableTask& non_nestable_task =
          *main_thread_only().non_nestable_task_queue.begin();
      non_nestable_task.task_queue->RequeueDeferredNonNestableTask(
          std::move(non_nestable_task.task), non_nestable_task.work_type);
      main_thread_only().non_nestable_task_queue.pop_front();
    }
    if (observer_)
      observer_->OnExitNestedRunLoop();
  }
}

void TaskQueueManager::OnQueueHasIncomingImmediateWork(
    internal::TaskQueueImpl* queue,
    internal::EnqueueOrder enqueue_order,
    bool queue_is_blocked) {
  MoveableAutoLock lock(any_thread_lock_);
  any_thread().has_incoming_immediate_work.insert(
      std::make_pair(queue, enqueue_order));
  if (!queue_is_blocked)
    MaybeScheduleImmediateWorkLocked(FROM_HERE, std::move(lock));
}

void TaskQueueManager::MaybeScheduleImmediateWork(
    const base::Location& from_here) {
  MoveableAutoLock lock(any_thread_lock_);
  MaybeScheduleImmediateWorkLocked(from_here, std::move(lock));
}

void TaskQueueManager::MaybeScheduleImmediateWorkLocked(
    const base::Location& from_here,
    MoveableAutoLock lock) {
  {
    MoveableAutoLock auto_lock(std::move(lock));
    // Unless we're nested, try to avoid posting redundant DoWorks.
    if (any_thread().nesting_depth == 0 &&
        (any_thread().do_work_running_count == 1 ||
         any_thread().immediate_do_work_posted_count > 0)) {
      return;
    }

    any_thread().immediate_do_work_posted_count++;
  }

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "TaskQueueManager::MaybeScheduleImmediateWorkLocked::PostTask");
  controller_->ScheduleWork();
}

void TaskQueueManager::MaybeScheduleDelayedWork(
    const base::Location& from_here,
    TimeDomain* requesting_time_domain,
    base::TimeTicks now,
    base::TimeTicks run_time) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  // Make sure we don't cancel another TimeDomain's wake-up.
  DCHECK(!next_delayed_do_work_ ||
         next_delayed_do_work_.time_domain() == requesting_time_domain);
  {
    base::AutoLock lock(any_thread_lock_);

    // Unless we're nested, don't post a delayed DoWork if there's an immediate
    // DoWork in flight or we're inside a DoWork. We can rely on DoWork posting
    // a delayed continuation as needed.
    if (any_thread().nesting_depth == 0 &&
        (any_thread().immediate_do_work_posted_count > 0 ||
         any_thread().do_work_running_count == 1)) {
      return;
    }
  }

  // If there's a delayed DoWork scheduled to run sooner, we don't need to do
  // anything because DoWork will post a delayed continuation as needed.
  if (next_delayed_do_work_ && next_delayed_do_work_.run_time() <= run_time)
    return;

  base::TimeDelta delay = std::max(base::TimeDelta(), run_time - now);
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "TaskQueueManager::MaybeScheduleDelayedWork::PostDelayedTask",
               "delay_ms", delay.InMillisecondsF());
  next_delayed_do_work_ = NextDelayedDoWork(run_time, requesting_time_domain);
  controller_->ScheduleDelayedWork(delay);
}

void TaskQueueManager::CancelDelayedWork(TimeDomain* requesting_time_domain,
                                         base::TimeTicks run_time) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (next_delayed_do_work_.run_time() != run_time)
    return;

  DCHECK_EQ(next_delayed_do_work_.time_domain(), requesting_time_domain);
  controller_->CancelDelayedWork();
  next_delayed_do_work_.Clear();
}

void TaskQueueManager::DoWork(WorkType work_type) {
  CHECK(Validate());

  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  TRACE_EVENT1("renderer.scheduler", "TaskQueueManager::DoWork", "delayed",
               work_type == WorkType::kDelayed);

  LazyNow lazy_now(real_time_domain()->CreateLazyNow());

  // This must be done before running any tasks because they could invoke a
  // nested run loop and we risk having a stale |next_delayed_do_work_|.
  if (work_type == WorkType::kDelayed)
    next_delayed_do_work_.Clear();

  for (int i = 0; i < work_batch_size_; i++) {
    IncomingImmediateWorkMap queues_to_reload;

    {
      base::AutoLock lock(any_thread_lock_);
      if (i == 0) {
        any_thread().do_work_running_count++;

        if (work_type == WorkType::kImmediate) {
          any_thread().immediate_do_work_posted_count--;
          DCHECK_GE(any_thread().immediate_do_work_posted_count, 0);
        }
      }
      std::swap(queues_to_reload, any_thread().has_incoming_immediate_work);
    }

    // It's important we call ReloadEmptyWorkQueues out side of the lock to
    // avoid a lock order inversion.
    ReloadEmptyWorkQueues(queues_to_reload);

    WakeUpReadyDelayedQueues(&lazy_now);

    internal::WorkQueue* work_queue = nullptr;
    if (!SelectWorkQueueToService(&work_queue))
      break;

    // NB this may unregister |work_queue|.
    base::TimeTicks time_after_task;
    switch (ProcessTaskFromWorkQueue(work_queue, lazy_now, &time_after_task)) {
      case ProcessTaskResult::kDeferred:
        // If a task was deferred, try again with another task.
        continue;
      case ProcessTaskResult::kExecuted:
        break;
      case ProcessTaskResult::kTaskQueueManagerDeleted:
        return;  // The TaskQueueManager got deleted, we must bail out.
    }

    CHECK(Validate());

    lazy_now = time_after_task.is_null() ? real_time_domain()->CreateLazyNow()
                                         : LazyNow(time_after_task);

    // Only run a single task per batch in nested run loops so that we can
    // properly exit the nested loop when someone calls RunLoop::Quit().
    if (main_thread_only().nesting_depth > 0)
      break;
  }

  if (main_thread_only().nesting_depth == 0)
    CleanUpQueues();

  // TODO(alexclarke): Consider refactoring the above loop to terminate only
  // when there's no more work left to be done, rather than posting a
  // continuation task.

  {
    MoveableAutoLock lock(any_thread_lock_);
    base::Optional<NextTaskDelay> next_delay =
        ComputeDelayTillNextTaskLocked(&lazy_now);

    any_thread().do_work_running_count--;
    DCHECK_GE(any_thread().do_work_running_count, 0);

    PostDoWorkContinuationLocked(next_delay, &lazy_now, std::move(lock));
  }
}

void TaskQueueManager::PostDoWorkContinuationLocked(
    base::Optional<NextTaskDelay> next_delay,
    LazyNow* lazy_now,
    MoveableAutoLock lock) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  {
    MoveableAutoLock auto_lock(std::move(lock));

    // If there are no tasks left then we don't need to post a continuation.
    if (!next_delay) {
      // If there's a pending delayed DoWork, cancel it because it's not needed.
      if (next_delayed_do_work_) {
        next_delayed_do_work_.Clear();
        controller_->CancelDelayedWork();
      }
      return;
    }

    // If an immediate DoWork is posted, we don't need to post a continuation.
    if (any_thread().immediate_do_work_posted_count > 0)
      return;

    if (next_delay->delay() <= base::TimeDelta()) {
      // If a delayed DoWork is pending then we don't need to post a
      // continuation because it should run immediately.
      if (next_delayed_do_work_ &&
          next_delayed_do_work_.run_time() <= lazy_now->Now()) {
        return;
      }

      any_thread().immediate_do_work_posted_count++;
    }
  }

  // We avoid holding |any_thread_lock_| while posting the task.
  if (next_delay->delay() <= base::TimeDelta()) {
    controller_->ScheduleWork();
  } else {
    base::TimeTicks run_time = lazy_now->Now() + next_delay->delay();

    if (next_delayed_do_work_.run_time() == run_time)
      return;

    next_delayed_do_work_ =
        NextDelayedDoWork(run_time, next_delay->time_domain());
    controller_->ScheduleDelayedWork(next_delay->delay());
  }
}

base::Optional<TaskQueueManager::NextTaskDelay>
TaskQueueManager::ComputeDelayTillNextTaskLocked(LazyNow* lazy_now) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  // Unfortunately because |any_thread_lock_| is held it's not safe to call
  // ReloadEmptyWorkQueues here (possible lock order inversion), however this
  // check is equavalent to calling ReloadEmptyWorkQueues first.
  for (const auto& pair : any_thread().has_incoming_immediate_work) {
    if (pair.first->CouldTaskRun(pair.second))
      return NextTaskDelay();
  }

  // If the selector has non-empty queues we trivially know there is immediate
  // work to be done.
  if (!selector_.EnabledWorkQueuesEmpty())
    return NextTaskDelay();

  // Otherwise we need to find the shortest delay, if any.  NB we don't need to
  // call WakeUpReadyDelayedQueues because it's assumed DelayTillNextTask will
  // return base::TimeDelta>() if the delayed task is due to run now.
  base::Optional<NextTaskDelay> delay_till_next_task;
  for (TimeDomain* time_domain : time_domains_) {
    base::Optional<base::TimeDelta> delay =
        time_domain->DelayTillNextTask(lazy_now);
    if (!delay)
      continue;

    NextTaskDelay task_delay = (delay.value() == base::TimeDelta())
                                   ? NextTaskDelay()
                                   : NextTaskDelay(delay.value(), time_domain);

    if (!delay_till_next_task || delay_till_next_task > task_delay)
      delay_till_next_task = task_delay;
  }
  return delay_till_next_task;
}

bool TaskQueueManager::SelectWorkQueueToService(
    internal::WorkQueue** out_work_queue) {
  bool should_run = selector_.SelectWorkQueueToService(out_work_queue);
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler.debug"), "TaskQueueManager",
      this, AsValueWithSelectorResult(should_run, *out_work_queue));
  return should_run;
}

void TaskQueueManager::DidQueueTask(
    const internal::TaskQueueImpl::Task& pending_task) {
  task_annotator_.DidQueueTask("TaskQueueManager::PostTask", pending_task);
}

TaskQueueManager::ProcessTaskResult TaskQueueManager::ProcessTaskFromWorkQueue(
    internal::WorkQueue* work_queue,
    LazyNow time_before_task,
    base::TimeTicks* time_after_task) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  base::WeakPtr<TaskQueueManager> protect = GetWeakPtr();
  internal::TaskQueueImpl::Task pending_task =
      work_queue->TakeTaskFromWorkQueue();
  bool should_record_thread_time = ShouldRecordCPUTimeForTask();

  // It's possible the task was canceled, if so bail out.
  // The task should be non-null, but it seems to be possible to due
  // a hard-to-track bug. The first check is a defence against this bug,
  // and this check isn't expected to be true in practice.
  if (!pending_task.task || pending_task.task.IsCancelled())
    return ProcessTaskResult::kExecuted;

  internal::TaskQueueImpl* queue = work_queue->task_queue();
  if (queue->GetQuiescenceMonitored())
    task_was_run_on_quiescence_monitored_queue_ = true;
  if (pending_task.nestable == base::Nestable::kNonNestable &&
      main_thread_only().nesting_depth > 0) {
    // Defer non-nestable work. NOTE these tasks can be arbitrarily delayed so
    // the additional delay should not be a problem.
    // Note because we don't delete queues while nested, it's perfectly OK to
    // store the raw pointer for |queue| here.
    NonNestableTask deferred_task{std::move(pending_task), queue,
                                  work_queue->queue_type()};
    // We push these tasks onto the front to make sure that when requeued they
    // are pushed in the right order.
    main_thread_only().non_nestable_task_queue.push_front(
        std::move(deferred_task));
    return ProcessTaskResult::kDeferred;
  }

  base::TimeTicks task_start_time;
  TRACE_TASK_EXECUTION("TaskQueueManager::ProcessTaskFromWorkQueue",
                       pending_task);

  NotifyWillProcessTaskObservers(pending_task, queue, time_before_task,
                                 &task_start_time);

  base::ThreadTicks task_start_thread_time;
  if (should_record_thread_time)
    task_start_thread_time = base::ThreadTicks::Now();
  base::debug::SetCrashKeyString(main_thread_only().file_name_crash_key,
                                 pending_task.posted_from.file_name());
  base::debug::SetCrashKeyString(main_thread_only().function_name_crash_key,
                                 pending_task.posted_from.function_name());

  {
    TRACE_EVENT1("renderer.scheduler", "TaskQueueManager::RunTask", "queue",
                 queue->GetName());
    // NOTE when TaskQueues get unregistered a reference ends up getting
    // retained by |queues_to_delete_| which is cleared at the top of |DoWork|.
    // This means we are OK to use raw pointers here.
    internal::TaskQueueImpl* prev_executing_task_queue =
        currently_executing_task_queue_;
    currently_executing_task_queue_ = queue;
    task_annotator_.RunTask("TaskQueueManager::PostTask", &pending_task);
    // Detect if the TaskQueueManager just got deleted.  If this happens we must
    // not access any member variables after this point.
    if (!protect)
      return ProcessTaskResult::kTaskQueueManagerDeleted;

    currently_executing_task_queue_ = prev_executing_task_queue;
  }

  base::ThreadTicks task_end_thread_time;
  if (should_record_thread_time)
    task_end_thread_time = base::ThreadTicks::Now();

  NotifyDidProcessTaskObservers(
      pending_task, queue,
      should_record_thread_time
          ? base::Optional<base::TimeDelta>(task_end_thread_time -
                                            task_start_thread_time)
          : base::nullopt,
      task_start_time, time_after_task);

  return ProcessTaskResult::kExecuted;
}

void TaskQueueManager::NotifyWillProcessTaskObservers(
    const internal::TaskQueueImpl::Task& pending_task,
    internal::TaskQueueImpl* queue,
    LazyNow time_before_task,
    base::TimeTicks* task_start_time) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "TaskQueueManager::NotifyWillProcessTaskObservers");
  if (!queue->GetShouldNotifyObservers())
    return;

  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "TaskQueueManager.WillProcessTaskObservers");
    for (auto& observer : task_observers_)
      observer.WillProcessTask(pending_task);
  }

  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "TaskQueueManager.QueueNotifyWillProcessTask");
    queue->NotifyWillProcessTask(pending_task);
  }

  bool notify_time_observers = main_thread_only().nesting_depth == 0 &&
                               (task_time_observers_.might_have_observers() ||
                                queue->RequiresTaskTiming());
  if (!notify_time_observers)
    return;
  *task_start_time = time_before_task.Now();
  double task_start_time_sec = MonotonicTimeInSeconds(*task_start_time);

  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "TaskQueueManager.WillProcessTaskTimeObservers");
    for (auto& observer : task_time_observers_)
      observer.WillProcessTask(task_start_time_sec);
  }

  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "TaskQueueManager.QueueOnTaskStarted");
    queue->OnTaskStarted(pending_task, *task_start_time);
  }
}

void TaskQueueManager::NotifyDidProcessTaskObservers(
    const internal::TaskQueueImpl::Task& pending_task,
    internal::TaskQueueImpl* queue,
    base::Optional<base::TimeDelta> thread_time,
    base::TimeTicks task_start_time,
    base::TimeTicks* time_after_task) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "TaskQueueManager::NotifyDidProcessTaskObservers");

  if (!queue->GetShouldNotifyObservers())
    return;

  double task_start_time_sec = MonotonicTimeInSeconds(task_start_time);
  double task_end_time_sec = 0;

  if (task_start_time_sec) {
    *time_after_task = real_time_domain()->Now();
    task_end_time_sec = MonotonicTimeInSeconds(*time_after_task);

    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "TaskQueueManager.DidProcessTaskTimeObservers");
    for (auto& observer : task_time_observers_)
      observer.DidProcessTask(task_start_time_sec, task_end_time_sec);
  }

  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "TaskQueueManager.DidProcessTaskObservers");
    for (auto& observer : task_observers_)
      observer.DidProcessTask(pending_task);
  }

  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "TaskQueueManager.QueueNotifyDidProcessTask");
    queue->NotifyDidProcessTask(pending_task);
  }

  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "TaskQueueManager.QueueOnTaskCompleted");
    if (task_start_time_sec && task_end_time_sec)
      queue->OnTaskCompleted(pending_task, task_start_time, *time_after_task,
                             thread_time);
  }

  if (task_start_time_sec && task_end_time_sec &&
      task_end_time_sec - task_start_time_sec > kLongTaskTraceEventThreshold) {
    TRACE_EVENT_INSTANT1("blink", "LongTask", TRACE_EVENT_SCOPE_THREAD,
                         "duration", task_end_time_sec - task_start_time_sec);
  }
}

void TaskQueueManager::SetWorkBatchSize(int work_batch_size) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK_GE(work_batch_size, 1);
  work_batch_size_ = work_batch_size;
}

void TaskQueueManager::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  task_observers_.AddObserver(task_observer);
}

void TaskQueueManager::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  task_observers_.RemoveObserver(task_observer);
}

void TaskQueueManager::AddTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  task_time_observers_.AddObserver(task_time_observer);
}

void TaskQueueManager::RemoveTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  task_time_observers_.RemoveObserver(task_time_observer);
}

bool TaskQueueManager::GetAndClearSystemIsQuiescentBit() {
  bool task_was_run = task_was_run_on_quiescence_monitored_queue_;
  task_was_run_on_quiescence_monitored_queue_ = false;
  return !task_was_run;
}

internal::EnqueueOrder TaskQueueManager::GetNextSequenceNumber() {
  return enqueue_order_generator_.GenerateNext();
}

LazyNow TaskQueueManager::CreateLazyNow() const {
  return LazyNow(controller_->GetClock());
}

size_t TaskQueueManager::GetNumberOfPendingTasks() const {
  size_t task_count = 0;
  for (auto& queue : active_queues_)
    task_count += queue->GetNumberOfPendingTasks();
  return task_count;
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
TaskQueueManager::AsValueWithSelectorResult(
    bool should_run,
    internal::WorkQueue* selected_work_queue) const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  base::TimeTicks now = real_time_domain()->CreateLazyNow().Now();
  state->BeginArray("active_queues");
  for (auto& queue : active_queues_)
    queue->AsValueInto(now, state.get());
  state->EndArray();
  state->BeginArray("queues_to_gracefully_shutdown");
  for (const auto& pair : queues_to_gracefully_shutdown_)
    pair.first->AsValueInto(now, state.get());
  state->EndArray();
  state->BeginArray("queues_to_delete");
  for (const auto& pair : queues_to_delete_)
    pair.first->AsValueInto(now, state.get());
  state->EndArray();
  state->BeginDictionary("selector");
  selector_.AsValueInto(state.get());
  state->EndDictionary();
  if (should_run) {
    state->SetString("selected_queue",
                     selected_work_queue->task_queue()->GetName());
    state->SetString("work_queue_name", selected_work_queue->name());
  }

  state->BeginArray("time_domains");
  for (auto* time_domain : time_domains_)
    time_domain->AsValueInto(state.get());
  state->EndArray();
  {
    base::AutoLock lock(any_thread_lock_);
    state->SetInteger("nesting_depth", any_thread().nesting_depth);
    state->SetInteger("do_work_running_count",
                      any_thread().do_work_running_count);
    state->SetInteger("immediate_do_work_posted_count",
                      any_thread().immediate_do_work_posted_count);

    state->BeginArray("has_incoming_immediate_work");
    for (const auto& pair : any_thread().has_incoming_immediate_work) {
      state->AppendString(pair.first->GetName());
    }
    state->EndArray();
  }
  return std::move(state);
}

void TaskQueueManager::OnTaskQueueEnabled(internal::TaskQueueImpl* queue) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(queue->IsQueueEnabled());
  // Only schedule DoWork if there's something to do.
  if (queue->HasTaskToRunImmediately() && !queue->BlockedByFence())
    MaybeScheduleImmediateWork(FROM_HERE);
}

void TaskQueueManager::OnTriedToSelectBlockedWorkQueue(
    internal::WorkQueue* work_queue) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(!work_queue->Empty());
  if (observer_)
    observer_->OnTriedToExecuteBlockedTask();
}

bool TaskQueueManager::HasImmediateWorkForTesting() const {
  return !selector_.EnabledWorkQueuesEmpty();
}

void TaskQueueManager::SweepCanceledDelayedTasks() {
  std::map<TimeDomain*, base::TimeTicks> time_domain_now;
  for (const auto& queue : active_queues_)
    SweepCanceledDelayedTasksInQueue(queue, &time_domain_now);
  for (const auto& pair : queues_to_gracefully_shutdown_)
    SweepCanceledDelayedTasksInQueue(pair.first, &time_domain_now);
}

void TaskQueueManager::TakeQueuesToGracefullyShutdownFromHelper() {
  std::vector<std::unique_ptr<internal::TaskQueueImpl>> queues =
      graceful_shutdown_helper_->TakeQueues();
  for (std::unique_ptr<internal::TaskQueueImpl>& queue : queues) {
    queues_to_gracefully_shutdown_[queue.get()] = std::move(queue);
  }
}

void TaskQueueManager::CleanUpQueues() {
  TakeQueuesToGracefullyShutdownFromHelper();

  for (auto it = queues_to_gracefully_shutdown_.begin();
       it != queues_to_gracefully_shutdown_.end();) {
    if (it->first->IsEmpty()) {
      UnregisterTaskQueueImpl(std::move(it->second));
      active_queues_.erase(it->first);
      queues_to_gracefully_shutdown_.erase(it++);
    } else {
      ++it;
    }
  }
  queues_to_delete_.clear();
}

base::Optional<base::PendingTask> TaskQueueManager::TakeTask(
    WorkType work_type) {
  // TODO(altimin): Unify TakeTask and DoWork and run selector here.
  return base::PendingTask(FROM_HERE, base::Bind(&TaskQueueManager::DoWork,
                                                 GetWeakPtr(), work_type));
}

bool TaskQueueManager::DidRunTask() {
  return false;
}

scoped_refptr<internal::GracefulQueueShutdownHelper>
TaskQueueManager::GetGracefulQueueShutdownHelper() const {
  return graceful_shutdown_helper_;
}

base::WeakPtr<TaskQueueManager> TaskQueueManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void TaskQueueManager::SetDefaultTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  controller_->SetDefaultTaskRunner(task_runner);
}

base::TickClock* TaskQueueManager::GetClock() const {
  return controller_->GetClock();
}

base::TimeTicks TaskQueueManager::NowTicks() const {
  return controller_->GetClock()->NowTicks();
}

bool TaskQueueManager::ShouldRecordCPUTimeForTask() {
  return uniform_distribution_(random_generator_) <
         kSamplingRateForRecordingCPUTime;
}

void TaskQueueManager::SetRandomSeed(uint64_t value) {
  random_generator_.seed(value);
}

bool TaskQueueManager::Validate() {
  return memory_corruption_sentinel_ == kMemoryCorruptionSentinelValue;
}

void TaskQueueManager::EnableCrashKeys(
    const char* file_name_crash_key_name,
    const char* function_name_crash_key_name) {
  DCHECK(!main_thread_only().file_name_crash_key);
  DCHECK(!main_thread_only().function_name_crash_key);
  main_thread_only().file_name_crash_key = base::debug::AllocateCrashKeyString(
      file_name_crash_key_name, base::debug::CrashKeySize::Size64);
  main_thread_only().function_name_crash_key =
      base::debug::AllocateCrashKeyString(function_name_crash_key_name,
                                          base::debug::CrashKeySize::Size64);
}

}  // namespace scheduler
}  // namespace blink
