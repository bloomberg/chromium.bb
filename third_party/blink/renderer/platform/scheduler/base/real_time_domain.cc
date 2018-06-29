// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/base/real_time_domain.h"

#include "base/bind.h"
#include "third_party/blink/renderer/platform/scheduler/base/task_queue_impl_forward.h"
#include "third_party/blink/renderer/platform/scheduler/base/task_queue_manager_impl.h"

namespace base {
namespace sequence_manager {
namespace internal {

RealTimeDomain::RealTimeDomain() {}

RealTimeDomain::~RealTimeDomain() = default;

LazyNow RealTimeDomain::CreateLazyNow() const {
  return LazyNow(sequence_manager()->GetTickClock());
}

TimeTicks RealTimeDomain::Now() const {
  return sequence_manager()->NowTicks();
}

Optional<TimeDelta> RealTimeDomain::DelayTillNextTask(LazyNow* lazy_now) {
  Optional<TimeTicks> next_run_time = NextScheduledRunTime();
  if (!next_run_time)
    return nullopt;

  TimeTicks now = lazy_now->Now();
  if (now >= next_run_time) {
    // Overdue work needs to be run immediately.
    return TimeDelta();
  }

  TimeDelta delay = *next_run_time - now;
  TRACE_EVENT1("sequence_manager", "RealTimeDomain::DelayTillNextTask",
               "delay_ms", delay.InMillisecondsF());
  return delay;
}

const char* RealTimeDomain::GetName() const {
  return "RealTimeDomain";
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
