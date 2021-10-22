// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/launcher_nudge_controller.h"

#include <memory>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/home_button_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace ash {
namespace {

// Keys for user preferences.
constexpr char kShownCount[] = "shown_count";
constexpr char kLastShownTime[] = "last_shown_time";
constexpr char kFirstLoginTime[] = "first_login_time";
constexpr char kWasLauncherShown[] = "was_launcher_shown";

// Constants for launcher nudge controller.
constexpr base::TimeDelta kFirstTimeShowNudgeInterval = base::Days(1);
constexpr base::TimeDelta kShowNudgeInterval = base::Days(1);

// Returns the last active user pref service.
PrefService* GetPrefs() {
  return Shell::Get()->session_controller()->GetLastActiveUserPrefService();
}

// Gets the timestamp when the nudge was last shown.
base::Time GetLastShownTime(PrefService* prefs) {
  const base::DictionaryValue* dictionary =
      prefs->GetDictionary(prefs::kShelfLauncherNudge);
  if (!dictionary)
    return base::Time();
  absl::optional<base::Time> last_shown_time =
      base::ValueToTime(dictionary->FindPath(kLastShownTime));
  return last_shown_time.value_or(base::Time());
}

// Gets the timestamp when the user first logged in. The value will not be
// set if the user has logged in before the launcher nudge feature was
// enabled.
base::Time GetFirstLoginTime(PrefService* prefs) {
  const base::DictionaryValue* dictionary =
      prefs->GetDictionary(prefs::kShelfLauncherNudge);
  if (!dictionary)
    return base::Time();
  absl::optional<base::Time> first_login_time =
      base::ValueToTime(dictionary->FindPath(kFirstLoginTime));
  return first_login_time.value_or(base::Time());
}

// Returns true if the launcher has been shown before.
bool WasLauncherShownPreviously(PrefService* prefs) {
  const base::DictionaryValue* dictionary =
      prefs->GetDictionary(prefs::kShelfLauncherNudge);
  if (!dictionary)
    return false;
  return dictionary->FindBoolPath(kWasLauncherShown).value_or(false);
}

}  // namespace

LauncherNudgeController::LauncherNudgeController()
    : show_nudge_timer_(std::make_unique<base::WallClockTimer>()) {
  Shell::Get()->app_list_controller()->AddObserver(this);
  tablet_mode_observation_.Observe(Shell::Get()->tablet_mode_controller());
}

LauncherNudgeController::~LauncherNudgeController() {
  if (Shell::Get()->app_list_controller())
    Shell::Get()->app_list_controller()->RemoveObserver(this);
  tablet_mode_observation_.Reset();
}

// static
void LauncherNudgeController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kShelfLauncherNudge);
}

// static
HomeButton* LauncherNudgeController::GetHomeButtonForDisplay(
    int64_t display_id) {
  return Shell::Get()
      ->GetRootWindowControllerWithDisplayId(display_id)
      ->shelf()
      ->navigation_widget()
      ->GetHomeButton();
}

// static
int LauncherNudgeController::GetShownCount(PrefService* prefs) {
  const base::DictionaryValue* dictionary =
      prefs->GetDictionary(prefs::kShelfLauncherNudge);
  if (!dictionary)
    return 0;
  return dictionary->FindIntPath(kShownCount).value_or(0);
}

base::TimeDelta LauncherNudgeController::GetNudgeInterval(
    bool is_first_time) const {
  return is_first_time ? kFirstTimeShowNudgeInterval : kShowNudgeInterval;
}

void LauncherNudgeController::SetClockForTesting(
    const base::Clock* clock,
    const base::TickClock* timer_clock) {
  DCHECK(!show_nudge_timer_->IsRunning());
  show_nudge_timer_ =
      std::make_unique<base::WallClockTimer>(clock, timer_clock);
  clock_for_test_ = clock;
}

bool LauncherNudgeController::IsRecheckTimerRunningForTesting() {
  return show_nudge_timer_->IsRunning();
}

bool LauncherNudgeController::ShouldShowNudge(base::Time& recheck_time) const {
  PrefService* prefs = GetPrefs();
  if (!prefs)
    return false;

  // Only show the launcher nudge in clamshell mode.
  if (Shell::Get()->IsInTabletMode())
    return false;

  // If the shown count meets the limit or the launcher has been opened before,
  // don't show the nudge.
  if (GetShownCount(prefs) >= kMaxShownCount ||
      WasLauncherShownPreviously(prefs)) {
    return false;
  }

  base::Time last_shown_time;
  base::TimeDelta interval;

  if (GetShownCount(prefs) == 0) {
    // Set the `last_shown_time` to the timestamp when the user first logs in
    // if the nudge hasn't been shown yet and the actual `last_shown_time` is
    // null. Calculate the expect nudge show time using that timestamp and its
    // corresponding interval.
    last_shown_time = GetFirstLoginTime(prefs);
    interval = GetNudgeInterval(/*is_first_time=*/true);
  } else {
    last_shown_time = GetLastShownTime(prefs);
    interval = GetNudgeInterval(/*is_first_time=*/false);
  }
  DCHECK(!last_shown_time.is_null());

  if (GetNow() < last_shown_time + interval) {
    // Set the `recheck_time` to the expected time to show nudge.
    recheck_time = last_shown_time + interval;
    return false;
  }

  return true;
}

