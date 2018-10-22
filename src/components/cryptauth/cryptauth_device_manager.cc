// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/cryptauth_device_manager.h"

#include "components/cryptauth/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace cryptauth {

// static
void CryptAuthDeviceManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDoublePref(prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds,
                               0.0);
  registry->RegisterBooleanPref(
      prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure, false);
  registry->RegisterIntegerPref(prefs::kCryptAuthDeviceSyncReason,
                                INVOCATION_REASON_UNKNOWN);
  registry->RegisterListPref(prefs::kCryptAuthDeviceSyncUnlockKeys);
}

CryptAuthDeviceManager::CryptAuthDeviceManager() = default;

CryptAuthDeviceManager::~CryptAuthDeviceManager() = default;

void CryptAuthDeviceManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CryptAuthDeviceManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CryptAuthDeviceManager::NotifySyncStarted() {
  for (auto& observer : observers_)
    observer.OnSyncStarted();
}

void CryptAuthDeviceManager::NotifySyncFinished(
    SyncResult sync_result,
    DeviceChangeResult device_change_result) {
  for (auto& observer : observers_)
    observer.OnSyncFinished(sync_result, device_change_result);
}

}  // namespace cryptauth
