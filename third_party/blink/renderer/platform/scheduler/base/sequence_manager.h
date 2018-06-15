// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_SEQUENCE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_SEQUENCE_MANAGER_H_

#include <memory>
#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/sequence_manager/task_time_observer.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/base/real_time_domain.h"
#include "third_party/blink/renderer/platform/scheduler/base/task_queue_impl_forward.h"
#include "third_party/blink/renderer/platform/scheduler/base/time_domain.h"

namespace base {
namespace sequence_manager {

// SequenceManager manages TaskQueues which have different properties
// (e.g. priority, common task type) multiplexing all posted tasks into
// a single backing sequence (currently bound to a single thread, which is
// refererred as *main thread* in the comments below). SequenceManager
// implementation can be used in a various ways to apply scheduling logic.
class PLATFORM_EXPORT SequenceManager {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    // Called back on the main thread.
    virtual void OnBeginNestedRunLoop() = 0;
    virtual void OnExitNestedRunLoop() = 0;
  };

  virtual ~SequenceManager() = default;

  // Create SequenceManager using MessageLoop on the current thread.
  static std::unique_ptr<SequenceManager> CreateOnCurrentThread();

  // Must be called once, on the main thread.
  // Observer is expected to outlive the SequenceManager.
  virtual void SetObserver(Observer* observer) = 0;

  // Must be called on the main thread.
  virtual void AddTaskObserver(MessageLoop::TaskObserver* task_observer) = 0;
  virtual void RemoveTaskObserver(MessageLoop::TaskObserver* task_observer) = 0;
  virtual void AddTaskTimeObserver(TaskTimeObserver* task_time_observer) = 0;
  virtual void RemoveTaskTimeObserver(TaskTimeObserver* task_time_observer) = 0;

  // TaskQueues have their TimeDomain which must be registered in advance.
  virtual void RegisterTimeDomain(TimeDomain* time_domain) = 0;
  virtual void UnregisterTimeDomain(TimeDomain* time_domain) = 0;

  // TODO(kraynov): Upcast to TimeDomain.
  virtual RealTimeDomain* GetRealTimeDomain() const = 0;
  virtual const TickClock* GetTickClock() const = 0;
  virtual TimeTicks NowTicks() const = 0;

  // Sets the SingleThreadTaskRunner that will be returned by
  // ThreadTaskRunnerHandle::Get on the main thread.
  virtual void SetDefaultTaskRunner(
      scoped_refptr<SingleThreadTaskRunner> task_runner) = 0;

  // Removes all canceled delayed tasks.
  virtual void SweepCanceledDelayedTasks() = 0;

  // Some TaskQueues do monitor a quiescent state, so this method returns
  // true if any such queue has ran a task since the last call.
  virtual bool GetAndClearSystemIsQuiescentBit() = 0;

  // Set the number of tasks executed in a single SequenceManager invocation.
  // Increasing this number reduces the overhead of the tasks dispatching
  // logic at the cost of a potentially worse latency. 1 by default.
  virtual void SetWorkBatchSize(int work_batch_size) = 0;

  virtual void EnableCrashKeys(const char* file_name_crash_key,
                               const char* function_name_crash_key) = 0;

  // Returns the portion of tasks for which CPU time is recorded or 0 if not
  // sampled.
  virtual double GetSamplingRateForRecordingCPUTime() const = 0;

  // Creates a task queue with the given type, |spec| and args.
  // Must be called on the main thread.
  // TODO(scheduler-dev): SequenceManager should not create TaskQueues.
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

}  // namespace sequence_manager
}  // namespace base

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_SEQUENCE_MANAGER_H_
