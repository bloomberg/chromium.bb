// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace offline_pages {
namespace prefetch_prefs {
namespace {
// Prefs only accessed in this file
const char kEnabled[] = "offline_prefetch.enabled";
const char kLimitlessPrefetchingEnabledTimePref[] =
    "offline_prefetch.limitless_prefetching_enabled_time";
const char kSendPrefetchTestingHeaderPref[] =
    "offline_prefetch.testing_header_value";
}  // namespace

const char kBackoff[] = "offline_prefetch.backoff";

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kBackoff);
  registry->RegisterBooleanPref(kEnabled, true);
  registry->RegisterTimePref(kLimitlessPrefetchingEnabledTimePref,
                             base::Time());
  registry->RegisterStringPref(kSendPrefetchTestingHeaderPref, std::string());
}

void SetPrefetchingEnabledInSettings(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kEnabled, enabled);
}

bool IsEnabled(PrefService* prefs) {
  return IsPrefetchingOfflinePagesEnabled() && prefs->GetBoolean(kEnabled);
}

void SetLimitlessPrefetchingEnabled(PrefService* prefs, bool enabled) {
  DCHECK(prefs);
  if (enabled)
    prefs->SetTime(kLimitlessPrefetchingEnabledTimePref, OfflineTimeNow());
  else
    prefs->SetTime(kLimitlessPrefetchingEnabledTimePref, base::Time());
}

bool IsLimitlessPrefetchingEnabled(PrefService* prefs) {
  base::TimeDelta max_duration;
  if (version_info::IsOfficialBuild())
    max_duration = base::TimeDelta::FromDays(1);
  else
    max_duration = base::TimeDelta::FromDays(365);

  DCHECK(prefs);
  const base::Time enabled_time =
      prefs->GetTime(kLimitlessPrefetchingEnabledTimePref);
  const base::Time now = OfflineTimeNow();

  return (now >= enabled_time) && (now < (enabled_time + max_duration));
}

void SetPrefetchTestingHeader(PrefService* prefs, const std::string& value) {
  DCHECK(prefs);
  prefs->SetString(kSendPrefetchTestingHeaderPref, value);
}

std::string GetPrefetchTestingHeader(PrefService* prefs) {
  DCHECK(prefs);
  return prefs->GetString(kSendPrefetchTestingHeaderPref);
}

}  // namespace prefetch_prefs
}  // namespace offline_pages
