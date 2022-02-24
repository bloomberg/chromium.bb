// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_NUDGE_CONTROLLER_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_NUDGE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {
enum class AppListSortOrder;

// Controls and coordinates the toast nudge views in app list. Note that it
// currently assumes that at most one nudge could be visible at a time.
class ASH_EXPORT AppListNudgeController {
 public:
  // The type of nudge that is currently showing.
  enum class NudgeType {
    // No nudge in app list is showing.
    kNone,
    // A nudge in continue section which notifies that recommended files are
    // going to be shown in continue section.
    kPrivacyNotice,
    // A nudge in app list that guide users to reorder apps using context menu.
    kReorderNudge,
  };

  AppListNudgeController();
  AppListNudgeController(const AppListNudgeController&) = delete;
  AppListNudgeController& operator=(const AppListNudgeController&) = delete;
  ~AppListNudgeController() = default;

  // Registers profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Gets the number of times that the nudge with type `type` has been shown.
  static int GetShownCount(PrefService* prefs, NudgeType type);

  // Set the value of global variable `g_nudge_disabled_for_test` to disable
  // showing the nudge.
  static void SetNudgeDisabledForTest(bool is_disabled);

  // Returns true if the reorder nudge should be shown.
  bool ShouldShowReorderNudge() const;

  // Called when the app list temporary sort order changes. Null `new_order`
  // indicates that the temporary sort order is cleared.
  void OnTemporarySortOrderChanged(
      const absl::optional<AppListSortOrder>& new_order);

  // Updates the nudge type when the privacy notice is showing or hiding. The
  // caller of this function is responsible for the actual creation and removal
  // of the nudge view.
  void SetPrivacyNoticeShown(bool shown);

  // Sets the new nudge visible state and updates the prefs. The caller of
  // this function is responsible for the actual creation and removal of the
  // nudge view.
  void SetNudgeVisible(bool is_nudge_visible, NudgeType type);

  // Sets the new nudge active state and updates the prefs. The caller of
  // this function is responsible for the actual creation and removal of the
  // nudge view. Note that inactive nudge state does not necessarily mean that
  // the nudge is hidden. A inactive nudge could be visible in the background.
  void SetNudgeActive(bool is_nudge_active, NudgeType type);

  // Updates the the current nudge state in prefs to determine if a nudge should
  // be showing.
  void UpdateCurrentNudgeStateInPrefs(bool is_visible_updated,
                                      bool is_active_updated);

  NudgeType current_nudge() const { return current_nudge_; }
  bool is_visible() const { return is_visible_; }

 private:
  // If the nudge is hidden and the showing duration is long enough, increments
  // the shown count in prefs.
  void MaybeIncrementShownCountInPrefs(DictionaryPrefUpdate& update,
                                       base::TimeDelta duration);

  // The timestamp when the current nudge started showing.
  base::Time current_nudge_show_timestamp_;

  // Current type of nudge that is showing.
  NudgeType current_nudge_ = NudgeType::kNone;

  // Records if `current_nudge_` is visible.
  bool is_visible_ = false;

  // Records if `current_nudge_` is active. The nudge could be visible
  // but be shadowed by other views to be inactive.
  bool is_active_ = false;

  // Caches that the nudge is considered as shown before the next shown count
  // update.
  bool is_nudge_considered_as_shown_ = false;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_NUDGE_CONTROLLER_H_
