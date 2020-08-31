// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/contextual_tooltip.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/contextual_nudge_status_tracker.h"
#include "ash/shell.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/util/values/values_util.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

namespace contextual_tooltip {

namespace {

// Keys for tooltip sub-preferences for shown count and last time shown.
constexpr char kShownCount[] = "shown_count";
constexpr char kLastTimeShown[] = "last_time_shown";

// Keys for tooltip sub-preferences of how many times a gesture has been
// successfully performed by the user.
constexpr char kSuccessCount[] = "success_count";

// Whether the drag handle nudge cannot be shown because the shelf is currently
// hidden - used to unblock showing back gesture when shelf is hidden (the back
// gesture will normally show only if the drag handle nudge has already been
// shown within the last nudge show interval).
bool g_drag_handle_nudge_disabled_for_hidden_shelf = false;

// Whether the back gesture nudge is currently being shown.
bool g_back_gesture_nudge_showing = false;

base::Clock* g_clock_override = nullptr;

base::Time GetTime() {
  if (g_clock_override)
    return g_clock_override->Now();
  return base::Time::Now();
}

std::string TooltipTypeToString(TooltipType type) {
  switch (type) {
    case TooltipType::kBackGesture:
      return "back_gesture";
    case TooltipType::kHomeToOverview:
      return "home_to_overview";
    case TooltipType::kInAppToHome:
      return "in_app_to_home";
  }
  return "invalid";
}

// Creates the path to the dictionary value from the contextual tooltip type and
// the sub-preference.
std::string GetPath(TooltipType type, const std::string& sub_pref) {
  return base::JoinString({TooltipTypeToString(type), sub_pref}, ".");
}

base::Time GetLastShownTime(PrefService* prefs, TooltipType type) {
  const base::Value* last_shown_time =
      prefs->GetDictionary(prefs::kContextualTooltips)
          ->FindPath(GetPath(type, kLastTimeShown));
  if (!last_shown_time)
    return base::Time();
  return *util::ValueToTime(last_shown_time);
}

int GetSuccessCount(PrefService* prefs, TooltipType type) {
  base::Optional<int> success_count =
      prefs->GetDictionary(prefs::kContextualTooltips)
          ->FindIntPath(GetPath(type, kSuccessCount));
  return success_count.value_or(0);
}

const base::Optional<base::TimeDelta>& GetMinIntervalOverride() {
  // Overridden minimum time between showing contextual nudges to the user.
  static base::Optional<base::TimeDelta> min_interval_override;
  if (!min_interval_override) {
    min_interval_override = switches::ContextualNudgesInterval();
  }
  return min_interval_override;
}

using TrackerTable =
    std::map<TooltipType, std::unique_ptr<ContextualNudgeStatusTracker>>;

TrackerTable& GetStatusTrackerTable() {
  // Dictionary mapping each nudge to its status tracker.
  static base::NoDestructor<TrackerTable> status_tracker_table;
  return *status_tracker_table;
}

ContextualNudgeStatusTracker* GetStatusTracker(TooltipType type) {
  if (GetStatusTrackerTable().find(type) == GetStatusTrackerTable().end()) {
    GetStatusTrackerTable().insert(TrackerTable::value_type(
        type, std::make_unique<ContextualNudgeStatusTracker>(type)));
  }
  return GetStatusTrackerTable().find(type)->second.get();
}

}  // namespace

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kContextualTooltips);
}

