// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/auto_advancing_virtual_time_domain.h"

namespace blink {
namespace scheduler {

AutoAdvancingVirtualTimeDomain::AutoAdvancingVirtualTimeDomain(
    base::TimeTicks initial_time)
    : VirtualTimeDomain(initial_time),
      can_advance_virtual_time_(true),
      observer_(nullptr) {}

AutoAdvancingVirtualTimeDomain::~AutoAdvancingVirtualTimeDomain() {}

base::Optional<base::TimeDelta>
AutoAdvancingVirtualTimeDomain::DelayTillNextTask(LazyNow* lazy_now) {
  base::TimeTicks run_time;
  if (!can_advance_virtual_time_ || !NextScheduledRunTime(&run_time))
    return base::nullopt;

  AdvanceTo(run_time);
  if (observer_)
    observer_->OnVirtualTimeAdvanced();
  return base::TimeDelta();  // Makes DoWork post an immediate continuation.
}

void AutoAdvancingVirtualTimeDomain::RequestWakeUpAt(base::TimeTicks now,
                                                     base::TimeTicks run_time) {
  // Avoid posting pointless DoWorks.  I.e. if the time domain has more then one
  // scheduled wake up then we don't need to do anything.
  if (can_advance_virtual_time_ && NumberOfScheduledWakeUps() == 1u)
    RequestDoWork();
}

void AutoAdvancingVirtualTimeDomain::CancelWakeUpAt(base::TimeTicks run_time) {
  // We ignore this because RequestWakeUpAt doesn't post a delayed task.
}

void AutoAdvancingVirtualTimeDomain::SetObserver(Observer* observer) {
  observer_ = observer;
}

void AutoAdvancingVirtualTimeDomain::SetCanAdvanceVirtualTime(
    bool can_advance_virtual_time) {
  can_advance_virtual_time_ = can_advance_virtual_time;
  if (can_advance_virtual_time_)
    RequestDoWork();
}

const char* AutoAdvancingVirtualTimeDomain::GetName() const {
  return "AutoAdvancingVirtualTimeDomain";
}

AutoAdvancingVirtualTimeDomain::Observer::Observer() {}

AutoAdvancingVirtualTimeDomain::Observer::~Observer() {}

}  // namespace scheduler
}  // namespace blink
