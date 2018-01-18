// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_H_

#include <map>
#include <random>

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
#include "platform/scheduler/base/graceful_queue_shutdown_helper.h"
#include "platform/scheduler/base/moveable_auto_lock.h"
#include "platform/scheduler/base/sequence.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/task_queue_selector.h"

namespace base {
namespace debug {
struct CrashKeyString;
}  // namespace debug

namespace trace_event {
class ConvertableToTraceFormat;
}  // namespace trace_event
}  // namespace base

namespace blink {
namespace scheduler {

namespace internal {
class TaskQueueImpl;
class ThreadController;
}  // namespace internal

namespace task_queue_manager_unittest {
class TaskQueueManagerTest;
}  // namespace task_queue_manager_unittest

class LazyNow;
class RealTimeDomain;
class TaskQueue;
class TaskTimeObserver;
class TimeDomain;

// The task queue manager provides N task queues and a selector interface for
// choosing which task queue to service next. Each task queue consists of two
// sub queues:
//
// 1. Incoming task queue. Tasks that are posted get immediately appended here.
//    When a task is appended into an empty incoming queue, the task manager
//    work function (DoWork()) is scheduled to run on the main task runner.
//
// 2. Work queue. If a work queue is empty when DoWork() is entered, tasks from
//    the incoming task queue (if any) are moved here. The work queues are
//    registered with the selector as input to the scheduling decision.
//
// TODO(altimin): Split TaskQueueManager into TaskQueueManager and
// TaskQueueManagerImpl.
class PLATFORM_EXPORT TaskQueueManager
    : public internal::Sequence,
      public internal::TaskQueueSelector::Observer,
      public base::RunLoop::NestingObserver {
 public:
  // Observer class that is called back on the main thread.
  class PLATFORM_EXPORT Observer {
   public:
    virtual ~Observer() {}

    virtual void OnTriedToExecuteBlockedTask() = 0;

    virtual void OnBeginNestedRunLoop() = 0;

    virtual void OnExitNestedRunLoop() = 0;
  };

  ~TaskQueueManager() override;

  // Assume direct control over current thread and create a TaskQueueManager.
  // This function should be called only once per thread.
  // This function assumes that a MessageLoop is initialized for current
  // thread.
  static std::unique_ptr<TaskQueueManager> TakeOverCurrentThread();

  // Sets the SingleThreadTaskRunner that will be returned by
  // ThreadTaskRunnerHandle::Get and MessageLoop::current().task_runner() on the
  // thread associated with this TaskQueueManager.
  void SetDefaultTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Implementation of Sequence:
  base::Optional<base::PendingTask> TakeTask(WorkType work_type) override;
  bool DidRunTask() override;

  // Requests that a task to process work is posted on the main task runner.
  // These tasks are de-duplicated in two buckets: main-thread and all other
  // threads. This distinction is done to reduce the overhead from locks, we
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
  // TODO(altimin): TaskQueueManager should not create TaskQueues.
  template <typename TaskQueueType, typename... Args>
  scoped_refptr<TaskQueueType> CreateTaskQueue(const TaskQueue::Spec& spec,
                                               Args&&... args) {
    scoped_refptr<TaskQueueType> task_queue(new TaskQueueType(
        CreateTaskQueueImpl(spec), spec, std::forward<Args>(args)...));
    return task_queue;
  }

  void EnableCrashKeys(const char* file_name_crash_key,
                       const char* function_name_crash_key);

  // Called once to set the Observer. This function is called on the main
  // thread. If |observer| is null, then no callbacks will occur.
  // Note: |observer| is expected to outlive the SchedulerHelper.
  void SetObserver(Observer* observer);

  // Time domains must be registered for the task queues to get updated.
  void RegisterTimeDomain(TimeDomain* time_domain);
  void UnregisterTimeDomain(TimeDomain* time_domain);

  RealTimeDomain* real_time_domain() const { return real_time_domain_.get(); }

  LazyNow CreateLazyNow() const;

  // Returns the currently executing TaskQueue if any. Must be called on the
  // thread this class was created on.
  internal::TaskQueueImpl* currently_executing_task_queue() const {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
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

  scoped_refptr<internal::GracefulQueueShutdownHelper>
  GetGracefulQueueShutdownHelper() const;

  base::TickClock* GetClock() const;
  base::TimeTicks NowTicks() const;

  base::WeakPtr<TaskQueueManager> GetWeakPtr();

 protected:
  // Create a task queue manager where |controller| controls the thread
  // on which the tasks are eventually run.
  explicit TaskQueueManager(
      std::unique_ptr<internal::ThreadController> controller);

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

    base::TimeDelta delay() const { return delay_; }
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

 protected:
  // Protected functions for testing.

  size_t ActiveQueuesCount() { return active_queues_.size(); }

  size_t QueuesToShutdownCount() {
    TakeQueuesToGracefullyShutdownFromHelper();
    return queues_to_gracefully_shutdown_.size();
  }

  size_t QueuesToDeleteCount() { return queues_to_delete_.size(); }

  void SetRandomSeed(uint64_t seed);

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

  enum class ProcessTaskResult {
    kDeferred,
    kExecuted,
    kTaskQueueManagerDeleted,
  };

  using IncomingImmediateWorkMap =
      std::unordered_map<internal::TaskQueueImpl*, internal::EnqueueOrder>;

  struct AnyThread {
    AnyThread() = default;

    // Task queues with newly available work on the incoming queue.
    IncomingImmediateWorkMap has_incoming_immediate_work;

    int do_work_running_count = 0;
    int immediate_do_work_posted_count = 0;
    int nesting_depth = 0;
  };

  // TODO(scheduler-dev): Review if we really need non-nestable tasks at all.
  struct NonNestableTask {
    internal::TaskQueueImpl::Task task;
    internal::TaskQueueImpl* task_queue;
    Sequence::WorkType work_type;
  };
  using NonNestableTaskDeque = WTF::Deque<NonNestableTask, 8>;

  // TODO(alexclarke): Move more things into MainThreadOnly
  struct MainThreadOnly {
    MainThreadOnly() = default;

    int nesting_depth = 0;
    NonNestableTaskDeque non_nestable_task_queue;
    // TODO(altimin): Switch to instruction pointer crash key when it's
    // available.
    base::debug::CrashKeyString* file_name_crash_key = nullptr;
    base::debug::CrashKeyString* function_name_crash_key = nullptr;
  };

  // TaskQueueSelector::Observer:
  void OnTaskQueueEnabled(internal::TaskQueueImpl* queue) override;
  void OnTriedToSelectBlockedWorkQueue(
      internal::WorkQueue* work_queue) override;

  // base::RunLoop::NestingObserver:
  void OnBeginNestedRunLoop() override;
  void OnExitNestedRunLoop() override;

  // Called by the task queue to register a new pending task.
  void DidQueueTask(const internal::TaskQueueImpl::Task& pending_task);

  // Use the selector to choose a pending task and run it.
  void DoWork(WorkType work_type);

  // Post a DoWork() continuation if |next_delay| is not empty.
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

  // Runs a single nestable task from the |queue|. On exit, |out_task| will
  // contain the task which was executed. Non-nestable task are reposted on the
  // run loop. The queue must not be empty. On exit |time_after_task| may get
  // set (not guaranteed), sampling |real_time_domain()->Now()| immediately
  // after running the task.
  ProcessTaskResult ProcessTaskFromWorkQueue(internal::WorkQueue* work_queue,
                                             LazyNow time_before_task,
                                             base::TimeTicks* time_after_task);

  void NotifyWillProcessTaskObservers(const internal::TaskQueueImpl::Task& task,
                                      internal::TaskQueueImpl* queue,
                                      LazyNow time_before_task,
                                      base::TimeTicks* task_start_time);

  void NotifyDidProcessTaskObservers(
      const internal::TaskQueueImpl::Task& task,
      internal::TaskQueueImpl* queue,
      base::Optional<base::TimeDelta> thread_time,
      base::TimeTicks task_start_time,
      base::TimeTicks* time_after_task);

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

  // Calls |ReloadImmediateWorkQueueIfEmpty| on all queues in
  // |queues_to_reload|.
  void ReloadEmptyWorkQueues(
      const IncomingImmediateWorkMap& queues_to_reload) const;

  std::unique_ptr<internal::TaskQueueImpl> CreateTaskQueueImpl(
      const TaskQueue::Spec& spec);

  void TakeQueuesToGracefullyShutdownFromHelper();

  // Deletes queues marked for deletion and empty queues marked for shutdown.
  void CleanUpQueues();

  bool ShouldRecordCPUTimeForTask();

  std::set<TimeDomain*> time_domains_;
  std::unique_ptr<RealTimeDomain> real_time_domain_;

  // List of task queues managed by this TaskQueueManager.
  // - active_queues contains queues that are still running tasks.
  //   Most often they are owned by relevant TaskQueues, but
  //   queues_to_gracefully_shutdown_ are included here too.
  // - queues_to_gracefully_shutdown contains queues which should be deleted
  //   when they become empty.
  // - queues_to_delete contains soon-to-be-deleted queues, because some
  //   internal scheduling code does not expect queues to be pulled
  //   from underneath.

  std::set<internal::TaskQueueImpl*> active_queues_;
  std::map<internal::TaskQueueImpl*, std::unique_ptr<internal::TaskQueueImpl>>
      queues_to_gracefully_shutdown_;
  std::map<internal::TaskQueueImpl*, std::unique_ptr<internal::TaskQueueImpl>>
      queues_to_delete_;

  const scoped_refptr<internal::GracefulQueueShutdownHelper>
      graceful_shutdown_helper_;

  internal::EnqueueOrderGenerator enqueue_order_generator_;
  base::debug::TaskAnnotator task_annotator_;

  THREAD_CHECKER(main_thread_checker_);
  std::unique_ptr<internal::ThreadController> controller_;
  internal::TaskQueueSelector selector_;

  std::mt19937_64 random_generator_;
  std::uniform_real_distribution<double> uniform_distribution_;

  bool task_was_run_on_quiescence_monitored_queue_ = false;

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

  // A check to bail out early during memory corruption.
  // crbug.com/757940
  bool Validate();

  int32_t memory_corruption_sentinel_;

  MainThreadOnly main_thread_only_;
  MainThreadOnly& main_thread_only() {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    return main_thread_only_;
  }
  const MainThreadOnly& main_thread_only() const {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    return main_thread_only_;
  }

  NextDelayedDoWork next_delayed_do_work_;

  int work_batch_size_ = 1;
  size_t task_count_ = 0;

  base::ObserverList<base::MessageLoop::TaskObserver> task_observers_;

  base::ObserverList<TaskTimeObserver> task_time_observers_;

  // NOT OWNED
  internal::TaskQueueImpl* currently_executing_task_queue_ = nullptr;

  Observer* observer_ = nullptr;  // NOT OWNED
  base::WeakPtrFactory<TaskQueueManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueManager);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_H_
