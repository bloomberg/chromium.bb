// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TASK_RUNNER_H_
#define PLATFORM_IMPL_TASK_RUNNER_H_

#include <atomic>
#include <condition_variable>  // NOLINT
#include <map>
#include <memory>
#include <mutex>  // NOLINT
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/types/optional.h"
#include "platform/api/network_runner.h"
#include "platform/api/time.h"
#include "platform/api/trace_logging.h"
#include "platform/base/error.h"

namespace openscreen {
namespace platform {

class TaskRunnerImpl final : public TaskRunner {
 public:
  using Task = TaskRunner::Task;

  class TaskWaiter {
   public:
    virtual ~TaskWaiter() = default;

    // These calls should be thread-safe.  The absolute minimum is that
    // OnTaskPosted must be safe to call from another thread while this is
    // inside WaitForTaskToBePosted.  NOTE: There may be spurious wakeups from
    // WaitForTaskToBePosted depending on whether the specific implementation
    // chooses to clear queued WakeUps before entering WaitForTaskToBePosted.

    // Blocks until some event occurs, which means new tasks may have been
    // posted.  Wait may only block up to |timeout| where 0 means don't block at
    // all (not block forever).
    virtual Error WaitForTaskToBePosted(Clock::duration timeout) = 0;

    // If a WaitForTaskToBePosted call is currently blocking, unblock it
    // immediately.
    virtual void OnTaskPosted() = 0;
  };

  explicit TaskRunnerImpl(
      platform::ClockNowFunctionPtr now_function,
      TaskWaiter* event_waiter = nullptr,
      Clock::duration waiter_timeout = std::chrono::milliseconds(100));

  // TaskRunner overrides
  ~TaskRunnerImpl() final;
  void PostPackagedTask(Task task) final;
  void PostPackagedTaskWithDelay(Task task, Clock::duration delay) final;

  // Tasks will only be executed if RunUntilStopped has been called, and
  // RequestStopSoon has not. Important note: TaskRunner does NOT do any
  // threading, so calling "RunUntilStopped()" will block whatever thread you
  // are calling it on.
  void RunUntilStopped();

  // Thread-safe method for requesting the TaskRunner to stop running. This sets
  // a flag that will get checked in the run loop, typically after completing
  // the current task.
  void RequestStopSoon();

  // Execute all tasks immediately, useful for testing only. Note: this method
  // will schedule any delayed tasks that are ready to run, but does not block
  // waiting for delayed tasks to become eligible.
  void RunUntilIdleForTesting();

 private:
#ifndef TRACE_FORCE_DISABLE
  // Wrapper around a Task used to store the TraceId Metadata along with the
  // task itself, and to set the current TraceIdHierarchy before executing the
  // task.
  class TaskWithMetadata {
   public:
    // NOTE: 'explicit' keyword omitted so that conversion construtor can be
    // used. This simplifies switching between 'Task' and 'TaskWithMetadata'
    // based on the compilation flag.
    TaskWithMetadata(Task task)
        : task_(std::move(task)), trace_ids_(TRACE_HIERARCHY){};

    void operator()() {
      TRACE_SET_HIERARCHY(trace_ids_);
      std::move(task_)();
    }

   private:
    Task task_;
    TraceIdHierarchy trace_ids_;
  };
#else   // TRACE_FORCE_DISABLE defined
  using TaskWithMetadata = Task;
#endif  // TRACE_FORCE_DISABLE

  // Run all tasks already in the task queue. If the queue is empty, wait for
  // either (1) a delayed task to become available, or (2) a task to be added
  // to the queue.
  void RunCurrentTasksBlocking();

  // Run tasks already in the queue, for testing. If the queue is empty, this
  // method does not block but instead returns immediately.
  void RunCurrentTasksForTesting();

  // Loop method that runs tasks in the current thread, until the
  // RequestStopSoon method is called.
  void RunTasksUntilStopped();

  // Look at all tasks in the delayed task queue, then schedule them if the
  // minimum delay time has elapsed.
  void ScheduleDelayedTasks();

  // Look at the current state of the TaskRunner and determine if the run loop
  // should be woken up
  bool ShouldWakeUpRunLoop();

  // Takes the task_mutex_ lock, returning immediately if work is available. If
  // no work is available, this places the task running thread into a waiting
  // state until we stop running or work is available.
  std::unique_lock<std::mutex> WaitForWorkAndAcquireLock();

  const platform::ClockNowFunctionPtr now_function_;

  // Atomic so that we can perform atomic exchanges.
  std::atomic_bool is_running_;

  // This mutex is used for |tasks_| and |delayed_tasks_|, and also for
  // notifying the run loop to wake up when it is waiting for a task to be added
  // to the queue in |run_loop_wakeup_|.
  std::mutex task_mutex_;
  std::vector<TaskWithMetadata> tasks_ GUARDED_BY(task_mutex_);
  std::multimap<Clock::time_point, TaskWithMetadata> delayed_tasks_
      GUARDED_BY(task_mutex_);

  // When |task_waiter_| is nullptr, |run_loop_wakeup_| is used for sleeping the
  // task runner.  Otherwise, |run_loop_wakeup_| isn't used and |task_waiter_|
  // is used instead (along with |waiter_timeout_|).
  std::condition_variable run_loop_wakeup_;
  TaskWaiter* const task_waiter_;
  Clock::duration waiter_timeout_;

  // To prevent excessive re-allocation of the underlying array of the |tasks_|
  // vector, use an A/B vector-swap mechanism. |running_tasks_| starts out
  // empty, and is swapped with |tasks_| when it is time to run the Tasks.
  std::vector<TaskWithMetadata> running_tasks_;

  OSP_DISALLOW_COPY_AND_ASSIGN(TaskRunnerImpl);
};
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TASK_RUNNER_H_
