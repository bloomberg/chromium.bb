// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_METRICS_H_
#define COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_METRICS_H_

class PrefService;

namespace syncer {
class SyncUserSettings;
}

namespace unified_consent {
namespace metrics {

// Google services that can be toggled in user settings.
// Used in histograms. Do not change existing values, append new values at the
// end.
enum class SettingsHistogramValue {
  kNone = 0,
  kUnifiedConsentGiven = 1,
  kUserEvents = 2,
  kUrlKeyedAnonymizedDataCollection = 3,
  kSafeBrowsingExtendedReporting = 4,
  kSpellCheck = 5,
  kAllServicesWereEnabled = 6,

  kMaxValue = kAllServicesWereEnabled
};

// Records settings entries in the SyncAndGoogleServicesSettings.
// kNone is recorded when none of the settings is enabled.
void RecordSettingsHistogram(PrefService* pref_service);

// Records the sync data types that were turned off during the advanced sync
// opt-in flow. When none of the data types were turned off, kNone is recorded.
void RecordSyncSetupDataTypesHistrogam(syncer::SyncUserSettings* sync_settings,
                                       PrefService* pref_service);

}  // namespace metrics
}  // namespace unified_consent

#endif  // COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_METRICS_H_
