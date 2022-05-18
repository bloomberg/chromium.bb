// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/flags/android/chrome_session_state.h"

#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flags/jni_headers/ChromeSessionState_jni.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/metrics/public/cpp/ukm_source.h"

using chrome::android::ActivityType;
using chrome::android::DarkModeState;

namespace {
ActivityType activity_type = ActivityType::kUndeclared;
bool is_in_multi_window_mode = false;
DarkModeState dark_mode_state = DarkModeState::kUnknown;

// Name of local state pref to persist the last |chrome::android::ActivityType|.
const char kLastActivityTypePref[] =
    "user_experience_metrics.last_activity_type";

}  // namespace

namespace chrome {
namespace android {

// TODO(b/182286787): A/B experiment monitoring session/activity resume order.
const base::Feature kFixedUmaSessionResumeOrder{
    "FixedUmaSessionResumeOrder", base::FEATURE_DISABLED_BY_DEFAULT};

CustomTabsVisibilityHistogram GetCustomTabsVisibleValue(
    ActivityType activity_type) {
  switch (activity_type) {
    case ActivityType::kTabbed:
    case ActivityType::kWebapp:
    case ActivityType::kWebApk:
      return VISIBLE_CHROME_TAB;
    case ActivityType::kCustomTab:
    case ActivityType::kTrustedWebActivity:
      return VISIBLE_CUSTOM_TAB;
    case ActivityType::kUndeclared:
      return NO_VISIBLE_TAB;
  }
  NOTREACHED();
  return VISIBLE_CHROME_TAB;
}

ActivityType GetInitialActivityTypeForTesting() {
  return activity_type;
}

void SetInitialActivityTypeForTesting(ActivityType type) {
  activity_type = type;
}

void SetActivityType(PrefService* local_state, ActivityType type) {
  DCHECK(local_state);
  DCHECK_NE(type, ActivityType::kUndeclared);

  ActivityType prev_activity_type = activity_type;
  activity_type = type;

  // EmitActivityTypeHistograms on first SetActivityType call if using the fixed
  // uma session restore order (b/182286787).
  if (prev_activity_type == ActivityType::kUndeclared &&
      base::FeatureList::IsEnabled(kFixedUmaSessionResumeOrder)) {
    EmitActivityTypeHistograms(activity_type);
    SaveActivityTypeToLocalState(local_state, activity_type);
  }

  // TODO(crbug/1228735): deprecate custom tab field.
  ukm::UkmSource::SetCustomTabVisible(
      GetCustomTabsVisibleValue(activity_type) == VISIBLE_CUSTOM_TAB);
  ukm::UkmSource::SetAndroidActivityTypeState(static_cast<int>(activity_type));
}

ActivityType GetActivityType() {
  // TODO(b/182286787): With old session resume order, the initial state is
  // kTabbed.
  if (activity_type == ActivityType::kUndeclared &&
      !base::FeatureList::IsEnabled(kFixedUmaSessionResumeOrder)) {
    activity_type = ActivityType::kTabbed;
  }
  return activity_type;
}

DarkModeState GetDarkModeState() {
  return dark_mode_state;
}

bool GetIsInMultiWindowModeValue() {
  return is_in_multi_window_mode;
}

void EmitActivityTypeHistograms(ActivityType type) {
  UMA_STABILITY_HISTOGRAM_ENUMERATION("CustomTabs.Visible",
                                      GetCustomTabsVisibleValue(type));
  UMA_STABILITY_HISTOGRAM_ENUMERATION("Android.ChromeActivity.Type", type);
}

void RegisterActivityTypePrefs(PrefRegistrySimple* registry) {
  DCHECK(registry);
  // Register with a default value of -1 which is not a valid enum value.
  registry->RegisterIntegerPref(kLastActivityTypePref, -1);
}

absl::optional<chrome::android::ActivityType> GetActivityTypeFromLocalState(
    PrefService* local_state) {
  auto value = local_state->GetInteger(kLastActivityTypePref);
  if (value >= static_cast<int>(ActivityType::kTabbed) &&
      value <= static_cast<int>(ActivityType::kMaxValue)) {
    return static_cast<ActivityType>(value);
  }
  return absl::nullopt;
}

void SaveActivityTypeToLocalState(PrefService* local_state,
                                  chrome::android::ActivityType value) {
  local_state->SetInteger(kLastActivityTypePref, static_cast<int>(value));
}

}  // namespace android
}  // namespace chrome

static void JNI_ChromeSessionState_SetActivityType(JNIEnv* env, jint type) {
  DCHECK(g_browser_process);
  DCHECK(g_browser_process->local_state());
  chrome::android::SetActivityType(g_browser_process->local_state(),
                                   static_cast<ActivityType>(type));
}

static void JNI_ChromeSessionState_SetDarkModeState(JNIEnv* env, jint state) {
  dark_mode_state = static_cast<DarkModeState>(state);
}

static void JNI_ChromeSessionState_SetIsInMultiWindowMode(
    JNIEnv* env,
    jboolean j_is_in_multi_window_mode) {
  is_in_multi_window_mode = j_is_in_multi_window_mode;
}
