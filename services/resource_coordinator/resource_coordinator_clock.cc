// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/resource_coordinator_clock.h"

#include "base/time/tick_clock.h"

namespace resource_coordinator {

namespace {

std::unique_ptr<base::TickClock>& g_tick_clock_for_testing() {
  static std::unique_ptr<base::TickClock> tick_clock_for_testing = nullptr;
  return tick_clock_for_testing;
}

}  // namespace

base::TimeTicks ResourceCoordinatorClock::NowTicks() {
  return g_tick_clock_for_testing() ? g_tick_clock_for_testing()->NowTicks()
                                    : base::TimeTicks::Now();
}

base::TickClock* ResourceCoordinatorClock::GetClockForTesting() {
  return g_tick_clock_for_testing().get();
}

void ResourceCoordinatorClock::ResetClockForTesting() {
  g_tick_clock_for_testing().reset();
}

void ResourceCoordinatorClock::SetClockForTesting(
    std::unique_ptr<base::TickClock> tick_clock) {
  DCHECK(!g_tick_clock_for_testing());
  g_tick_clock_for_testing() = std::move(tick_clock);
}

}  // namespace resource_coordinator
