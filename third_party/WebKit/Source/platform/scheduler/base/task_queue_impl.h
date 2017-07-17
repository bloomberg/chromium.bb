// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_IMPL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_IMPL_H_

#include <stddef.h>

#include <memory>
#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/pending_task.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "platform/scheduler/base/enqueue_order.h"
#include "platform/scheduler/base/intrusive_heap.h"
#include "platform/scheduler/base/task_queue.h"
#include "platform/wtf/Deque.h"

namespace blink {
namespace scheduler {
class LazyNow;
class TimeDomain;
class TaskQueueManager;

namespace internal {
class WorkQueue;
class WorkQueueSets;

// TaskQueueImpl has four main queues:
//
// Immediate (non-delayed) tasks:
//    immediate_incoming_queue - PostTask enqueues tasks here
//    immediate_work_queue
//
// Delayed tasks
//    delayed_incoming_queue - PostDelayedTask enqueues tasks here
//    delayed_work_queue
//
// The immediate_incoming_queue can be accessed from any thread, the other
// queues are main-thread only. To reduce the overhead of locking,
// immediate_work_queue is swapped with immediate_incoming_queue when
// immediate_work_queue becomes empty.
//
// Delayed tasks are initially posted to delayed_incoming_queue and a wake-up
// is scheduled with the TimeDomain.  When the delay has elapsed, the TimeDomain
// calls UpdateDelayedWorkQueue and ready delayed tasks are moved into the
// delayed_work_queue.  Note the EnqueueOrder (used for ordering) for a delayed
// task is not set until it's moved into the delayed_work_queue.
//
// TaskQueueImpl uses the WorkQueueSets and the TaskQueueSelector to implement
// prioritization. Task selection is done by the TaskQueueSelector and when a
// queue is selected, it round-robins between the immediate_work_queue and
// delayed_work_queue.  The reason for this is we want to make sure delayed
// tasks (normally the most common type) don't starve out immediate work.
class PLATFORM_EXPORT TaskQueueImpl {
 public:
  TaskQueueImpl(TaskQueueManager* task_queue_manager,
                TimeDomain* time_domain,
                const TaskQueue::Spec& spec);

  ~TaskQueueImpl();

  // Represents a time at which a task wants to run. Tasks scheduled for the
  // same point in time will be ordered by their sequence numbers.
  struct DelayedWakeUp {
    base::TimeTicks time;
    int sequence_num;

    bool operator<=(const DelayedWakeUp& other) const {
      if (time == other.time) {
        DCHECK_NE(sequence_num, other.sequence_num);
        return (sequence_num - other.sequence_num) < 0;
      }
      return time < other.time;
    }
  };

  class PLATFORM_EXPORT Task : public TaskQueue::Task {
   public:
    Task();
    Task(const tracked_objects::Location& posted_from,
         base::OnceClosure task,
         base::TimeTicks desired_run_time,
         EnqueueOrder sequence_number,
         bool nestable);

    Task(const tracked_objects::Location& posted_from,
         base::OnceClosure task,
         base::TimeTicks desired_run_time,
         EnqueueOrder sequence_number,
         bool nestable,
         EnqueueOrder enqueue_order);

    DelayedWakeUp delayed_wake_up() const {
      return DelayedWakeUp{delayed_run_time, sequence_num};
    }

    EnqueueOrder enqueue_order() const {
#ifndef NDEBUG
      DCHECK(enqueue_order_set_);
#endif
      return enqueue_order_;
    }

    void set_enqueue_order(EnqueueOrder enqueue_order) {
#ifndef NDEBUG
      DCHECK(!enqueue_order_set_);
      enqueue_order_set_ = true;
#endif
      enqueue_order_ = enqueue_order;
    }

#ifndef NDEBUG
    bool enqueue_order_set() const { return enqueue_order_set_; }
#endif

