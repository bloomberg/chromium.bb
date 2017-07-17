// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_H_

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "platform/PlatformExport.h"

namespace base {
namespace trace_event {
class BlameContext;
}
}

namespace blink {
namespace scheduler {

namespace internal {
class TaskQueueImpl;
}

class TimeDomain;

class PLATFORM_EXPORT TaskQueue : public base::SingleThreadTaskRunner {
 public:
  class PLATFORM_EXPORT Observer {
   public:
    virtual ~Observer() {}

    // Notify observer that the time at which this queue wants to run
    // the next task has changed. |next_wakeup| can be in the past
    // (e.g. base::TimeTicks() can be used to notify about immediate work).
    // Can be called on any thread
    // All methods but SetObserver, SetTimeDomain and GetTimeDomain can be
    // called on |queue|.
    //
    // TODO(altimin): Make it base::Optional<base::TimeTicks> to tell
    // observer about cancellations.
    virtual void OnQueueNextWakeUpChanged(TaskQueue* queue,
                                          base::TimeTicks next_wake_up) = 0;
  };

  // Unregisters the task queue after which no tasks posted to it will run and
  // the TaskQueueManager's reference to it will be released soon.
  virtual void UnregisterTaskQueue();

  enum QueuePriority {
    // Queues with control priority will run before any other queue, and will
    // explicitly starve other queues. Typically this should only be used for
    // private queues which perform control operations.
    CONTROL_PRIORITY,

    // The selector will prioritize high over normal and low and normal over
    // low. However it will ensure neither of the lower priority queues can be
    // completely starved by higher priority tasks. All three of these queues
    // will always take priority over and can starve the best effort queue.
    HIGH_PRIORITY,
    // Queues with normal priority are the default.
    NORMAL_PRIORITY,
    LOW_PRIORITY,

    // Queues with best effort priority will only be run if all other queues are
    // empty. They can be starved by the other queues.
    BEST_EFFORT_PRIORITY,
    // Must be the last entry.
    QUEUE_PRIORITY_COUNT,
    FIRST_QUEUE_PRIORITY = CONTROL_PRIORITY,
  };

  // Can be called on any thread.
  static const char* PriorityToString(QueuePriority priority);

  // Options for constructing a TaskQueue.
  struct Spec {
    explicit Spec(const char* name)
        : name(name),
          should_monitor_quiescence(false),
          time_domain(nullptr),
          should_notify_observers(true),
          should_report_when_execution_blocked(false) {}

    Spec SetShouldMonitorQuiescence(bool should_monitor) {
      should_monitor_quiescence = should_monitor;
      return *this;
    }

    Spec SetShouldNotifyObservers(bool run_observers) {
      should_notify_observers = run_observers;
      return *this;
    }

    Spec SetTimeDomain(TimeDomain* domain) {
      time_domain = domain;
      return *this;
    }

    // See TaskQueueManager::Observer::OnTriedToExecuteBlockedTask.
    Spec SetShouldReportWhenExecutionBlocked(bool should_report) {
      should_report_when_execution_blocked = should_report;
      return *this;
    }

    const char* name;
    bool should_monitor_quiescence;
    TimeDomain* time_domain;
    bool should_notify_observers;
    bool should_report_when_execution_blocked;
  };

  // Interface to pass per-task metadata to RendererScheduler.
  class PLATFORM_EXPORT Task : public base::PendingTask {
   public:
    Task(const tracked_objects::Location& posted_from,
         base::OnceClosure task,
         base::TimeTicks desired_run_time,
         bool nestable);
  };

  // An interface that lets the owner vote on whether or not the associated
  // TaskQueue should be enabled.
  class QueueEnabledVoter {
   public:
    QueueEnabledVoter() {}
    virtual ~QueueEnabledVoter() {}

    // Votes to enable or disable the associated TaskQueue. The TaskQueue will
    // only be enabled if all the voters agree it should be enabled, or if there
    // are no voters.
    // NOTE this must be called on the thread the associated TaskQueue was
    // created on.
    virtual void SetQueueEnabled(bool enabled) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(QueueEnabledVoter);
  };

  // Returns an interface that allows the caller to vote on whether or not this
  // TaskQueue is enabled. The TaskQueue will be enabled if there are no voters
  // or if all agree it should be enabled.
  // NOTE this must be called on the thread this TaskQueue was created by.
  std::unique_ptr<QueueEnabledVoter> CreateQueueEnabledVoter();

  // NOTE this must be called on the thread this TaskQueue was created by.
  bool IsQueueEnabled() const;

  // Returns true if the queue is completely empty.
  bool IsEmpty() const;

  // Returns the number of pending tasks in the queue.
  size_t GetNumberOfPendingTasks() const;

  // Returns true if the queue has work that's ready to execute now.
  // NOTE: this must be called on the thread this TaskQueue was created by.
  bool HasTaskToRunImmediately() const;

  // Returns requested run time of next scheduled wake-up for a delayed task
  // which is not ready to run. If there are no such tasks or the queue is
  // disabled (by a QueueEnabledVoter) it returns base::nullopt.
  // NOTE: this must be called on the thread this TaskQueue was created by.
  base::Optional<base::TimeTicks> GetNextScheduledWakeUp();

  // Can be called on any thread.
  virtual const char* GetName() const;

  // Set the priority of the queue to |priority|. NOTE this must be called on
  // the thread this TaskQueue was created by.
  void SetQueuePriority(QueuePriority priority);

  // Returns the current queue priority.
  QueuePriority GetQueuePriority() const;

  // These functions can only be called on the same thread that the task queue
  // manager executes its tasks on.
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer);
  void RemoveTaskObserver(base::MessageLoop::TaskObserver* task_observer);

  // Set the blame context which is entered and left while executing tasks from
  // this task queue. |blame_context| must be null or outlive this task queue.
  // Must be called on the thread this TaskQueue was created by.
  void SetBlameContext(base::trace_event::BlameContext* blame_context);

  // Removes the task queue from the previous TimeDomain and adds it to
  // |domain|.  This is a moderately expensive operation.
  void SetTimeDomain(TimeDomain* domain);

  // Returns the queue's current TimeDomain.  Can be called from any thread.
  TimeDomain* GetTimeDomain() const;

  enum class InsertFencePosition {
    NOW,  // Tasks posted on the queue up till this point further may run.
          // All further tasks are blocked.
    BEGINNING_OF_TIME,  // No tasks posted on this queue may run.
  };

  // Inserts a barrier into the task queue which prevents tasks with an enqueue
  // order greater than the fence from running until either the fence has been
  // removed or a subsequent fence has unblocked some tasks within the queue.
  // Note: delayed tasks get their enqueue order set once their delay has
  // expired, and non-delayed tasks get their enqueue order set when posted.
  void InsertFence(InsertFencePosition position);

  // Removes any previously added fence and unblocks execution of any tasks
  // blocked by it.
  void RemoveFence();

  bool HasFence() const;

  // Returns true if the queue has a fence which is blocking execution of tasks.
  bool BlockedByFence() const;

  void SetObserver(Observer* observer);

  // base::SingleThreadTaskRunner implementation
  bool RunsTasksInCurrentSequence() const override;
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override;

 protected:
  explicit TaskQueue(std::unique_ptr<internal::TaskQueueImpl> impl);
  ~TaskQueue() override;

  internal::TaskQueueImpl* GetTaskQueueImpl() const { return impl_.get(); }

 private:
  friend class internal::TaskQueueImpl;
  friend class TaskQueueManager;

  friend class TaskQueueThrottlerTest;

  const std::unique_ptr<internal::TaskQueueImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueue);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_H_
