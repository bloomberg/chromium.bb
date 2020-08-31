// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_sharing_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"

const char kNearbySharingEnabledPrefName[] = "nearby_sharing.enabled";

void RegisterNearbySharingPrefs(user_prefs::PrefRegistrySyncable* registry) {
  // This pref is not synced.
  // TODO(vecore): Change the default to false after the settings ui is
  // available.
  registry->RegisterBooleanPref(
      kNearbySharingEnabledPrefName, true /* default_value */,
      PrefRegistry::PrefRegistrationFlags::NO_REGISTRATION_FLAGS /* flags */);
}
