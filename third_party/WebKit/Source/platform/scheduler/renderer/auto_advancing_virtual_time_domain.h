// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_AUTO_ADVANCING_VIRTUAL_TIME_DOMAIN_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_AUTO_ADVANCING_VIRTUAL_TIME_DOMAIN_H_

#include "base/macros.h"
#include "platform/scheduler/base/virtual_time_domain.h"

namespace blink {
namespace scheduler {

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
    : public VirtualTimeDomain {
 public:
  explicit AutoAdvancingVirtualTimeDomain(base::TimeTicks initial_time);
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

 private:
  bool can_advance_virtual_time_;
  Observer* observer_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(AutoAdvancingVirtualTimeDomain);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_AUTO_ADVANCING_VIRTUAL_TIME_DOMAIN_H_
