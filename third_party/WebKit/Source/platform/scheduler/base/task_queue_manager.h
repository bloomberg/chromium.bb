// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_H_

#include <map>

#include "base/atomic_sequence_num.h"
#include "base/cancelable_callback.h"
#include "base/debug/task_annotator.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/pending_task.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "platform/scheduler/base/enqueue_order.h"
#include "platform/scheduler/base/moveable_auto_lock.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/task_queue_selector.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}  // namespace trace_event
}  // namespace base

namespace blink {
namespace scheduler {
namespace task_queue_manager_unittest {
class TaskQueueManagerTest;
}
namespace internal {
class TaskQueueImpl;
}  // namespace internal

class LazyNow;
class RealTimeDomain;
class TaskQueue;
class TaskQueueManagerDelegate;
class TaskTimeObserver;
class TimeDomain;

// The task queue manager provides N task queues and a selector interface for
// choosing which task queue to service next. Each task queue consists of two
// sub queues:
//
// 1. Incoming task queue. Tasks that are posted get immediately appended here.
//    When a task is appended into an empty incoming queue, the task manager
//    work function (DoWork) is scheduled to run on the main task runner.
//
// 2. Work queue. If a work queue is empty when DoWork() is entered, tasks from
//    the incoming task queue (if any) are moved here. The work queues are
//    registered with the selector as input to the scheduling decision.
//
class PLATFORM_EXPORT TaskQueueManager
    : public internal::TaskQueueSelector::Observer,
      public base::RunLoop::NestingObserver {
 public:
  // Create a task queue manager where |delegate| identifies the thread
  // on which where the tasks are  eventually run. Category strings must have
  // application lifetime (statics or literals). They may not include " chars.
  explicit TaskQueueManager(scoped_refptr<TaskQueueManagerDelegate> delegate);
  ~TaskQueueManager() override;

  // Requests that a task to process work is posted on the main task runner.
  // These tasks are de-duplicated in two buckets: main-thread and all other
  // threads.  This distinction is done to reduce the overehead from locks, we
  // assume the main-thread path will be hot.
  void MaybeScheduleImmediateWork(const base::Location& from_here);

  // Requests that a delayed task to process work is posted on the main task
  // runner. These delayed tasks are de-duplicated. Must be called on the thread
  // this class was created on.
  void MaybeScheduleDelayedWork(const base::Location& from_here,
                                TimeDomain* requesting_time_domain,
                                base::TimeTicks now,
                                base::TimeTicks run_time);

  // Cancels a delayed task to process work at |run_time|, previously requested
  // with MaybeScheduleDelayedWork.
  void CancelDelayedWork(TimeDomain* requesting_time_domain,
                         base::TimeTicks run_time);

  // Set the number of tasks executed in a single invocation of the task queue
  // manager. Increasing the batch size can reduce the overhead of yielding
  // back to the main message loop -- at the cost of potentially delaying other
  // tasks posted to the main loop. The batch size is 1 by default.
  void SetWorkBatchSize(int work_batch_size);

  // These functions can only be called on the same thread that the task queue
  // manager executes its tasks on.
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer);
  void RemoveTaskObserver(base::MessageLoop::TaskObserver* task_observer);
  void AddTaskTimeObserver(TaskTimeObserver* task_time_observer);
  void RemoveTaskTimeObserver(TaskTimeObserver* task_time_observer);

  // Returns true if any task from a monitored task queue was was run since the
  // last call to GetAndClearSystemIsQuiescentBit.
  bool GetAndClearSystemIsQuiescentBit();

  // Creates a task queue with the given type, |spec| and args. Must be called
  // on the thread this class was created on.
  template <typename TaskQueueType, typename... Args>
  scoped_refptr<TaskQueueType> CreateTaskQueue(const TaskQueue::Spec& spec,
                                               Args&&... args) {
    scoped_refptr<TaskQueueType> task_queue(new TaskQueueType(
        CreateTaskQueueImpl(spec), std::forward<Args>(args)...));
    return task_queue;
  }

  class PLATFORM_EXPORT Observer {
   public:
    virtual ~Observer() {}

    virtual void OnTriedToExecuteBlockedTask() = 0;

    virtual void OnBeginNestedRunLoop() = 0;

    virtual void OnExitNestedRunLoop() = 0;
  };

  // Called once to set the Observer. This function is called on the main
  // thread. If |observer| is null, then no callbacks will occur.
  // Note |observer| is expected to outlive the SchedulerHelper.
  void SetObserver(Observer* observer);

