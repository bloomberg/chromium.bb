// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/test/mock_time_domain.h"

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace sequence_manager {

MockTimeDomain::MockTimeDomain(TimeTicks initial_now_ticks)
    : now_ticks_(initial_now_ticks) {}

MockTimeDomain::~MockTimeDomain() = default;

TimeTicks MockTimeDomain::NowTicks() const {
  return now_ticks_;
}

void MockTimeDomain::SetNowTicks(TimeTicks now_ticks) {
  now_ticks_ = now_ticks;
}

TimeTicks MockTimeDomain::GetNextDelayedTaskTime(
    WakeUp next_wake_up,
    sequence_manager::LazyNow* lazy_now) const {
  return TimeTicks::Max();
}

bool MockTimeDomain::MaybeFastForwardToWakeUp(
    absl::optional<WakeUp> next_wake_up,
    bool quit_when_idle_requested) {
  return false;
}

const char* MockTimeDomain::GetName() const {
  return "MockTimeDomain";
}

}  // namespace sequence_manager
}  // namespace base
