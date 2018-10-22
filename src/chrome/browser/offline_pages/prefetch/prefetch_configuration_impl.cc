// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/prefetch_configuration_impl.h"

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace offline_pages {

// static
void PrefetchConfigurationImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kOfflinePrefetchEnabled, true);
}

PrefetchConfigurationImpl::PrefetchConfigurationImpl(PrefService* pref_service)
    : pref_service_(pref_service) {}

PrefetchConfigurationImpl::~PrefetchConfigurationImpl() = default;

bool PrefetchConfigurationImpl::IsPrefetchingEnabledBySettings() {
  return pref_service_->GetBoolean(prefs::kOfflinePrefetchEnabled);
}

void PrefetchConfigurationImpl::SetPrefetchingEnabledInSettings(bool enabled) {
  pref_service_->SetBoolean(prefs::kOfflinePrefetchEnabled, enabled);
}

}  // namespace offline_pages