  // Returns the delegate used by the TaskQueueManager.
  const scoped_refptr<TaskQueueManagerDelegate>& Delegate() const;

  // Time domains must be registered for the task queues to get updated.
  void RegisterTimeDomain(TimeDomain* time_domain);
  void UnregisterTimeDomain(TimeDomain* time_domain);

  RealTimeDomain* real_time_domain() const { return real_time_domain_.get(); }

  LazyNow CreateLazyNow() const;

  // Returns the currently executing TaskQueue if any. Must be called on the
  // thread this class was created on.
  internal::TaskQueueImpl* currently_executing_task_queue() const {
    DCHECK(main_thread_checker_.CalledOnValidThread());
    return currently_executing_task_queue_;
  }

  // Return number of pending tasks in task queues.
  size_t GetNumberOfPendingTasks() const;

  // Returns true if there is a task that could be executed immediately.
  bool HasImmediateWorkForTesting() const;

  // Removes all canceled delayed tasks.
  void SweepCanceledDelayedTasks();

  // Unregisters a TaskQueue previously created by |NewTaskQueue()|.
  // No tasks will run on this queue after this call.
  void UnregisterTaskQueueImpl(
      std::unique_ptr<internal::TaskQueueImpl> task_queue);

  base::WeakPtr<TaskQueueManager> GetWeakPtr();

 protected:
  friend class LazyNow;
  friend class internal::TaskQueueImpl;
  friend class task_queue_manager_unittest::TaskQueueManagerTest;

  // Intermediate data structure, used to compute NextDelayedDoWork.
  class NextTaskDelay {
   public:
    NextTaskDelay() : time_domain_(nullptr) {}

    using AllowAnyDelayForTesting = int;

    NextTaskDelay(base::TimeDelta delay, TimeDomain* time_domain)
        : delay_(delay), time_domain_(time_domain) {
      DCHECK_GT(delay, base::TimeDelta());
      DCHECK(time_domain);
    }

    NextTaskDelay(base::TimeDelta delay,
                  TimeDomain* time_domain,
                  AllowAnyDelayForTesting)
        : delay_(delay), time_domain_(time_domain) {
      DCHECK(time_domain);
    }

    base::TimeDelta Delay() const { return delay_; }
    TimeDomain* time_domain() const { return time_domain_; }

    bool operator>(const NextTaskDelay& other) const {
      return delay_ > other.delay_;
    }

    bool operator<(const NextTaskDelay& other) const {
      return delay_ < other.delay_;
    }

   private:
    base::TimeDelta delay_;
    TimeDomain* time_domain_;
  };

 private:
  // Represents a scheduled delayed DoWork (if any). Only public for testing.
  class NextDelayedDoWork {
   public:
    NextDelayedDoWork() : time_domain_(nullptr) {}
    NextDelayedDoWork(base::TimeTicks run_time, TimeDomain* time_domain)
        : run_time_(run_time), time_domain_(time_domain) {
      DCHECK_NE(run_time, base::TimeTicks());
      DCHECK(time_domain);
    }

    base::TimeTicks run_time() const { return run_time_; }
    TimeDomain* time_domain() const { return time_domain_; }

    void Clear() {
      run_time_ = base::TimeTicks();
      time_domain_ = nullptr;
    }

    explicit operator bool() const { return !run_time_.is_null(); }

   private:
    base::TimeTicks run_time_;
    TimeDomain* time_domain_;
  };

  // TaskQueueSelector::Observer implementation:
  void OnTaskQueueEnabled(internal::TaskQueueImpl* queue) override;
  void OnTriedToSelectBlockedWorkQueue(
      internal::WorkQueue* work_queue) override;

  // base::RunLoop::NestingObserver implementation:
  void OnBeginNestedRunLoop() override;

  // Called by the task queue to register a new pending task.
  void DidQueueTask(const internal::TaskQueueImpl::Task& pending_task);

  // Use the selector to choose a pending task and run it.
  void DoWork(bool delayed);

  // Post a DoWork continuation if |next_delay| is not empty.
  void PostDoWorkContinuationLocked(base::Optional<NextTaskDelay> next_delay,
                                    LazyNow* lazy_now,
                                    MoveableAutoLock lock);

  // Delayed Tasks with run_times <= Now() are enqueued onto the work queue and
  // reloads any empty work queues.
  void WakeUpReadyDelayedQueues(LazyNow* lazy_now);

