// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_gcm_manager.h"

#include "chromeos/services/device_sync/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace chromeos {

namespace device_sync {

CryptAuthGCMManager::Observer::~Observer() {}

void CryptAuthGCMManager::Observer::OnGCMRegistrationResult(bool success) {}

void CryptAuthGCMManager::Observer::OnReenrollMessage() {}

void CryptAuthGCMManager::Observer::OnResyncMessage() {}

// static.
void CryptAuthGCMManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kCryptAuthGCMRegistrationId,
                               std::string());
}

}  // namespace device_sync

}  // namespace chromeos
