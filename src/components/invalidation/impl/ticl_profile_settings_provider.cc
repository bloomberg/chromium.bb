// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/ticl_profile_settings_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "components/gcm_driver/gcm_channel_status_syncer.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace invalidation {

TiclProfileSettingsProvider::TiclProfileSettingsProvider(PrefService* prefs)
    : prefs_(prefs) {
  registrar_.Init(prefs_);
  registrar_.Add(
      gcm::prefs::kGCMChannelStatus,
      base::Bind(&TiclProfileSettingsProvider::FireOnUseGCMChannelChanged,
                 base::Unretained(this)));
}

TiclProfileSettingsProvider::~TiclProfileSettingsProvider() {}

bool TiclProfileSettingsProvider::UseGCMChannel() const {
  return true;
}

}  // namespace invalidation
