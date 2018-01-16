// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_AUTO_ADVANCING_VIRTUAL_TIME_DOMAIN_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_AUTO_ADVANCING_VIRTUAL_TIME_DOMAIN_H_

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "platform/scheduler/base/virtual_time_domain.h"

namespace blink {
namespace scheduler {
class SchedulerHelper;

// A time domain that runs tasks sequentially in time order but doesn't sleep
// between delayed tasks.
//
// KEY: A-E are delayed tasks
// |    A   B C  D           E  (Execution with RealTimeDomain)
// |-----------------------------> time
//
// |ABCDE                       (Execution with AutoAdvancingVirtualTimeDomain)
// |-----------------------------> time
class PLATFORM_EXPORT AutoAdvancingVirtualTimeDomain
    : public VirtualTimeDomain,
      public base::MessageLoop::TaskObserver {
 public:
  AutoAdvancingVirtualTimeDomain(base::TimeTicks initial_time,
                                 SchedulerHelper* helper);
  ~AutoAdvancingVirtualTimeDomain() override;

  // TimeDomain implementation:
  base::Optional<base::TimeDelta> DelayTillNextTask(LazyNow* lazy_now) override;
  void RequestWakeUpAt(base::TimeTicks now, base::TimeTicks run_time) override;
  void CancelWakeUpAt(base::TimeTicks run_time) override;
  const char* GetName() const override;

  class PLATFORM_EXPORT Observer {
   public:
    Observer();
    virtual ~Observer();

    // Notification received when the virtual time advances.
    virtual void OnVirtualTimeAdvanced() = 0;
  };

  // Note its assumed that |observer| will either remove itself or last at least
  // as long as this AutoAdvancingVirtualTimeDomain.
  void SetObserver(Observer* observer);

  // Controls whether or not virtual time is allowed to advance, when the
  // TaskQueueManager runs out of immediate work to do.
  void SetCanAdvanceVirtualTime(bool can_advance_virtual_time);

  // If non-null, virtual time may not advance past |virtual_time_fence|.
  void SetVirtualTimeFence(base::TimeTicks virtual_time_fence);

  // The maximum number amount of delayed task starvation we will allow.
  // NB a value of 0 allows infinite starvation. A reasonable value for this in
  // practice is around 1000 tasks, which should only affect rendering of the
  // heaviest pages.
  void SetMaxVirtualTimeTaskStarvationCount(int max_task_starvation_count);

  // Updates to min(NextDelayedTaskTime, |new_virtual_time|) if thats ahead of
  // the current virtual time.  Returns true if time was advanced.
  bool MaybeAdvanceVirtualTime(base::TimeTicks new_virtual_time);

  // base::PendingTask implementation:
  void WillProcessTask(const base::PendingTask& pending_task) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

  int task_starvation_count() const { return task_starvation_count_; }

 private:
  // The number of tasks that have been run since the last time VirtualTime
  // advanced. Used to detect excessive starvation of delayed tasks.
  int task_starvation_count_;

  // The maximum number amount of delayed task starvation we will allow.
  // NB a value of 0 allows infinite starvation.
  int max_task_starvation_count_;

  bool can_advance_virtual_time_;
  Observer* observer_;       // NOT OWNED
  SchedulerHelper* helper_;  // NOT OWNED

  // VirtualTime is usually doled out in 100ms intervals using fences and this
  // variable let us honor a request to MaybeAdvanceVirtualTime that straddles
  // one of these boundaries.
  base::TimeTicks requested_next_virtual_time_;

  // Upper limit on how far virtual time is allowed to advance.
  base::TimeTicks virtual_time_fence_;

  DISALLOW_COPY_AND_ASSIGN(AutoAdvancingVirtualTimeDomain);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_AUTO_ADVANCING_VIRTUAL_TIME_DOMAIN_H_
