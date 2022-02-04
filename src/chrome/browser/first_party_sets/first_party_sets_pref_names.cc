// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace first_party_sets {

// *************** LOCAL STATE PREFS ***************

// A boolean pref indicating whether First-Party Sets is enabled by enterprise
// policy.
const char kFirstPartySetsEnabled[] = "first_party_sets.enabled";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kFirstPartySetsEnabled, true);
}

}  // namespace first_party_sets
