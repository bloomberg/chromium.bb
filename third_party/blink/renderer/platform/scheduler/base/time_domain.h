// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_TIME_DOMAIN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_TIME_DOMAIN_H_

#include <map>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/task/sequence_manager/intrusive_heap.h"
#include "base/task/sequence_manager/lazy_now.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/scheduler/base/task_queue_impl_forward.h"

namespace base {
namespace sequence_manager {

class SequenceManager;
class TaskQueueManagerImpl;

namespace internal {
class TaskQueueImpl;
}  // internal

// TimeDomain wakes up TaskQueues when their delayed tasks are due to run.
// This class allows overrides to enable clock overriding on some TaskQueues
// (e.g. auto-advancing virtual time, throttled clock, etc).
//
// TaskQueue maintains its own next wake-up time and communicates it
// to the TimeDomain, which aggregates wake-ups across registered TaskQueues
// into a global wake-up, which ultimately gets passed to the ThreadCOntroller.
class PLATFORM_EXPORT TimeDomain {
 public:
  virtual ~TimeDomain();

  // Returns LazyNow in TimeDomain's time.
  // Can be called from any thread.
  // TODO(alexclarke): Make this main thread only.
  virtual LazyNow CreateLazyNow() const = 0;

  // Evaluates TimeDomain's time.
  // Can be called from any thread.
  // TODO(alexclarke): Make this main thread only.
  virtual TimeTicks Now() const = 0;

  // Computes the delay until the time when TimeDomain needs to wake up
  // some TaskQueue. Specific time domains (e.g. virtual or throttled) may
  // return TimeDelata() if TaskQueues have any delayed tasks they deem
  // eligible to run. It's also allowed to advance time domains's internal
  // clock when this method is called.
  // Can be called from main thread only.
  // NOTE: |lazy_now| and the return value are in the SequenceManager's time.
  virtual Optional<TimeDelta> DelayTillNextTask(LazyNow* lazy_now) = 0;

  void AsValueInto(trace_event::TracedValue* state) const;

 protected:
  TimeDomain();

  SequenceManager* sequence_manager() const;

  // Returns the earliest scheduled wake up in the TimeDomain's time.
  Optional<TimeTicks> NextScheduledRunTime() const;

  size_t NumberOfScheduledWakeUps() const {
    return delayed_wake_up_queue_.size();
  }

  // Tell SequenceManager to (un)schedule delayed or immediate work.
  // Time is provided in SequenceManager's time.
  // May be overriden to control wake ups manually.
  virtual void RequestWakeUpAt(TimeTicks now, TimeTicks run_time);
  virtual void CancelWakeUpAt(TimeTicks run_time);
  virtual void RequestDoWork();

  // For implementation-specific tracing.
  virtual void AsValueIntoInternal(trace_event::TracedValue* state) const;
  virtual const char* GetName() const = 0;

 private:
  friend class internal::TaskQueueImpl;
  friend class TaskQueueManagerImpl;
  friend class MockTimeDomain;

  // Called when the TimeDomain is registered.
  // TODO(kraynov): Pass SequenceManager in the constructor.
  void OnRegisterWithSequenceManager(TaskQueueManagerImpl* sequence_manager);

  // Schedule TaskQueue to wake up at certain time, repeating calls with
  // the same |queue| invalidate previous requests.
  // Nullopt |wake_up| cancels a previously set wake up for |queue|.
  // NOTE: |lazy_now| is provided in TimeDomain's time.
  void SetNextWakeUpForQueue(
      internal::TaskQueueImpl* queue,
      Optional<internal::TaskQueueImpl::DelayedWakeUp> wake_up,
      LazyNow* lazy_now);

  // Remove the TaskQueue from any internal data sctructures.
  void UnregisterQueue(internal::TaskQueueImpl* queue);

  // Wake up each TaskQueue where the delay has elapsed.
  void WakeUpReadyDelayedQueues(LazyNow* lazy_now);

  struct ScheduledDelayedWakeUp {
    internal::TaskQueueImpl::DelayedWakeUp wake_up;
    internal::TaskQueueImpl* queue;

    bool operator<=(const ScheduledDelayedWakeUp& other) const {
      return wake_up <= other.wake_up;
    }

    void SetHeapHandle(internal::HeapHandle handle) {
      DCHECK(handle.IsValid());
      queue->set_heap_handle(handle);
    }

    void ClearHeapHandle() {
      DCHECK(queue->heap_handle().IsValid());
      queue->set_heap_handle(internal::HeapHandle());
    }
  };

  TaskQueueManagerImpl* sequence_manager_;  // Not owned.
  internal::IntrusiveHeap<ScheduledDelayedWakeUp> delayed_wake_up_queue_;

  ThreadChecker main_thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(TimeDomain);
};

}  // namespace sequence_manager
}  // namespace base

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_TIME_DOMAIN_H_
