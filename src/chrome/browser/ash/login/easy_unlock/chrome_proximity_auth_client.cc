// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/chrome_proximity_auth_client.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/ash/device_sync/device_sync_client_factory.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service_regular.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service_signin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace ash {

ChromeProximityAuthClient::ChromeProximityAuthClient(Profile* profile)
    : profile_(profile) {}

ChromeProximityAuthClient::~ChromeProximityAuthClient() {}

void ChromeProximityAuthClient::UpdateSmartLockState(SmartLockState state) {
  EasyUnlockService* service = EasyUnlockService::Get(profile_);
  if (service)
    service->UpdateSmartLockState(state);
}

void ChromeProximityAuthClient::FinalizeUnlock(bool success) {
  EasyUnlockService* service = EasyUnlockService::Get(profile_);
  if (service)
    service->FinalizeUnlock(success);
}

void ChromeProximityAuthClient::FinalizeSignin(const std::string& secret) {
  EasyUnlockService* service = EasyUnlockService::Get(profile_);
  if (service)
    service->FinalizeSignin(secret);
}

void ChromeProximityAuthClient::GetChallengeForUserAndDevice(
    const std::string& user_email,
    const std::string& remote_public_key,
    const std::string& channel_binding_data,
    base::OnceCallback<void(const std::string& challenge)> callback) {
  EasyUnlockService* easy_unlock_service = EasyUnlockService::Get(profile_);
  if (easy_unlock_service->GetType() == EasyUnlockService::TYPE_REGULAR) {
    PA_LOG(ERROR) << "Unable to get challenge when user is logged in.";
    std::move(callback).Run(/*challenge=*/std::string());
    return;
  }

  static_cast<EasyUnlockServiceSignin*>(easy_unlock_service)
      ->WrapChallengeForUserAndDevice(AccountId::FromUserEmail(user_email),
                                      remote_public_key, channel_binding_data,
                                      std::move(callback));
}

proximity_auth::ProximityAuthPrefManager*
ChromeProximityAuthClient::GetPrefManager() {
  EasyUnlockService* service = EasyUnlockService::Get(profile_);
  if (service)
    return service->GetProximityAuthPrefManager();
  return nullptr;
}

}  // namespace ash
