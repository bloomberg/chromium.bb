// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/kiosk_test_helpers.h"

#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace ash {

KioskSessionInitializedWaiter::KioskSessionInitializedWaiter() {
  scoped_observations_.AddObservation(KioskAppManager::Get());
  scoped_observations_.AddObservation(WebKioskAppManager::Get());
}

KioskSessionInitializedWaiter::~KioskSessionInitializedWaiter() = default;

void KioskSessionInitializedWaiter::Wait() {
  if (KioskAppManager::Get()->app_session() ||
      WebKioskAppManager::Get()->app_session()) {
    return;
  }

  run_loop_.Run();
}

void KioskSessionInitializedWaiter::OnKioskSessionInitialized() {
  run_loop_.Quit();
}

ScopedDeviceSettings::ScopedDeviceSettings() : settings_helper_(false) {
  settings_helper_.ReplaceDeviceSettingsProviderWithStub();
  owner_settings_service_ = settings_helper_.CreateOwnerSettingsService(
      ProfileManager::GetPrimaryUserProfile());
}

ScopedDeviceSettings::~ScopedDeviceSettings() = default;

ScopedCanConfigureNetwork::ScopedCanConfigureNetwork(bool can_configure,
                                                     bool needs_owner_auth)
    : can_configure_(can_configure),
      needs_owner_auth_(needs_owner_auth),
      can_configure_network_callback_(
          base::BindRepeating(&ScopedCanConfigureNetwork::CanConfigureNetwork,
                              base::Unretained(this))),
      needs_owner_auth_callback_(base::BindRepeating(
          &ScopedCanConfigureNetwork::NeedsOwnerAuthToConfigureNetwork,
          base::Unretained(this))) {
  KioskLaunchController::SetCanConfigureNetworkCallbackForTesting(
      &can_configure_network_callback_);
  KioskLaunchController::SetNeedOwnerAuthToConfigureNetworkCallbackForTesting(
      &needs_owner_auth_callback_);
}
ScopedCanConfigureNetwork::~ScopedCanConfigureNetwork() {
  KioskLaunchController::SetCanConfigureNetworkCallbackForTesting(nullptr);
  KioskLaunchController::SetNeedOwnerAuthToConfigureNetworkCallbackForTesting(
      nullptr);
}

}  // namespace ash
