// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_notifier_impl.h"

#include "base/time/time.h"
#include "base/timer/timer.h"

namespace {

// TODO(crbug.com/1076270): Finalize a value for this, and possibly use
// different values for different UI surfaces.
constexpr base::TimeDelta kImpressionTimer = base::TimeDelta::FromSeconds(1);

}  // namespace

AppListNotifierImpl::AppListNotifierImpl() = default;
AppListNotifierImpl::~AppListNotifierImpl() = default;

void AppListNotifierImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppListNotifierImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppListNotifierImpl::NotifyLaunched(Location location,
                                         const std::string& result) {
  launched_result_ = result;

  // Only two UI views appear at once: the app tiles and results list. If a
  // launch occurs in one, mark the other as 'ignored' rather than abandoned.
  if (location == Location::kList) {
    DoStateTransition(Location::kTile, State::kIgnored);
  } else if (location == Location::kTile) {
    DoStateTransition(Location::kList, State::kIgnored);
  }

  DoStateTransition(location, State::kLaunched);
}

void AppListNotifierImpl::NotifyResultsUpdated(
    Location location,
    const std::vector<std::string>& results) {
  results_[location] = results;
}

void AppListNotifierImpl::NotifySearchQueryChanged(
    const base::string16& query) {
  query_ = query;
  DoStateTransition(Location::kList, State::kShown);
  DoStateTransition(Location::kTile, State::kShown);
}

void AppListNotifierImpl::NotifyUIStateChanged(ash::AppListViewState view) {
  view_ = view;

  if (view == ash::AppListViewState::kHalf ||
      view == ash::AppListViewState::kFullscreenSearch) {
    DoStateTransition(Location::kList, State::kShown);
    DoStateTransition(Location::kTile, State::kShown);
  } else {
    DoStateTransition(Location::kList, State::kNone);
    DoStateTransition(Location::kTile, State::kNone);
  }

  if (view == ash::AppListViewState::kPeeking ||
      view == ash::AppListViewState::kFullscreenAllApps) {
    DoStateTransition(Location::kChip, State::kShown);
  } else {
    DoStateTransition(Location::kChip, State::kNone);
  }
}

void AppListNotifierImpl::RestartTimer(Location location) {
  if (timers_.find(location) == timers_.end()) {
    timers_[location] = std::make_unique<base::OneShotTimer>();
  }

  auto& timer = timers_[location];
  if (timer->IsRunning()) {
    timer->Stop();
  }
  // base::Unretained is safe here because the timer is a member of |this|, and
  // OneShotTimer cancels its timer on destruction.
  timer->Start(FROM_HERE, kImpressionTimer,
               base::BindOnce(&AppListNotifierImpl::OnTimerFinished,
                              base::Unretained(this), location));
}

void AppListNotifierImpl::StopTimer(Location location) {
  const auto it = timers_.find(location);
  if (it != timers_.end() && it->second->IsRunning()) {
    it->second->Stop();
  }
}

void AppListNotifierImpl::OnTimerFinished(Location location) {
  DoStateTransition(location, State::kSeen);
}

void AppListNotifierImpl::DoStateTransition(Location location,
                                            State new_state) {
  const State old_state = states_[location];

  // Update most recent state. We special-case kLaunched and kIgnored, which are
  // temporary states reflecting a launch either in |location| or another view.
  // They immediately transition to kNone because the launcher closes after a
  // launch.
  if (new_state == State::kLaunched || new_state == State::kIgnored) {
    states_[location] = State::kNone;
  } else {
    states_[location] = new_state;
  }

  // These overlapping cases are equivalent to the explicit cases in the header
  // comment.

  // Restart timer on * -> kShown
  if (new_state == State::kShown) {
    RestartTimer(location);
  }

  // Stop timer on kShown -> {kNone, kLaunch}.
  if (old_state == State::kShown &&
      (new_state == State::kNone || new_state == State::kLaunched)) {
    StopTimer(location);
  }

  // Notify of impression on kShown -> {kSeen, kIgnored, kLaunched}.
  if (old_state == State::kShown &&
      (new_state == State::kSeen || new_state == State::kLaunched ||
       new_state == State::kIgnored)) {
    for (auto& observer : observers_) {
      observer.OnImpression(location, results_[location], query_);
    }
  }

  // Notify of launch on * -> kLaunched.
  if (new_state == State::kLaunched) {
    for (auto& observer : observers_) {
      observer.OnLaunch(location, launched_result_, results_[location], query_);
    }
  }

  // Notify of ignore on * -> kIgnored.
  if (new_state == State::kIgnored) {
    for (auto& observer : observers_) {
      observer.OnIgnore(location, results_[location], query_);
    }
  }

  // Notify of abandon on kSeen -> {kNone, kShown}.
  if (old_state == State::kSeen &&
      (new_state == State::kNone || new_state == State::kShown)) {
    for (auto& observer : observers_) {
      observer.OnAbandon(location, results_[location], query_);
    }
  }
}
