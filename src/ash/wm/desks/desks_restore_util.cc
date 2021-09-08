// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_restore_util.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/desks_util.h"
#include "base/auto_reset.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

namespace desks_restore_util {

namespace {

// |kDesksMetricsList| stores a list of dictionaries with the following
// key value pairs (<key> : <entry>):
// |kCreationTimeKey| : an int which represents the number of minutes for
// base::Time::FromDeltaSinceWindowsEpoch().
// |kFirstDayVisitedKey| : an int which represents the number of days since
// local epoch (Jan 1, 2010).
// |kLastDayVisitedKey| : an int which represents the number of days since
// local epoch (Jan 1, 2010).
// |kInteractedWithThisWeekKey| : a boolean tracking whether this desk has been
// interacted with in the last week.
constexpr char kCreationTimeKey[] = "creation_time";
constexpr char kFirstDayVisitedKey[] = "first_day";
constexpr char kLastDayVisitedKey[] = "last_day";
constexpr char kInteractedWithThisWeekKey[] = "interacted_week";

// |kDesksWeeklyActiveDesksMetrics| stores a dictionary with the following key
// value pairs (<key> : <entry>):
// |kWeeklyActiveDesksKey| : an int representing the number of weekly active
// desks.
// |kReportTimeKey| : an int respresenting the time a user's weekly active desks
// metric is scheduled to go off at. The value is the time left on the
// scheduler + the user's current time stored as the number of minutes for
// base::Time::FromDeltaSinceWindowsEpoch().
constexpr char kWeeklyActiveDesksKey[] = "weekly_active_desks";
constexpr char kReportTimeKey[] = "report_time";

// While restore is in progress, changes are being made to the desks and their
// names. Those changes should not trigger an update to the prefs.
bool g_pause_desks_prefs_updates = false;

PrefService* GetPrimaryUserPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

// Check if the desk index is valid against a list of existing desks in
// DesksController.
bool IsValidDeskIndex(int desk_index) {
  return desk_index >= 0 &&
         desk_index < int{DesksController::Get()->desks().size()} &&
         desk_index < int{desks_util::kMaxNumberOfDesks};
}

base::Time GetTime(int year, int month, int day_of_month, int day_of_week) {
  base::Time::Exploded time_exploded;
  time_exploded.year = year;
  time_exploded.month = month;
  time_exploded.day_of_week = day_of_week;
  time_exploded.day_of_month = day_of_month;
  time_exploded.hour = 0;
  time_exploded.minute = 0;
  time_exploded.second = 0;
  time_exploded.millisecond = 0;
  base::Time time;
  const bool result = base::Time::FromLocalExploded(time_exploded, &time);
  DCHECK(result);
  return time;
}

// Check if base::Time::Now() is during the time period 07/27/2021 to
// 09/07/2021.
bool IsNowInValidTimePeriod() {
  const auto now = base::Time::Now();
  return now <= GetTime(2021, 9, 7, /*Tuesday=*/2) &&
         now >= GetTime(2021, 7, 27, /*Tuesday=*/2);
}

}  // namespace

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  constexpr int kDefaultActiveDeskIndex = 0;
  registry->RegisterListPref(prefs::kDesksNamesList);
  registry->RegisterListPref(prefs::kDesksMetricsList);
  registry->RegisterDictionaryPref(prefs::kDesksWeeklyActiveDesksMetrics);
  registry->RegisterIntegerPref(prefs::kDesksActiveDesk,
                                kDefaultActiveDeskIndex);
  registry->RegisterBooleanPref(prefs::kUserHasUsedDesksRecently,
                                /*default_value=*/false);
}