   private:
#ifndef NDEBUG
    bool enqueue_order_set_;
#endif
    // Similar to sequence number, but ultimately the |enqueue_order_| is what
    // the scheduler uses for task ordering. For immediate tasks |enqueue_order|
    // is set when posted, but for delayed tasks it's not defined until they are
    // enqueued on the |delayed_work_queue_|. This is because otherwise delayed
    // tasks could run before an immediate task posted after the delayed task.
    EnqueueOrder enqueue_order_;
  };

  using OnNextWakeUpChangedCallback = base::Callback<void(base::TimeTicks)>;
  using OnTaskCompletedHandler = base::Callback<
      void(const TaskQueue::Task&, base::TimeTicks, base::TimeTicks)>;

  // TaskQueue implementation.
  const char* GetName() const;
  bool RunsTasksInCurrentSequence() const;
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay);
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay);
  // Require a reference to enclosing task queue for lifetime control.
  std::unique_ptr<TaskQueue::QueueEnabledVoter> CreateQueueEnabledVoter(
      scoped_refptr<TaskQueue> owning_task_queue);
  bool IsQueueEnabled() const;
  bool IsEmpty() const;
  size_t GetNumberOfPendingTasks() const;
  bool HasTaskToRunImmediately() const;
  base::Optional<base::TimeTicks> GetNextScheduledWakeUp();
  void SetQueuePriority(TaskQueue::QueuePriority priority);
  TaskQueue::QueuePriority GetQueuePriority() const;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer);
  void RemoveTaskObserver(base::MessageLoop::TaskObserver* task_observer);
  void SetTimeDomain(TimeDomain* time_domain);
  TimeDomain* GetTimeDomain() const;
  void SetBlameContext(base::trace_event::BlameContext* blame_context);
  void InsertFence(TaskQueue::InsertFencePosition position);
  void RemoveFence();
  bool HasFence() const;
  bool BlockedByFence() const;
  // Implementation of TaskQueue::SetObserver.
  void SetOnNextWakeUpChangedCallback(OnNextWakeUpChangedCallback callback);

  void UnregisterTaskQueue(scoped_refptr<TaskQueue> task_queue);

  // Returns true if a (potentially hypothetical) task with the specified
  // |enqueue_order| could run on the queue. Must be called from the main
  // thread.
  bool CouldTaskRun(EnqueueOrder enqueue_order) const;

  // Must only be called from the thread this task queue was created on.
  void ReloadImmediateWorkQueueIfEmpty();

  void AsValueInto(base::TimeTicks now,
                   base::trace_event::TracedValue* state) const;

  bool GetQuiescenceMonitored() const { return should_monitor_quiescence_; }
  bool GetShouldNotifyObservers() const { return should_notify_observers_; }

  void NotifyWillProcessTask(const base::PendingTask& pending_task);
  void NotifyDidProcessTask(const base::PendingTask& pending_task);

  // Called by TimeDomain when the wake-up for this queue has changed.
  // There is only one wake-up, new wake-up cancels any previous wake-ups.
  // If |scheduled_time_domain_wake_up| is base::nullopt then the wake-up
  // has been cancelled.
  // Must be called from the main thread.
  void SetScheduledTimeDomainWakeUp(
      base::Optional<base::TimeTicks> scheduled_time_domain_wake_up);

  // Check for available tasks in immediate work queues.
  // Used to check if we need to generate notifications about delayed work.
  bool HasPendingImmediateWork();

  WorkQueue* delayed_work_queue() {
    return main_thread_only().delayed_work_queue.get();
  }

  const WorkQueue* delayed_work_queue() const {
    return main_thread_only().delayed_work_queue.get();
  }

  WorkQueue* immediate_work_queue() {
    return main_thread_only().immediate_work_queue.get();
  }

  const WorkQueue* immediate_work_queue() const {
    return main_thread_only().immediate_work_queue.get();
  }

  bool should_report_when_execution_blocked() const {
    return should_report_when_execution_blocked_;
  }

