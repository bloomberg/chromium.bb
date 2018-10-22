// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/device_actions.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

using chromeos::NetworkHandler;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

DeviceActions::DeviceActions() {}

DeviceActions::~DeviceActions() = default;

void DeviceActions::SetWifiEnabled(bool enabled) {
  NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), enabled,
      chromeos::network_handler::ErrorCallback());
}

void DeviceActions::SetBluetoothEnabled(bool enabled) {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  DCHECK(profile);
  // Simply toggle the user pref, which is being observed by ash's bluetooth
  // power controller.
  profile->GetPrefs()->SetBoolean(ash::prefs::kUserBluetoothAdapterEnabled,
                                  enabled);
}