void RestorePrimaryUserDesks() {
  base::AutoReset<bool> in_progress(&g_pause_desks_prefs_updates, true);

  PrefService* primary_user_prefs = GetPrimaryUserPrefService();
  if (!primary_user_prefs) {
    // Can be null in tests.
    return;
  }

  const base::ListValue* desks_names =
      primary_user_prefs->GetList(prefs::kDesksNamesList);
  const base::ListValue* desks_metrics =
      primary_user_prefs->GetList(prefs::kDesksMetricsList);

  // First create the same number of desks.
  const size_t restore_size = desks_names->GetSize();

  // If we don't have any restore data, or the list is corrupt for some reason,
  // abort.
  if (!restore_size || restore_size > desks_util::kMaxNumberOfDesks)
    return;

  auto* desks_controller = DesksController::Get();
  while (desks_controller->desks().size() < restore_size)
    desks_controller->NewDesk(DesksCreationRemovalSource::kDesksRestore);

  const auto& desks_names_list = desks_names->GetList();
  const auto& desks_metrics_list = desks_metrics->GetList();
  const size_t desks_metrics_list_size = desks_metrics->GetSize();
  const auto now = base::Time::Now();
  for (size_t index = 0; index < restore_size; ++index) {
    const std::string& desk_name = desks_names_list[index].GetString();
    // Empty desks names are just place holders for desks whose names haven't
    // been modified by the user. Those don't need to be restored; they already
    // have the correct default names based on their positions in the desks
    // list.
    if (!desk_name.empty()) {
      desks_controller->RestoreNameOfDeskAtIndex(base::UTF8ToUTF16(desk_name),
                                                 index);
    }

    // Only restore metrics if there is existing data.
    if (index >= desks_metrics_list_size)
      continue;

    const auto& desks_metrics_dict = desks_metrics_list[index];

    // Restore creation time.
    const auto& creation_time_entry =
        desks_metrics_dict.FindIntPath(kCreationTimeKey);
    if (creation_time_entry.has_value()) {
      const auto creation_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMinutes(*creation_time_entry));
      if (!creation_time.is_null() && creation_time < now)
        desks_controller->RestoreCreationTimeOfDeskAtIndex(creation_time,
                                                           index);
    }

    // Restore consecutive daily metrics.
    const auto& first_day_visited_entry =
        desks_metrics_dict.FindIntPath(kFirstDayVisitedKey);
    const int first_day_visited = first_day_visited_entry.value_or(-1);

    const auto& last_day_visited_entry =
        desks_metrics_dict.FindIntPath(kLastDayVisitedKey);
    const int last_day_visited = last_day_visited_entry.value_or(-1);

    if (first_day_visited <= last_day_visited && first_day_visited != -1 &&
        last_day_visited != -1) {
      // Only restore the values if they haven't been corrupted.
      desks_controller->RestoreVisitedMetricsOfDeskAtIndex(
          first_day_visited, last_day_visited, index);
    }

    // Restore weekly active desks metrics.
    const auto& interacted_with_this_week_entry =
        desks_metrics_dict.FindBoolPath(kInteractedWithThisWeekKey);
    const bool interacted_with_this_week =
        interacted_with_this_week_entry.value_or(false);
    if (interacted_with_this_week) {
      desks_controller->RestoreWeeklyInteractionMetricOfDeskAtIndex(
          interacted_with_this_week, index);
    }
  }

  // Restore an active desk for the primary user.
  const int active_desk_index =
      primary_user_prefs->GetInteger(prefs::kDesksActiveDesk);

  // A crash in between prefs::kDesksNamesList and prefs::kDesksActiveDesk
  // can cause an invalid active desk index.
  if (!IsValidDeskIndex(active_desk_index))
    return;

  desks_controller->RestorePrimaryUserActiveDeskIndex(active_desk_index);

  // Restore weekly active desks metrics.
  auto* weekly_active_desks_dict =
      primary_user_prefs->GetDictionary(prefs::kDesksWeeklyActiveDesksMetrics);
  if (weekly_active_desks_dict) {
    const int report_time =
        weekly_active_desks_dict->FindIntPath(kReportTimeKey).value_or(-1);
    const int num_weekly_active_desks =
        weekly_active_desks_dict->FindIntPath(kWeeklyActiveDesksKey)
            .value_or(-1);

    // Discard stored metrics if either are corrupted.
    if (report_time != -1 && num_weekly_active_desks != -1) {
      desks_controller->RestoreWeeklyActiveDesksMetrics(
          num_weekly_active_desks,
          base::Time::FromDeltaSinceWindowsEpoch(
              base::TimeDelta::FromMinutes(report_time)));
    }
  }
}