  // Enqueues any delayed tasks which should be run now on the
  // |delayed_work_queue|. Returns the subsequent wake-up that is required, if
  // any. Must be called from the main thread.
  base::Optional<DelayedWakeUp> WakeUpForDelayedWork(LazyNow* lazy_now);

  base::Optional<base::TimeTicks> scheduled_time_domain_wake_up() const {
    return main_thread_only().scheduled_time_domain_wake_up;
  }

  HeapHandle heap_handle() const { return main_thread_only().heap_handle; }

  void set_heap_handle(HeapHandle heap_handle) {
    main_thread_only().heap_handle = heap_handle;
  }

  void PushImmediateIncomingTaskForTest(TaskQueueImpl::Task&& task);
  EnqueueOrder GetFenceForTest() const;

  class QueueEnabledVoterImpl : public TaskQueue::QueueEnabledVoter {
   public:
    explicit QueueEnabledVoterImpl(scoped_refptr<TaskQueue> task_queue);
    ~QueueEnabledVoterImpl() override;

    // QueueEnabledVoter implementation.
    void SetQueueEnabled(bool enabled) override;

    TaskQueueImpl* GetTaskQueueForTest() const {
      return task_queue_->GetTaskQueueImpl();
    }

   private:
    friend class TaskQueueImpl;

    scoped_refptr<TaskQueue> task_queue_;
    bool enabled_;
  };

  // Iterates over |delayed_incoming_queue| removing canceled tasks.
  void SweepCanceledDelayedTasks(base::TimeTicks now);

  // Allows wrapping TaskQueue to set a handler to subscribe for notifications
  // about completed tasks.
  void SetOnTaskCompletedHandler(OnTaskCompletedHandler handler);
  void OnTaskCompleted(const TaskQueue::Task& task,
                       base::TimeTicks start,
                       base::TimeTicks end);

  // Disables queue for testing purposes, when a QueueEnabledVoter can't be
  // constructed due to not having TaskQueue.
  void SetQueueEnabledForTest(bool enabled);

 private:
  friend class WorkQueue;
  friend class WorkQueueTest;

  enum class TaskType {
    NORMAL,
    NON_NESTABLE,
  };

  struct AnyThread {
    AnyThread(TaskQueueManager* task_queue_manager, TimeDomain* time_domain);
    ~AnyThread();

    // TaskQueueManager, TimeDomain and Observer are maintained in two copies:
    // inside AnyThread and inside MainThreadOnly. They can be changed only from
    // main thread, so it should be locked before accessing from other threads.
    TaskQueueManager* task_queue_manager;
    TimeDomain* time_domain;
    // Callback corresponding to TaskQueue::Observer::OnQueueNextChanged.
    OnNextWakeUpChangedCallback on_next_wake_up_changed_callback;
  };

  struct MainThreadOnly {
    MainThreadOnly(TaskQueueManager* task_queue_manager,
                   TaskQueueImpl* task_queue,
                   TimeDomain* time_domain);
    ~MainThreadOnly();

    // Another copy of TaskQueueManager, TimeDomain and Observer
    // for lock-free access from the main thread.
    // See description inside struct AnyThread for details.
    TaskQueueManager* task_queue_manager;
    TimeDomain* time_domain;
    // Callback corresponding to TaskQueue::Observer::OnQueueNextChanged.
    OnNextWakeUpChangedCallback on_next_wake_up_changed_callback;

    std::unique_ptr<WorkQueue> delayed_work_queue;
    std::unique_ptr<WorkQueue> immediate_work_queue;
    std::priority_queue<Task> delayed_incoming_queue;
    base::ObserverList<base::MessageLoop::TaskObserver> task_observers;
    size_t set_index;
    HeapHandle heap_handle;
    int is_enabled_refcount;
    int voter_refcount;
    base::trace_event::BlameContext* blame_context;  // Not owned.
    EnqueueOrder current_fence;
    base::Optional<base::TimeTicks> scheduled_time_domain_wake_up;
    OnTaskCompletedHandler on_task_completed_handler;
    // If false, queue will be disabled. Used only for tests.
    bool is_enabled_for_test;
  };

