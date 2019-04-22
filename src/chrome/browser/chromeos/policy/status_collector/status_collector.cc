// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_collector/status_collector.h"

#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "components/user_manager/user_manager.h"

namespace policy {
namespace {

namespace ent_mgmt = ::enterprise_management;

// Returns the DeviceLocalAccount associated with the current kiosk session.
// Returns nullptr if there is no active kiosk session, or if that kiosk
// session has been removed from policy since the session started, in which
// case we won't report its status).
std::unique_ptr<DeviceLocalAccount> GetCurrentKioskDeviceLocalAccount(
    chromeos::CrosSettings* settings) {
  if (!user_manager::UserManager::Get()->IsLoggedInAsKioskApp() &&
      !user_manager::UserManager::Get()->IsLoggedInAsArcKioskApp()) {
    return nullptr;
  }
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  const std::vector<DeviceLocalAccount> accounts =
      GetDeviceLocalAccounts(settings);

  for (const auto& device_local_account : accounts) {
    if (AccountId::FromUserEmail(device_local_account.user_id) ==
        user->GetAccountId()) {
      return std::make_unique<DeviceLocalAccount>(device_local_account);
    }
  }
  LOG(WARNING) << "Kiosk app not found in list of device-local accounts";
  return nullptr;
}

}  // namespace

StatusCollectorParams::StatusCollectorParams() {
  device_status = std::make_unique<ent_mgmt::DeviceStatusReportRequest>();
  session_status = std::make_unique<ent_mgmt::SessionStatusReportRequest>();
  child_status = std::make_unique<ent_mgmt::ChildStatusReportRequest>();
}
StatusCollectorParams::~StatusCollectorParams() = default;

// Move only.
StatusCollectorParams::StatusCollectorParams(StatusCollectorParams&&) = default;
StatusCollectorParams& StatusCollectorParams::operator=(
    StatusCollectorParams&&) = default;

std::unique_ptr<DeviceLocalAccount>
StatusCollector::GetAutoLaunchedKioskSessionInfo() {
  std::unique_ptr<DeviceLocalAccount> account =
      GetCurrentKioskDeviceLocalAccount(chromeos::CrosSettings::Get());
  if (!account) {
    // No auto-launched kiosk session active.
    return nullptr;
  }

  chromeos::KioskAppManager::App current_app;
  bool regular_app_auto_launched_with_zero_delay =
      chromeos::KioskAppManager::Get()->GetApp(account->kiosk_app_id,
                                               &current_app) &&
      current_app.was_auto_launched_with_zero_delay;
  bool arc_app_auto_launched_with_zero_delay =
      chromeos::ArcKioskAppManager::Get()
          ->current_app_was_auto_launched_with_zero_delay();

  return regular_app_auto_launched_with_zero_delay ||
                 arc_app_auto_launched_with_zero_delay
             ? std::move(account)
             : nullptr;
}

}  // namespace policy
