// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/throttled_time_domain.h"

namespace blink {
namespace scheduler {

ThrottledTimeDomain::ThrottledTimeDomain(TimeDomain::Observer* observer,
                                         const char* tracing_category)
    : RealTimeDomain(observer, tracing_category) {}

ThrottledTimeDomain::~ThrottledTimeDomain() {}

const char* ThrottledTimeDomain::GetName() const {
  return "ThrottledTimeDomain";
}

void ThrottledTimeDomain::RequestWakeupAt(base::TimeTicks now,
                                          base::TimeTicks run_time) {
  // We assume the owner (i.e. TaskQueueThrottler) will manage wakeups on our
  // behalf.
}

void ThrottledTimeDomain::CancelWakeupAt(base::TimeTicks run_time) {
  // We ignore this because RequestWakeupAt is a NOP.
}

base::Optional<base::TimeDelta> ThrottledTimeDomain::DelayTillNextTask(
    LazyNow* lazy_now) {
  base::TimeTicks next_run_time;
  if (!NextScheduledRunTime(&next_run_time))
    return base::nullopt;

  base::TimeTicks now = lazy_now->Now();
  if (now >= next_run_time)
    return base::TimeDelta();  // Makes DoWork post an immediate continuation.

  // We assume the owner (i.e. TaskQueueThrottler) will manage wakeups on our
  // behalf.
  return base::nullopt;
}

}  // namespace scheduler
}  // namespace blink