void LauncherNudgeController::HandleNudgeShown() {
  PrefService* prefs = GetPrefs();
  if (!prefs)
    return;

  const int shown_count = GetShownCount(prefs);
  DictionaryPrefUpdate update(prefs, prefs::kShelfLauncherNudge);
  update->SetIntPath(kShownCount, shown_count + 1);
  update->SetPath(kLastShownTime, base::TimeToValue(GetNow()));
}

void LauncherNudgeController::MaybeShowNudge() {
  if (!features::IsShelfLauncherNudgeEnabled())
    return;

  base::Time recheck_time;
  if (!ShouldShowNudge(recheck_time)) {
    // If `recheck_time` is set, start the timer to check again later for the
    // next time to show nudge.
    if (!recheck_time.is_null())
      ScheduleShowNudgeAttempt(recheck_time);

    return;
  }

  // Don't run the nudge animation if the duration multiplier is 0 to prevent
  // crashes that caused by showing the animation that immediately gets deleted.
  if (ui::ScopedAnimationDurationScaleMode::duration_multiplier() != 0) {
    // Only show the nudge on the home button which is on the same display with
    // the cursor.
    int64_t display_id_for_nudge =
        Shell::Get()->cursor_manager()->GetDisplay().id();
    HomeButton* home_button = GetHomeButtonForDisplay(display_id_for_nudge);
    home_button->StartNudgeAnimation();
    // Only update the prefs if the nudge animation is actually shown.
    HandleNudgeShown();
  }

  // Schedule the next attempt to show nudge if the shown count hasn't hit the
  // limit after showing a nudge.
  PrefService* prefs = GetPrefs();
  if (GetShownCount(prefs) < kMaxShownCount) {
    ScheduleShowNudgeAttempt(GetLastShownTime(prefs) +
                             GetNudgeInterval(/*is_first_time=*/false));
  }
}

void LauncherNudgeController::ScheduleShowNudgeAttempt(
    base::Time recheck_time) {
  show_nudge_timer_->Start(
      FROM_HERE, recheck_time,
      base::BindOnce(&LauncherNudgeController::MaybeShowNudge,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LauncherNudgeController::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  if (Shell::Get()->session_controller()->IsUserFirstLogin()) {
    // If the current logged in user is a new one, record the first login time
    // to know when to show the nudge.
    DictionaryPrefUpdate update(prefs, prefs::kShelfLauncherNudge);
    update->SetPath(kFirstLoginTime, base::TimeToValue(GetNow()));
  } else if (GetFirstLoginTime(prefs).is_null()) {
    // For the users that has logged in before the nudge feature is landed, we
    // assume the user has opened the launcher before and thus don't show the
    // nudge to them.
    return;
  }

  // Check if the nudge should be shown right away and show the nudge
  // if necessary.
  MaybeShowNudge();
}

void LauncherNudgeController::OnAppListVisibilityChanged(bool shown,
                                                         int64_t display_id) {
  PrefService* prefs = GetPrefs();
  if (!prefs)
    return;

  // App list is shown by default in tablet mode, and does not necessary
  // require explicit user action. As a result, don't track app list visibility
  // changes in tablet mode as actions affecting nudge availability in clamshell
  // mode.
  if (Shell::Get()->IsInTabletMode())
    return;

  if (!WasLauncherShownPreviously(prefs) && shown) {
    DictionaryPrefUpdate update(prefs, prefs::kShelfLauncherNudge);
    update->SetBoolPath(kWasLauncherShown, true);
  }
}

void LauncherNudgeController::OnTabletModeEnded() {
  // If a nudge event became available while the device was in tablet mode, it
  // would have been ignored. Recheck whether the nudge can be shown again.
  MaybeShowNudge();
}

void LauncherNudgeController::OnTabletControllerDestroyed() {
  tablet_mode_observation_.Reset();
}

base::Time LauncherNudgeController::GetNow() const {
  if (clock_for_test_)
    return clock_for_test_->Now();

  return base::Time::Now();
}

}  // namespace ash