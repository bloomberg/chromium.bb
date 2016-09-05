// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_THROTTLING_HELPER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_THROTTLING_HELPER_H_

#include <set>

#include "base/macros.h"
#include "platform/scheduler/base/cancelable_closure_holder.h"
#include "platform/scheduler/base/time_domain.h"
#include "public/platform/WebViewScheduler.h"

namespace blink {
namespace scheduler {

class RendererSchedulerImpl;
class ThrottledTimeDomain;
class WebFrameSchedulerImpl;

// The job of the ThrottlingHelper is to run tasks posted on throttled queues at
// most once per second. This is done by disabling throttled queues and running
// a special "heart beat" function |PumpThrottledTasks| which when run
// temporarily enables throttled queues and inserts a fence to ensure tasks
// posted from a throttled task run next time the queue is pumped.
//
// Of course the ThrottlingHelper isn't the only sub-system that wants to enable
// or disable queues. E.g. RendererSchedulerImpl also does this for policy
// reasons.  To prevent the systems from fighting, clients of ThrottlingHelper
// must use SetQueueEnabled rather than calling the function directly on the
// queue.
//
// There may be more than one system that wishes to throttle a queue (e.g.
// renderer suspension vs tab level suspension) so the ThrottlingHelper keeps a
// count of the number of systems that wish a queue to be throttled.
// See IncreaseThrottleRefCount & DecreaseThrottleRefCount.
class BLINK_PLATFORM_EXPORT ThrottlingHelper : public TimeDomain::Observer {
 public:
  ThrottlingHelper(RendererSchedulerImpl* renderer_scheduler,
                   const char* tracing_category);

  ~ThrottlingHelper() override;

  // TimeDomain::Observer implementation:
  void OnTimeDomainHasImmediateWork() override;
  void OnTimeDomainHasDelayedWork() override;

  // The purpose of this method is to make sure throttling doesn't conflict with
  // enabling/disabling the queue for policy reasons.
  // If |task_queue| is throttled then the ThrottlingHelper remembers the
  // |enabled| setting.  In addition if |enabled| is false then the queue is
  // immediatly disabled.  Otherwise if |task_queue| not throttled then
  // TaskQueue::SetEnabled(enabled) is called.
  void SetQueueEnabled(TaskQueue* task_queue, bool enabled);

  // Increments the throttled refcount and causes |task_queue| to be throttled
  // if its not already throttled.
  void IncreaseThrottleRefCount(TaskQueue* task_queue);

  // If the refcouint is non-zero it's decremented.  If the throttled refcount
  // becomes zero then |task_queue| is unthrottled.  If the refcount was already
  // zero this function does nothing.
  void DecreaseThrottleRefCount(TaskQueue* task_queue);

  // Removes |task_queue| from |throttled_queues_|.
  void UnregisterTaskQueue(TaskQueue* task_queue);

  // Returns true if the |task_queue| is throttled.
  bool IsThrottled(TaskQueue* task_queue) const;

  // Tells the ThrottlingHelper we're using virtual time, which disables all
  // throttling.
  void EnableVirtualTime();

  const ThrottledTimeDomain* time_domain() const { return time_domain_.get(); }

  static base::TimeTicks ThrottledRunTime(base::TimeTicks unthrottled_runtime);

  const scoped_refptr<TaskQueue>& task_runner() const { return task_runner_; }

 private:
  struct Metadata {
    Metadata() : throttling_ref_count(0), enabled(false) {}

    Metadata(size_t ref_count, bool is_enabled)
        : throttling_ref_count(ref_count), enabled(is_enabled) {}

    size_t throttling_ref_count;
    bool enabled;
  };
  using TaskQueueMap = std::map<TaskQueue*, Metadata>;

  void PumpThrottledTasks();

  // Note |unthrottled_runtime| might be in the past. When this happens we
  // compute the delay to the next runtime based on now rather than
  // unthrottled_runtime.
  void MaybeSchedulePumpThrottledTasksLocked(
      const tracked_objects::Location& from_here,
      base::TimeTicks now,
      base::TimeTicks unthrottled_runtime);

  TaskQueueMap throttled_queues_;
  base::Closure forward_immediate_work_closure_;
  scoped_refptr<TaskQueue> task_runner_;
  RendererSchedulerImpl* renderer_scheduler_;  // NOT OWNED
  base::TickClock* tick_clock_;                // NOT OWNED
  const char* tracing_category_;               // NOT OWNED
  std::unique_ptr<ThrottledTimeDomain> time_domain_;

  CancelableClosureHolder pump_throttled_tasks_closure_;
  base::TimeTicks pending_pump_throttled_tasks_runtime_;
  bool virtual_time_;

  base::WeakPtrFactory<ThrottlingHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThrottlingHelper);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_THROTTLING_HELPER_H_
