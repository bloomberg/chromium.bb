// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace offline_pages {
namespace prefetch_prefs {
namespace {
// Prefs only accessed in this file
const char kEnabled[] = "offline_prefetch.enabled";
}  // namespace

const char kBackoff[] = "offline_prefetch.backoff";

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kBackoff);
  registry->RegisterBooleanPref(kEnabled, true);
}

void SetPrefetchingEnabledInSettings(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kEnabled, enabled);
}

bool IsEnabled(PrefService* prefs) {
  return IsPrefetchingOfflinePagesEnabled() && prefs->GetBoolean(kEnabled);
}

}  // namespace prefetch_prefs
}  // namespace offline_pages
