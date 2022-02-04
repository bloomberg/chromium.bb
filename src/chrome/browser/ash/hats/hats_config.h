// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_HATS_HATS_CONFIG_H_
#define CHROME_BROWSER_ASH_HATS_HATS_CONFIG_H_

#include "base/time/time.h"

namespace base {
struct Feature;
}  // namespace base

namespace ash {

struct HatsConfig {
  HatsConfig(const base::Feature& feature,
             const char* const histogram_name,
             const base::TimeDelta& new_device_threshold,
             const char* const is_selected_pref_name,
             const char* const cycle_end_timestamp_pref_name);
  HatsConfig(const HatsConfig&) = delete;
  HatsConfig& operator=(const HatsConfig&) = delete;

  // Chrome OS-level feature (switch) we use to retrieve whether this HaTS
  // survey is enabled or not, and its parameters.
  const base::Feature& feature;

  // Histogram name. New entries should be added to
  // tools/metrics/histograms/metadata/browser/histograms.xml, as a variant of
  // Browser.ChromeOS.HatsSatisfaction.
  const char* const histogram_name;

  // Minimum amount of time after initial login or oobe after which we can show
  // the HaTS notification.
  const base::TimeDelta new_device_threshold;

  // Preference name for a boolean that stores whether we were selected for the
  // current survey cycle.
  const char* const is_selected_pref_name;

  // Preference name for an int64 that stores the current survey cycle end.
  const char* const cycle_end_timestamp_pref_name;
};

// CrOS HaTS configs are declared here and defined in hats_config.cc
extern const HatsConfig kHatsGeneralSurvey;
extern const HatsConfig kHatsEntSurvey;
extern const HatsConfig kHatsStabilitySurvey;
extern const HatsConfig kHatsPerformanceSurvey;
extern const HatsConfig kHatsOnboardingSurvey;
extern const HatsConfig kHatsSmartLockSurvey;
extern const HatsConfig kHatsUnlockSurvey;
extern const HatsConfig kHatsArcGamesSurvey;
extern const HatsConfig kHatsAudioSurvey;

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromeOS code migration is done.
namespace chromeos {
using ::ash::kHatsGeneralSurvey;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_HATS_HATS_CONFIG_H_