bool ShouldShowNudge(PrefService* prefs,
                     TooltipType type,
                     base::TimeDelta* recheck_delay) {
  auto set_recheck_delay = [&recheck_delay](base::TimeDelta delay) {
    if (recheck_delay)
      *recheck_delay = delay;
  };

  if (!features::AreContextualNudgesEnabled()) {
    set_recheck_delay(base::TimeDelta());
    return false;
  }

  if (type == TooltipType::kInAppToHome &&
      g_drag_handle_nudge_disabled_for_hidden_shelf) {
    set_recheck_delay(base::TimeDelta());
    return false;
  }

  const int success_count = GetSuccessCount(prefs, type);
  if ((type == TooltipType::kHomeToOverview &&
       success_count >= kSuccessLimitHomeToOverview) ||
      (type == TooltipType::kBackGesture &&
       success_count >= kSuccessLimitBackGesture) ||
      (type == TooltipType::kInAppToHome &&
       success_count >= kSuccessLimitInAppToHome)) {
    set_recheck_delay(base::TimeDelta());
    return false;
  }

  const int shown_count = GetShownCount(prefs, type);
  if (shown_count >= kNotificationLimit) {
    set_recheck_delay(base::TimeDelta());
    return false;
  }

  // Before showing back gesture nudge, do not show it if in-app to shelf nudge
  // should be shown (to prevent two nudges from showing up at the same time).
  // Verify that the in-app to home nudge was shown within the last show
  // interval.
  if (type == TooltipType::kBackGesture) {
    if (!g_drag_handle_nudge_disabled_for_hidden_shelf &&
        ShouldShowNudge(prefs, TooltipType::kInAppToHome, nullptr)) {
      set_recheck_delay(kMinIntervalBetweenBackAndDragHandleNudge);
      return false;
    }

    // Verify that drag handle nudge has been shown at least a minute ago.
    const base::Time drag_handle_nudge_last_shown_time =
        GetLastShownTime(prefs, TooltipType::kInAppToHome);
    if (!drag_handle_nudge_last_shown_time.is_null()) {
      const base::TimeDelta time_since_drag_handle_nudge =
          GetTime() - drag_handle_nudge_last_shown_time;
      if (time_since_drag_handle_nudge <
          kMinIntervalBetweenBackAndDragHandleNudge) {
        set_recheck_delay(kMinIntervalBetweenBackAndDragHandleNudge -
                          time_since_drag_handle_nudge);
        return false;
      }
    }
  }

  // Make sure that drag handle nudge is not shown within a minute of back
  // gesture nudge.
  if (type == TooltipType::kInAppToHome) {
    if (g_back_gesture_nudge_showing) {
      set_recheck_delay(kMinIntervalBetweenBackAndDragHandleNudge);
      return false;
    }

    const base::Time back_nudge_last_shown_time =
        GetLastShownTime(prefs, TooltipType::kBackGesture);
    if (!back_nudge_last_shown_time.is_null()) {
      const base::TimeDelta time_since_back_nudge =
          GetTime() - back_nudge_last_shown_time;
      if (time_since_back_nudge < kMinIntervalBetweenBackAndDragHandleNudge) {
        set_recheck_delay(kMinIntervalBetweenBackAndDragHandleNudge -
                          time_since_back_nudge);
        return false;
      }
    }
  }

  if (shown_count == 0)
    return true;

  const base::Time last_shown_time = GetLastShownTime(prefs, type);
  const base::TimeDelta min_interval =
      GetMinIntervalOverride().value_or(kMinInterval);
  const base::TimeDelta time_since_last_nudge = GetTime() - last_shown_time;
  if (time_since_last_nudge < min_interval) {
    set_recheck_delay(min_interval - time_since_last_nudge);
    return false;
  }
  return true;
}

base::TimeDelta GetNudgeTimeout(PrefService* prefs, TooltipType type) {
  const int shown_count = GetShownCount(prefs, type);
  if (shown_count == 0)
    return base::TimeDelta();
  return kNudgeShowDuration;
}

int GetShownCount(PrefService* prefs, TooltipType type) {
  base::Optional<int> shown_count =
      prefs->GetDictionary(prefs::kContextualTooltips)
          ->FindIntPath(GetPath(type, kShownCount));
  return shown_count.value_or(0);
}

void HandleNudgeShown(PrefService* prefs, TooltipType type) {
  const int shown_count = GetShownCount(prefs, type);
  DictionaryPrefUpdate update(prefs, prefs::kContextualTooltips);
  update->SetIntPath(GetPath(type, kShownCount), shown_count + 1);
  update->SetPath(GetPath(type, kLastTimeShown), util::TimeToValue(GetTime()));
  GetStatusTracker(type)->HandleNudgeShown(base::TimeTicks::Now());
}

void HandleGesturePerformed(PrefService* prefs, TooltipType type) {
  const int success_count = GetSuccessCount(prefs, type);
  DictionaryPrefUpdate update(prefs, prefs::kContextualTooltips);
  update->SetIntPath(GetPath(type, kSuccessCount), success_count + 1);
  GetStatusTracker(type)->HandleGesturePerformed(base::TimeTicks::Now());
}

void LogNudgeDismissedMetrics(TooltipType type, DismissNudgeReason reason) {
  GetStatusTracker(type)->LogNudgeDismissedMetrics(reason);
}

void SetDragHandleNudgeDisabledForHiddenShelf(bool nudge_disabled) {
  g_drag_handle_nudge_disabled_for_hidden_shelf = nudge_disabled;
}

void SetBackGestureNudgeShowing(bool showing) {
  g_back_gesture_nudge_showing = showing;
}

void ClearPrefs() {
  DCHECK(Shell::Get()->session_controller()->GetLastActiveUserPrefService());
  DictionaryPrefUpdate update(
      Shell::Get()->session_controller()->GetLastActiveUserPrefService(),
      prefs::kContextualTooltips);
  base::DictionaryValue* nudges_dict = update.Get();
  if (nudges_dict && !nudges_dict->empty())
    nudges_dict->Clear();
}

void OverrideClockForTesting(base::Clock* test_clock) {
  DCHECK(!g_clock_override);
  g_clock_override = test_clock;
}

void ClearClockOverrideForTesting() {
  DCHECK(g_clock_override);
  g_clock_override = nullptr;
}

void ClearStatusTrackerTableForTesting() {
  GetStatusTrackerTable().clear();
}

ASH_EXPORT bool CanRecordGesturePerformedMetricForTesting(TooltipType type) {
  return GetStatusTracker(type)->has_nudge_been_shown();
}

ASH_EXPORT bool CanRecordNudgeHiddenMetricForTesting(TooltipType type) {
  return GetStatusTracker(type)->visible();
}

}  // namespace contextual_tooltip

}  // namespace ash
