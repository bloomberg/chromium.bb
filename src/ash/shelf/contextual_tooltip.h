// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_CONTEXTUAL_TOOLTIP_H_
#define ASH_SHELF_CONTEXTUAL_TOOLTIP_H_

#include "ash/ash_export.h"
#include "base/time/clock.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace contextual_tooltip {

// Enumeration of all contextual tooltips.
enum class TooltipType {
  kBackGesture,
  kHomeToOverview,
  kInAppToHome,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. These values enumerate the reasons a
// contextual nudge may be hidden.
enum class DismissNudgeReason {
  kOther = 0,
  kPerformedGesture = 1,
  kTap = 2,
  kSwitchToClamshell = 3,
  kExitToHomeScreen = 4,
  kTimeout = 5,
  kActiveWindowChanged = 6,     // dismisses back gesture nudge
  kNavigationEntryChanged = 7,  // dismisses back gesture nudge
  kBackGestureStarted = 8,      // dismisses back gesture nudge
  kUserSessionInactive = 9,     // dismisses back gesture nudge
  kMaxValue = kUserSessionInactive,
};

// Maximum number of times a user can be shown a contextual nudge if the user
// hasn't performed the gesture |kSuccessLimit| times successfully.
constexpr int kNotificationLimit = 3;
constexpr int kSuccessLimitInAppToHome = 7;
constexpr int kSuccessLimitHomeToOverview = 3;
constexpr int kSuccessLimitBackGesture = 1;

// Minimum time between showing contextual nudges to the user.
constexpr base::TimeDelta kMinInterval = base::TimeDelta::FromDays(1);

// The amount of time a nudge is shown.
constexpr base::TimeDelta kNudgeShowDuration = base::TimeDelta::FromSeconds(5);

// The minimum amount of time that has to pass since showing drag handle nudge
// before showing the back gesture nudge.
constexpr base::TimeDelta kMinIntervalBetweenBackAndDragHandleNudge =
    base::TimeDelta::FromMinutes(1);

// Registers profile prefs.
ASH_EXPORT void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns true if the contextual tooltip of |type| should be shown for the user
// with the given |prefs|.
// If the nudge should not be shown at this time, the |recheck_delay| will be
// set to the time delta after which the nudge might become available. If nudge
// is not expected to be shown any longer, or it's not known when the nudge
// might become available again (e.g. for drag handle nudge if the shelf is
// hidden) |recheck_delay| will be set to zero.
// |recheck_delay| might be nullptr, in which case it will be ignored.
ASH_EXPORT bool ShouldShowNudge(PrefService* prefs,
                                TooltipType type,
                                base::TimeDelta* recheck_delay);

// Checks whether the tooltip should be hidden after a timeout. Returns the
// timeout if it should, returns base::TimeDelta() if not.
ASH_EXPORT base::TimeDelta GetNudgeTimeout(PrefService* prefs,
                                           TooltipType type);

// Returns the number of counts that |type| nudge has been shown for the user
// with the given |prefs|.
ASH_EXPORT int GetShownCount(PrefService* prefs, TooltipType type);

// Increments the counter tracking the number of times the tooltip has been
// shown. Updates the last shown time for the tooltip.
ASH_EXPORT void HandleNudgeShown(PrefService* prefs, TooltipType type);

// Increments the counter tracking the number of times the tooltip's
// correpsonding gesture has been performed successfully.
ASH_EXPORT void HandleGesturePerformed(PrefService* prefs, TooltipType type);

// Sets whether drag handle nudge should be prevented from showing because the
// shelf is in hidden state.
ASH_EXPORT void SetDragHandleNudgeDisabledForHiddenShelf(bool nudge_disabled);

// Sets whether the back gesture nudge is being shown (back gesture can be
// visible before HandleNudgeShown gets called).
ASH_EXPORT void SetBackGestureNudgeShowing(bool showing);

//  Handles metrics tracking the nudge being dismissed.
ASH_EXPORT void LogNudgeDismissedMetrics(TooltipType type,
                                         DismissNudgeReason reason);

// Resets all user prefs related to contextual tooltips.
ASH_EXPORT void ClearPrefs();

ASH_EXPORT void OverrideClockForTesting(base::Clock* test_clock);

ASH_EXPORT void ClearClockOverrideForTesting();

// Reset the dictionary tracking metrics for each TooltipType.
ASH_EXPORT void ClearStatusTrackerTableForTesting();

// Checks whether the tracker for |type| is tracking user gestures.
ASH_EXPORT bool CanRecordGesturePerformedMetricForTesting(TooltipType type);

// Checks whether the tracker for |type| is tracking tooltip visibility.
ASH_EXPORT bool CanRecordNudgeHiddenMetricForTesting(TooltipType type);

}  // namespace contextual_tooltip

}  // namespace ash

#endif  // ASH_SHELF_CONTEXTUAL_TOOLTIP_H_
