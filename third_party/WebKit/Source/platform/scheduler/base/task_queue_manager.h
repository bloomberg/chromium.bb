// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_H_

#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "platform/PlatformExport.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/task_time_observer.h"
#include "platform/scheduler/base/time_domain.h"

namespace blink {
namespace scheduler {

class PLATFORM_EXPORT TaskQueueManager {
 public:
  // Keep TaskQueueManagerImpl in sync with this interface.
  // The general rule is not to expose methods only used in scheduler/base.
  // Try to keep interface as lean as possible.

  // Observer class. Always called back on the main thread.
  class PLATFORM_EXPORT Observer {
   public:
    virtual ~Observer() {}
    virtual void OnBeginNestedRunLoop() = 0;
    virtual void OnExitNestedRunLoop() = 0;
  };

  virtual ~TaskQueueManager() = default;

  // Forwards to TaskQueueManagerImpl::TakeOverCurrentThread.
  // TODO(kraynov): Any way to make it truly agnostic of TaskQueueManagerImpl?
  static std::unique_ptr<TaskQueueManager> TakeOverCurrentThread();

  // Should be called once, on main thread only.
  // If |null| is passed, no callbacks will occur.
  // Note: |observer| is expected to outlive the SchedulerHelper.
  // TODO(kraynov): Review these lifetime assumptions.
  virtual void SetObserver(Observer* observer) = 0;

  // These functions can only be called on the same thread that the task queue
  // manager executes its tasks on.
  virtual void AddTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) = 0;
  virtual void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) = 0;
  virtual void AddTaskTimeObserver(TaskTimeObserver* task_time_observer) = 0;
  virtual void RemoveTaskTimeObserver(TaskTimeObserver* task_time_observer) = 0;

  // Time domains must be registered for the task queues to get updated.
  virtual void RegisterTimeDomain(TimeDomain* time_domain) = 0;
  virtual void UnregisterTimeDomain(TimeDomain* time_domain) = 0;
  virtual RealTimeDomain* GetRealTimeDomain() const = 0;

  virtual base::TickClock* GetClock() const = 0;
  virtual base::TimeTicks NowTicks() const = 0;

  // Sets the SingleThreadTaskRunner that will be returned by
  // ThreadTaskRunnerHandle::Get and MessageLoop::current().task_runner() on the
  // thread associated with this TaskQueueManager.
  virtual void SetDefaultTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) = 0;

  // Removes all canceled delayed tasks.
  virtual void SweepCanceledDelayedTasks() = 0;

  // Returns true if any task from a monitored task queue was was run since the
  // last call to GetAndClearSystemIsQuiescentBit.
  virtual bool GetAndClearSystemIsQuiescentBit() = 0;

  // Set the number of tasks executed in a single invocation of the task queue
  // manager. Increasing the batch size can reduce the overhead of yielding
  // back to the main message loop -- at the cost of potentially delaying other
  // tasks posted to the main loop. The batch size is 1 by default.
  virtual void SetWorkBatchSize(int work_batch_size) = 0;

  virtual void EnableCrashKeys(const char* file_name_crash_key,
                               const char* function_name_crash_key) = 0;

  // Return number of pending tasks in task queues.
  // TODO(kraynov): Remove test-only method.
  virtual size_t GetNumberOfPendingTasks() const = 0;

  // Returns true if there is a task that could be executed immediately.
  // TODO(kraynov): Remove test-only method.
  virtual bool HasImmediateWorkForTesting() const = 0;

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

 protected:
  virtual std::unique_ptr<internal::TaskQueueImpl> CreateTaskQueueImpl(
      const TaskQueue::Spec& spec) = 0;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_MANAGER_H_
