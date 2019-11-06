// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/performance_manager_clock.h"

#include "base/time/tick_clock.h"

namespace performance_manager {

namespace {

const base::TickClock*& g_tick_clock_for_testing() {
  static const base::TickClock* tick_clock_for_testing = nullptr;
  return tick_clock_for_testing;
}

}  // namespace

base::TimeTicks PerformanceManagerClock::NowTicks() {
  return g_tick_clock_for_testing() ? g_tick_clock_for_testing()->NowTicks()
                                    : base::TimeTicks::Now();
}

const base::TickClock* PerformanceManagerClock::GetClockForTesting() {
  return g_tick_clock_for_testing();
}

void PerformanceManagerClock::ResetClockForTesting() {
  g_tick_clock_for_testing() = nullptr;
}

void PerformanceManagerClock::SetClockForTesting(
    const base::TickClock* tick_clock) {
  DCHECK(!g_tick_clock_for_testing());
  g_tick_clock_for_testing() = tick_clock;
}

}  // namespace performance_manager
