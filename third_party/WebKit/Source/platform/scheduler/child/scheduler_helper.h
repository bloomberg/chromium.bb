// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_SCHEDULER_HELPER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_SCHEDULER_HELPER_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/time/tick_clock.h"
#include "platform/scheduler/base/task_queue_manager.h"
#include "platform/scheduler/base/task_queue_selector.h"

namespace blink {
namespace scheduler {

class SchedulerTqmDelegate;

// Common scheduler functionality for default tasks.
class PLATFORM_EXPORT SchedulerHelper : public TaskQueueManager::Observer {
 public:
  explicit SchedulerHelper(
      scoped_refptr<SchedulerTqmDelegate> task_queue_manager_delegate);
  ~SchedulerHelper() override;

  // There is a small overhead to recording task delay histograms, we may not
  // wish to do this on all threads.
  void SetRecordTaskDelayHistograms(bool record_task_delay_histograms);

  // TaskQueueManager::Observer implementation:
  void OnTriedToExecuteBlockedTask() override;
  void OnBeginNestedRunLoop() override;

  // Returns the default task queue.
  virtual scoped_refptr<TaskQueue> DefaultTaskQueue() = 0;

  // Returns the control task queue.  Tasks posted to this queue are executed
  // with the highest priority. Care must be taken to avoid starvation of other
  // task queues.
  virtual scoped_refptr<TaskQueue> ControlTaskQueue() = 0;

  // Adds or removes a task observer from the scheduler. The observer will be
  // notified before and after every executed task. These functions can only be
  // called on the thread this class was created on.
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer);
  void RemoveTaskObserver(base::MessageLoop::TaskObserver* task_observer);

  void AddTaskTimeObserver(TaskTimeObserver* task_time_observer);
  void RemoveTaskTimeObserver(TaskTimeObserver* task_time_observer);

  // Shuts down the scheduler by dropping any remaining pending work in the work
  // queues. After this call any work posted to the task queue will be
  // silently dropped.
  void Shutdown();

  // Returns true if Shutdown() has been called. Otherwise returns false.
  bool IsShutdown() const { return !task_queue_manager_.get(); }

  inline void CheckOnValidThread() const {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  class PLATFORM_EXPORT Observer {
   public:
    virtual ~Observer() {}

    // Called when the scheduler tried to execute a task from a disabled
    // queue. See TaskQueue::Spec::SetShouldReportWhenExecutionBlocked.
    virtual void OnTriedToExecuteBlockedTask() = 0;

    // Called when scheduler executes task with nested run loop.
    virtual void OnBeginNestedRunLoop() = 0;
  };

  // Called once to set the Observer. This function is called on the main
  // thread. If |observer| is null, then no callbacks will occur.
  // Note |observer| is expected to outlive the SchedulerHelper.
  void SetObserver(Observer* observer);

  // Remove all canceled delayed tasks.
  void SweepCanceledDelayedTasks();

  // Accessor methods.
  RealTimeDomain* real_time_domain() const;
  void RegisterTimeDomain(TimeDomain* time_domain);
  void UnregisterTimeDomain(TimeDomain* time_domain);
  const scoped_refptr<SchedulerTqmDelegate>& scheduler_tqm_delegate() const;
  bool GetAndClearSystemIsQuiescentBit();

  size_t GetNumberOfPendingTasks() const;

  // Test helpers.
  void SetWorkBatchSizeForTesting(size_t work_batch_size);
  TaskQueueManager* GetTaskQueueManagerForTesting();

 protected:
  void InitDefaultQueues(scoped_refptr<TaskQueue> default_task_queue,
                         scoped_refptr<TaskQueue> control_task_queue);

  base::ThreadChecker thread_checker_;
  scoped_refptr<SchedulerTqmDelegate> task_queue_manager_delegate_;
  std::unique_ptr<TaskQueueManager> task_queue_manager_;

 private:
  friend class SchedulerHelperTest;

  Observer* observer_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(SchedulerHelper);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_SCHEDULER_HELPER_H_
