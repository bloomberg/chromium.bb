// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_features.h"

namespace chromeos {

// Feature flag for disable/enable Lacros TTS support.
// Disable by default before the feature is completedly implemented.
const base::Feature kLacrosTtsSupport{"LacrosTtsSupport",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

namespace features {

// Enables or disables the ability to use advertisement monitoring.
// Advertisement monitoring allows applications to register low energy scanners
// that filter low energy advertisements in a power-efficient manner.
const base::Feature kBluetoothAdvertisementMonitoring{
    "BluetoothAdvertisementMonitoring", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables more filtering out of phones from the Bluetooth UI.
const base::Feature kBluetoothPhoneFilter{"BluetoothPhoneFilter",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables dark/light mode feature.
const base::Feature kDarkLightMode{"DarkLightMode",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Disables translation services of the Quick Answers V2.
const base::Feature kDisableQuickAnswersV2Translation{
    "DisableQuickAnswersV2Translation", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable quick answers V2 settings sub-toggles.
const base::Feature kQuickAnswersV2SettingsSubToggle{
    "QuickAnswersV2SettingsSubToggle", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsBluetoothAdvertisementMonitoringEnabled() {
  return base::FeatureList::IsEnabled(kBluetoothAdvertisementMonitoring);
}

bool IsDarkLightModeEnabled() {
  return base::FeatureList::IsEnabled(kDarkLightMode);
}

bool IsQuickAnswersV2TranslationDisabled() {
  return base::FeatureList::IsEnabled(kDisableQuickAnswersV2Translation);
}

bool IsQuickAnswersV2SettingsSubToggleEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersV2SettingsSubToggle);
}

}  // namespace features
}  // namespace chromeos
