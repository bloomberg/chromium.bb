// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/yielding_display_lock_budget.h"

#include "third_party/blink/renderer/platform/wtf/time.h"

#include <algorithm>

namespace blink {

YieldingDisplayLockBudget::YieldingDisplayLockBudget(
    DisplayLockContext* context)
    : DisplayLockBudget(context) {}

bool YieldingDisplayLockBudget::ShouldPerformPhase(Phase phase) const {
  // We should perform any phase earlier than the one we already completed.
  if (last_completed_phase_ && phase <= *last_completed_phase_)
    return true;

  // Otherwise, we can still do work while we're not past the deadline.
  return CurrentTimeTicks() < deadline_;
}

void YieldingDisplayLockBudget::DidPerformPhase(Phase phase) {
  if (!last_completed_phase_ || phase > *last_completed_phase_)
    last_completed_phase_ = phase;
}

void YieldingDisplayLockBudget::WillStartLifecycleUpdate() {
  ++lifecycle_count_;
  deadline_ =
      CurrentTimeTicks() + TimeDelta::FromMillisecondsD(GetCurrentBudgetMs());

  // Figure out the next phase we would run. If we had completed a phase before,
  // then we should try to complete the next one, otherwise we'll start with the
  // first phase.
  Phase next_phase =
      last_completed_phase_
          ? static_cast<Phase>(
                std::min(static_cast<unsigned>(*last_completed_phase_) + 1,
                         static_cast<unsigned>(Phase::kLast)))
          : Phase::kFirst;

  // Mark the next phase we're scheduled to run.
  for (auto phase = static_cast<unsigned>(next_phase);
       phase <= static_cast<unsigned>(Phase::kLast); ++phase) {
    if (MarkAncestorsDirtyForPhaseIfNeeded(static_cast<Phase>(phase)))
      break;
  }
}

bool YieldingDisplayLockBudget::NeedsLifecycleUpdates() const {
  if (last_completed_phase_ && *last_completed_phase_ == Phase::kLast)
    return false;

  auto next_phase = last_completed_phase_
                        ? static_cast<Phase>(
                              static_cast<unsigned>(*last_completed_phase_) + 1)
                        : Phase::kFirst;
  // Check if any future phase needs updates.
  for (auto phase = static_cast<unsigned>(next_phase);
       phase <= static_cast<unsigned>(Phase::kLast); ++phase) {
    if (IsElementDirtyForPhase(static_cast<Phase>(phase)))
      return true;
  }
  return false;
}

double YieldingDisplayLockBudget::GetCurrentBudgetMs() const {
  if (TimeTicks::IsHighResolution()) {
    if (lifecycle_count_ < 3)
      return 4.;
    if (lifecycle_count_ < 10)
      return 8.;
    if (lifecycle_count_ < 60)
      return 16.;
  } else {
    // Without a high resolution clock, the resolution can be as bad as 15ms, so
    // increase the budgets accordingly to ensure we don't abort before doing
    // any phases.
    if (lifecycle_count_ < 60)
      return 16.;
  }
  return 1e9;
}

}  // namespace blink