  // Chooses the next work queue to service. Returns true if |out_queue|
  // indicates the queue from which the next task should be run, false to
  // avoid running any tasks.
  bool SelectWorkQueueToService(internal::WorkQueue** out_work_queue);

  enum class ProcessTaskResult {
    DEFERRED,
    EXECUTED,
    TASK_QUEUE_MANAGER_DELETED
  };

  // Runs a single nestable task from the |queue|. On exit, |out_task| will
  // contain the task which was executed. Non-nestable task are reposted on the
  // run loop. The queue must not be empty.  On exit |time_after_task| may get
  // set (not guaranteed), sampling |real_time_domain()->Now()| immediately
  // after running the task.
  ProcessTaskResult ProcessTaskFromWorkQueue(internal::WorkQueue* work_queue,
                                             bool is_nested,
                                             LazyNow time_before_task,
                                             base::TimeTicks* time_after_task);

  bool RunsTasksInCurrentSequence() const;
  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay);

  internal::EnqueueOrder GetNextSequenceNumber();

  // Calls DelayTillNextTask on all time domains and returns the smallest delay
  // requested if any.
  base::Optional<NextTaskDelay> ComputeDelayTillNextTaskLocked(
      LazyNow* lazy_now);

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
  AsValueWithSelectorResult(bool should_run,
                            internal::WorkQueue* selected_work_queue) const;

  void MaybeScheduleImmediateWorkLocked(const base::Location& from_here,
                                        MoveableAutoLock lock);

  // Adds |queue| to |any_thread().has_incoming_immediate_work_| and if
  // |queue_is_blocked| is false it makes sure a DoWork is posted.
  // Can be called from any thread.
  void OnQueueHasIncomingImmediateWork(internal::TaskQueueImpl* queue,
                                       internal::EnqueueOrder enqueue_order,
                                       bool queue_is_blocked);

  using IncomingImmediateWorkMap =
      std::unordered_map<internal::TaskQueueImpl*, internal::EnqueueOrder>;

  // Calls |ReloadImmediateWorkQueueIfEmpty| on all queues in
  // |queues_to_reload|.
  void ReloadEmptyWorkQueues(
      const IncomingImmediateWorkMap& queues_to_reload) const;

  std::unique_ptr<internal::TaskQueueImpl> CreateTaskQueueImpl(
      const TaskQueue::Spec& spec);

  // Deletes queues marked for deletion and empty queues marked for shutdown.
  void CleanUpQueues();

  std::set<TimeDomain*> time_domains_;
  std::unique_ptr<RealTimeDomain> real_time_domain_;

  // List of task queues managed by this TaskQueueManager.
  // - active_queues contains queues owned by relevant TaskQueues.
  // - queues_to_delete contains soon-to-be-deleted queues, because some
  // internal
  //   scheduling code does not expect queues to be pulled from underneath.

  std::set<internal::TaskQueueImpl*> active_queues_;
  std::map<internal::TaskQueueImpl*, std::unique_ptr<internal::TaskQueueImpl>>
      queues_to_delete_;

  internal::EnqueueOrderGenerator enqueue_order_generator_;
  base::debug::TaskAnnotator task_annotator_;

  base::ThreadChecker main_thread_checker_;
  scoped_refptr<TaskQueueManagerDelegate> delegate_;
  internal::TaskQueueSelector selector_;

  base::RepeatingClosure immediate_do_work_closure_;
  base::RepeatingClosure delayed_do_work_closure_;
  base::CancelableClosure cancelable_delayed_do_work_closure_;

  bool task_was_run_on_quiescence_monitored_queue_;

  struct AnyThread {
    AnyThread();

    // Task queues with newly available work on the incoming queue.
    IncomingImmediateWorkMap has_incoming_immediate_work;

    int do_work_running_count;
    int immediate_do_work_posted_count;
    bool is_nested;  // Whether or not the message loop is currently nested.
  };

  // TODO(alexclarke): Add a MainThreadOnly struct too.

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

  NextDelayedDoWork next_delayed_do_work_;

  int work_batch_size_;
  size_t task_count_;

  base::ObserverList<base::MessageLoop::TaskObserver> task_observers_;

  base::ObserverList<TaskTimeObserver> task_time_observers_;

  internal::TaskQueueImpl* currently_executing_task_queue_;  // NOT OWNED

  Observer* observer_;  // NOT OWNED
  base::WeakPtrFactory<TaskQueueManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueManager);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_H_
