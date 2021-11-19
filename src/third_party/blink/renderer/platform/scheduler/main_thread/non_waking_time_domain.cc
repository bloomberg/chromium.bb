// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/non_waking_time_domain.h"

namespace blink {
namespace scheduler {

NonWakingTimeDomain::NonWakingTimeDomain(const base::TickClock* tick_clock)
    : tick_clock_(tick_clock) {}

NonWakingTimeDomain::~NonWakingTimeDomain() = default;

base::TimeTicks NonWakingTimeDomain::NowTicks() const {
  return tick_clock_->NowTicks();
}

base::TimeTicks NonWakingTimeDomain::GetNextDelayedTaskTime(
    base::sequence_manager::LazyNow* lazy_now) const {
  // NonWakingTimeDomain should never generate wakeups on its own.
  return base::TimeTicks::Max();
}

bool NonWakingTimeDomain::MaybeFastForwardToNextTask(
    bool quit_when_idle_requested) {
  return false;
}

const char* NonWakingTimeDomain::GetName() const {
  return "non_waking_time_domain";
}

void NonWakingTimeDomain::SetNextDelayedDoWork(
    base::sequence_manager::LazyNow* lazy_now,
    base::TimeTicks run_time) {
  // Do not request a wake-up, unlike a regular TimeDomain.
}

}  // namespace scheduler
}  // namespace blink
