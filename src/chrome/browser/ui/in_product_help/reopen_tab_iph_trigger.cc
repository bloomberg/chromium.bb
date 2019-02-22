// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/in_product_help/reopen_tab_iph_trigger.h"

#include <utility>

#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"

namespace in_product_help {

// static
const base::TimeDelta ReopenTabIPHTrigger::kTabMinimumActiveDuration =
    base::TimeDelta::FromSeconds(10);
// static
const base::TimeDelta ReopenTabIPHTrigger::kNewTabOpenedTimeout =
    base::TimeDelta::FromSeconds(10);
// static
const base::TimeDelta ReopenTabIPHTrigger::kOmniboxFocusedTimeout =
    base::TimeDelta::FromSeconds(10);

ReopenTabIPHTrigger::ReopenTabIPHTrigger(feature_engagement::Tracker* tracker,
                                         const base::TickClock* clock)
    : tracker_(tracker), clock_(clock), trigger_state_(NO_ACTIONS_SEEN) {
  DCHECK(tracker);
  DCHECK(clock);

  // Timeouts must be non-zero.
  DCHECK(!kNewTabOpenedTimeout.is_zero());
  DCHECK(!kOmniboxFocusedTimeout.is_zero());
}

ReopenTabIPHTrigger::~ReopenTabIPHTrigger() = default;

void ReopenTabIPHTrigger::SetShowHelpCallback(ShowHelpCallback callback) {
  DCHECK(callback);
  cb_ = std::move(callback);
}

void ReopenTabIPHTrigger::ActiveTabClosed(base::TimeTicks activation_time) {
  // Reset all flags at this point. We should only trigger IPH if the events
  // happen in the prescribed order.
  ResetTriggerState();

  DCHECK(activation_time <= clock_->NowTicks());
  // We only go to the next state if the closing tab was active for long enough.
  if (clock_->NowTicks() - activation_time >= kTabMinimumActiveDuration) {
    trigger_state_ = ACTIVE_TAB_CLOSED;
    time_of_last_step_ = clock_->NowTicks();
  }
}

void ReopenTabIPHTrigger::NewTabOpened() {
  if (trigger_state_ != ACTIVE_TAB_CLOSED)
    return;

  const base::TimeDelta elapsed_time = clock_->NowTicks() - time_of_last_step_;

  if (elapsed_time < kNewTabOpenedTimeout) {
    trigger_state_ = NEW_TAB_OPENED;
    time_of_last_step_ = clock_->NowTicks();
  } else {
    ResetTriggerState();
  }
}

void ReopenTabIPHTrigger::OmniboxFocused() {
  if (trigger_state_ != NEW_TAB_OPENED)
    return;

  const base::TimeDelta elapsed_time = clock_->NowTicks() - time_of_last_step_;

  if (elapsed_time < kOmniboxFocusedTimeout) {
    tracker_->NotifyEvent(feature_engagement::events::kReopenTabConditionsMet);
    if (tracker_->ShouldTriggerHelpUI(
            feature_engagement::kIPHReopenTabFeature)) {
      DCHECK(cb_);
      cb_.Run();
    }
  }
}

void ReopenTabIPHTrigger::HelpDismissed() {
  tracker_->Dismissed(feature_engagement::kIPHReopenTabFeature);
  ResetTriggerState();
}

void ReopenTabIPHTrigger::ResetTriggerState() {
  time_of_last_step_ = base::TimeTicks();
  trigger_state_ = NO_ACTIONS_SEEN;
}

}  // namespace in_product_help
