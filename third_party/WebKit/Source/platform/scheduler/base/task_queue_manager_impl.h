// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_IMPL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_IMPL_H_

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
#include "platform/scheduler/base/sequenced_task_source.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/task_queue_manager.h"
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
class PLATFORM_EXPORT TaskQueueManagerImpl
    : public TaskQueueManager,
      public internal::SequencedTaskSource,
      public internal::TaskQueueSelector::Observer,
      public base::RunLoop::NestingObserver {
 public:
  // Keep public methods in sync with TaskQueueManager interface.
  // The general rule is to keep methods only used in scheduler/base just here
  // and not to define them in the interface.

  using Observer = TaskQueueManager::Observer;

  ~TaskQueueManagerImpl() override;

  // Assume direct control over current thread and create a TaskQueueManager.
  // This function should be called only once per thread.
  // This function assumes that a MessageLoop is initialized for current
  // thread.
  static std::unique_ptr<TaskQueueManagerImpl> TakeOverCurrentThread();

  // TaskQueueManager implementation:
  void SetObserver(Observer* observer) override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void AddTaskTimeObserver(TaskTimeObserver* task_time_observer) override;
  void RemoveTaskTimeObserver(TaskTimeObserver* task_time_observer) override;
  void RegisterTimeDomain(TimeDomain* time_domain) override;
  void UnregisterTimeDomain(TimeDomain* time_domain) override;
  RealTimeDomain* GetRealTimeDomain() const override;
  base::TickClock* GetClock() const override;
  base::TimeTicks NowTicks() const override;
  void SetDefaultTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void SweepCanceledDelayedTasks() override;
  bool GetAndClearSystemIsQuiescentBit() override;
  void SetWorkBatchSize(int work_batch_size) override;
  void EnableCrashKeys(const char* file_name_crash_key,
                       const char* function_name_crash_key) override;

  // Implementation of SequencedTaskSource:
  base::Optional<base::PendingTask> TakeTask() override;
  void DidRunTask() override;
  base::TimeDelta DelayTillNextTask(LazyNow* lazy_now) override;

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

  LazyNow CreateLazyNow() const;

  // Returns the currently executing TaskQueue if any. Must be called on the
  // thread this class was created on.
  internal::TaskQueueImpl* currently_executing_task_queue() const;

  // Unregisters a TaskQueue previously created by |NewTaskQueue()|.
  // No tasks will run on this queue after this call.
  void UnregisterTaskQueueImpl(
      std::unique_ptr<internal::TaskQueueImpl> task_queue);

  scoped_refptr<internal::GracefulQueueShutdownHelper>
  GetGracefulQueueShutdownHelper() const;

  base::WeakPtr<TaskQueueManagerImpl> GetWeakPtr();

 protected:
  // Create a task queue manager where |controller| controls the thread
  // on which the tasks are eventually run.
  explicit TaskQueueManagerImpl(
      std::unique_ptr<internal::ThreadController> controller);

  friend class internal::TaskQueueImpl;
  friend class TaskQueueManagerForTest;

 private:
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
  };

  // TODO(scheduler-dev): Review if we really need non-nestable tasks at all.
  struct NonNestableTask {
    internal::TaskQueueImpl::Task task;
    internal::TaskQueueImpl* task_queue;
    WorkType work_type;
  };
  using NonNestableTaskDeque = WTF::Deque<NonNestableTask, 8>;

  // We have to track rentrancy because we support nested runloops but the
  // selector interface is unaware of those.  This struct keeps track off all
  // task related state needed to make pairs of TakeTask() / DidRunTask() work.
  struct ExecutingTask {
    ExecutingTask()
        : pending_task(
              TaskQueue::PostedTask(base::OnceClosure(), base::Location()),
              base::TimeTicks(),
              0) {}

    ExecutingTask(internal::TaskQueueImpl::Task&& pending_task,
                  internal::TaskQueueImpl* task_queue)
        : pending_task(std::move(pending_task)), task_queue(task_queue) {}

    internal::TaskQueueImpl::Task pending_task;
    internal::TaskQueueImpl* task_queue = nullptr;
    base::TimeTicks task_start_time;
    base::ThreadTicks task_start_thread_time;
    bool should_record_thread_time = false;
  };

  struct MainThreadOnly {
    MainThreadOnly();

    int nesting_depth = 0;
    NonNestableTaskDeque non_nestable_task_queue;
    // TODO(altimin): Switch to instruction pointer crash key when it's
    // available.
    base::debug::CrashKeyString* file_name_crash_key = nullptr;
    base::debug::CrashKeyString* function_name_crash_key = nullptr;

    std::mt19937_64 random_generator;
    std::uniform_real_distribution<double> uniform_distribution;

    internal::TaskQueueSelector selector;
    base::ObserverList<base::MessageLoop::TaskObserver> task_observers;
    base::ObserverList<TaskTimeObserver> task_time_observers;
    std::set<TimeDomain*> time_domains;
    std::unique_ptr<RealTimeDomain> real_time_domain;

    // List of task queues managed by this TaskQueueManager.
    // - active_queues contains queues that are still running tasks.
    //   Most often they are owned by relevant TaskQueues, but
    //   queues_to_gracefully_shutdown_ are included here too.
    // - queues_to_gracefully_shutdown contains queues which should be deleted
    //   when they become empty.
    // - queues_to_delete contains soon-to-be-deleted queues, because some
    //   internal scheduling code does not expect queues to be pulled
    //   from underneath.

    std::set<internal::TaskQueueImpl*> active_queues;
    std::map<internal::TaskQueueImpl*, std::unique_ptr<internal::TaskQueueImpl>>
        queues_to_gracefully_shutdown;
    std::map<internal::TaskQueueImpl*, std::unique_ptr<internal::TaskQueueImpl>>
        queues_to_delete;

    bool task_was_run_on_quiescence_monitored_queue = false;

    // Due to nested runloops more than one task can be executing concurrently.
    std::list<ExecutingTask> task_execution_stack;

    Observer* observer = nullptr;  // NOT OWNED
  };

  // TaskQueueSelector::Observer:
  void OnTaskQueueEnabled(internal::TaskQueueImpl* queue) override;

  // base::RunLoop::NestingObserver:
  void OnBeginNestedRunLoop() override;
  void OnExitNestedRunLoop() override;

  // Called by the task queue to register a new pending task.
  void DidQueueTask(const internal::TaskQueueImpl::Task& pending_task);

  // Delayed Tasks with run_times <= Now() are enqueued onto the work queue and
  // reloads any empty work queues.
  void WakeUpReadyDelayedQueues(LazyNow* lazy_now);

  void NotifyWillProcessTask(ExecutingTask* task, LazyNow* time_before_task);
  void NotifyDidProcessTask(const ExecutingTask& task,
                            LazyNow* time_after_task);

  internal::EnqueueOrder GetNextSequenceNumber();

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
  AsValueWithSelectorResult(bool should_run,
                            internal::WorkQueue* selected_work_queue) const;

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
      const TaskQueue::Spec& spec) override;

  void TakeQueuesToGracefullyShutdownFromHelper();

  // Deletes queues marked for deletion and empty queues marked for shutdown.
  void CleanUpQueues();

  bool ShouldRecordCPUTimeForTask();

  const scoped_refptr<internal::GracefulQueueShutdownHelper>
      graceful_shutdown_helper_;

  internal::EnqueueOrderGenerator enqueue_order_generator_;

  std::unique_ptr<internal::ThreadController> controller_;

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

  THREAD_CHECKER(main_thread_checker_);
  MainThreadOnly main_thread_only_;
  MainThreadOnly& main_thread_only() {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    return main_thread_only_;
  }
  const MainThreadOnly& main_thread_only() const {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    return main_thread_only_;
  }

  base::WeakPtrFactory<TaskQueueManagerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueManagerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_IMPL_H_