  bool PostImmediateTaskImpl(const tracked_objects::Location& from_here,
                             base::OnceClosure task,
                             TaskType task_type);
  bool PostDelayedTaskImpl(const tracked_objects::Location& from_here,
                           base::OnceClosure task,
                           base::TimeDelta delay,
                           TaskType task_type);

  // Push the task onto the |delayed_incoming_queue|. Lock-free main thread
  // only fast path.
  void PushOntoDelayedIncomingQueueFromMainThread(Task pending_task,
                                                  base::TimeTicks now);

  // Push the task onto the |delayed_incoming_queue|.  Slow path from other
  // threads.
  void PushOntoDelayedIncomingQueueLocked(Task pending_task);

  void ScheduleDelayedWorkTask(Task pending_task);

  void MoveReadyImmediateTasksToImmediateWorkQueueLocked();

  // Push the task onto the |immediate_incoming_queue| and for auto pumped
  // queues it calls MaybePostDoWorkOnMainRunner if the Incoming queue was
  // empty.
  void PushOntoImmediateIncomingQueueLocked(
      const tracked_objects::Location& posted_from,
      base::OnceClosure task,
      base::TimeTicks desired_run_time,
      EnqueueOrder sequence_number,
      bool nestable);

  // We reserve an inline capacity of 8 tasks to try and reduce the load on
  // PartitionAlloc.
  using TaskDeque = WTF::Deque<Task, 8>;

  // Extracts all the tasks from the immediate incoming queue and clears it.
  // Can be called from any thread.
  TaskDeque TakeImmediateIncomingQueue();

  void TraceQueueSize() const;
  static void QueueAsValueInto(const TaskDeque& queue,
                               base::TimeTicks now,
                               base::trace_event::TracedValue* state);
  static void QueueAsValueInto(const std::priority_queue<Task>& queue,
                               base::TimeTicks now,
                               base::trace_event::TracedValue* state);
  static void TaskAsValueInto(const Task& task,
                              base::TimeTicks now,
                              base::trace_event::TracedValue* state);

  void RemoveQueueEnabledVoter(const QueueEnabledVoterImpl* voter);
  void OnQueueEnabledVoteChanged(bool enabled);
  void EnableOrDisableWithSelector(bool enable);

  // Schedules delayed work on time domain and calls the observer.
  void ScheduleDelayedWorkInTimeDomain(base::TimeTicks now);

  const char* name_;

  const base::PlatformThreadId thread_id_;

  mutable base::Lock any_thread_lock_;
  AnyThread any_thread_;
  struct AnyThread& any_thread() {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }
  const struct AnyThread& any_thread() const {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }

  base::ThreadChecker main_thread_checker_;
  MainThreadOnly main_thread_only_;
  MainThreadOnly& main_thread_only() {
    DCHECK(main_thread_checker_.CalledOnValidThread());
    return main_thread_only_;
  }
  const MainThreadOnly& main_thread_only() const {
    DCHECK(main_thread_checker_.CalledOnValidThread());
    return main_thread_only_;
  }

  mutable base::Lock immediate_incoming_queue_lock_;
  TaskDeque immediate_incoming_queue_;
  TaskDeque& immediate_incoming_queue() {
    immediate_incoming_queue_lock_.AssertAcquired();
    return immediate_incoming_queue_;
  }
  const TaskDeque& immediate_incoming_queue() const {
    immediate_incoming_queue_lock_.AssertAcquired();
    return immediate_incoming_queue_;
  }

  const bool should_monitor_quiescence_;
  const bool should_notify_observers_;
  const bool should_report_when_execution_blocked_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueImpl);
};

}  // namespace internal
}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_IMPL_H_