void UpdatePrimaryUserDeskNamesPrefs() {
  if (g_pause_desks_prefs_updates)
    return;

  PrefService* primary_user_prefs = GetPrimaryUserPrefService();
  if (!primary_user_prefs) {
    // Can be null in tests.
    return;
  }

  ListPrefUpdate name_update(primary_user_prefs, prefs::kDesksNamesList);
  base::ListValue* name_pref_data = name_update.Get();
  name_pref_data->Clear();

  const auto& desks = DesksController::Get()->desks();
  for (const auto& desk : desks) {
    // Desks whose names were not changed by the user, are stored as empty
    // strings. They're just place holders to restore the correct desks count.
    // RestorePrimaryUserDesks() restores only non-empty desks names.
    name_pref_data->Append(desk->is_name_set_by_user()
                               ? base::UTF16ToUTF8(desk->name())
                               : std::string());
  }

  DCHECK_EQ(name_pref_data->GetSize(), desks.size());

  if (IsNowInValidTimePeriod() &&
      !primary_user_prefs->GetBoolean(prefs::kUserHasUsedDesksRecently)) {
    primary_user_prefs->SetBoolean(prefs::kUserHasUsedDesksRecently, true);
  }
}

void UpdatePrimaryUserDeskMetricsPrefs() {
  if (g_pause_desks_prefs_updates)
    return;

  PrefService* primary_user_prefs = GetPrimaryUserPrefService();
  if (!primary_user_prefs) {
    // Can be null in tests.
    return;
  }

  // Save per-desk metrics.
  ListPrefUpdate metrics_update(primary_user_prefs, prefs::kDesksMetricsList);
  base::ListValue* metrics_pref_data = metrics_update.Get();
  metrics_pref_data->Clear();

  auto* desks_controller = DesksController::Get();
  const auto& desks = desks_controller->desks();
  for (const auto& desk : desks) {
    base::DictionaryValue metrics_dict;
    metrics_dict.SetInteger(
        kCreationTimeKey,
        desk->creation_time().ToDeltaSinceWindowsEpoch().InMinutes());
    metrics_dict.SetInteger(kFirstDayVisitedKey, desk->first_day_visited());
    metrics_dict.SetInteger(kLastDayVisitedKey, desk->last_day_visited());
    metrics_dict.SetBoolean(kInteractedWithThisWeekKey,
                            desk->interacted_with_this_week());
    metrics_pref_data->Append(std::move(metrics_dict));
  }

  DCHECK_EQ(metrics_pref_data->GetSize(), desks.size());

  // Save weekly active report time.
  DictionaryPrefUpdate weekly_active_desks_update(
      primary_user_prefs, prefs::kDesksWeeklyActiveDesksMetrics);
  weekly_active_desks_update->SetIntPath(
      kReportTimeKey, desks_controller->GetWeeklyActiveReportTime()
                          .ToDeltaSinceWindowsEpoch()
                          .InMinutes());
  weekly_active_desks_update->SetIntPath(kWeeklyActiveDesksKey,
                                         Desk::GetWeeklyActiveDesks());
}

void UpdatePrimaryUserActiveDeskPrefs(int active_desk_index) {
  DCHECK(IsValidDeskIndex(active_desk_index));
  if (g_pause_desks_prefs_updates)
    return;

  PrefService* primary_user_prefs = GetPrimaryUserPrefService();
  if (!primary_user_prefs) {
    // Can be null in tests.
    return;
  }

  primary_user_prefs->SetInteger(prefs::kDesksActiveDesk, active_desk_index);
}

}  // namespace desks_restore_util

}  // namespace ash
