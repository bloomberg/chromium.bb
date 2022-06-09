// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_connect_delegate_chromeos.h"

#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ui/ash/network/enrollment_dialog_view.h"
#include "chrome/browser/ui/ash/network/network_state_notifier.h"
#include "chrome/browser/ui/ash/system_tray_client_impl.h"
#include "chrome/browser/ui/webui/chromeos/cellular_setup/mobile_setup_dialog.h"

namespace {

bool IsUIAvailable() {
  // UI is available when screen is unlocked.
  return !ash::ScreenLocker::default_screen_locker() ||
         !ash::ScreenLocker::default_screen_locker()->locked();
}

}  // namespace

NetworkConnectDelegateChromeOS::NetworkConnectDelegateChromeOS()
    : network_state_notifier_(new chromeos::NetworkStateNotifier()) {}

NetworkConnectDelegateChromeOS::~NetworkConnectDelegateChromeOS() {}

void NetworkConnectDelegateChromeOS::ShowNetworkConfigure(
    const std::string& network_id) {
  if (!IsUIAvailable())
    return;
  SystemTrayClientImpl::Get()->ShowNetworkConfigure(network_id);
}

void NetworkConnectDelegateChromeOS::ShowNetworkSettings(
    const std::string& network_id) {
  if (!IsUIAvailable())
    return;
  SystemTrayClientImpl::Get()->ShowNetworkSettings(network_id);
}

bool NetworkConnectDelegateChromeOS::ShowEnrollNetwork(
    const std::string& network_id) {
  if (!IsUIAvailable())
    return false;
  return chromeos::enrollment::CreateEnrollmentDialog(network_id);
}

void NetworkConnectDelegateChromeOS::ShowMobileSetupDialog(
    const std::string& network_id) {
  if (!IsUIAvailable())
    return;
  SystemTrayClientImpl::Get()->ShowSettingsCellularSetup(
      /*show_psim_flow=*/true);
}

void NetworkConnectDelegateChromeOS::ShowCarrierAccountDetail(
    const std::string& network_id) {
  if (!IsUIAvailable())
    return;
  chromeos::cellular_setup::MobileSetupDialog::ShowByNetworkId(network_id);
}

void NetworkConnectDelegateChromeOS::ShowNetworkConnectError(
    const std::string& error_name,
    const std::string& network_id) {
  network_state_notifier_->ShowNetworkConnectErrorForGuid(error_name,
                                                          network_id);
}

void NetworkConnectDelegateChromeOS::ShowMobileActivationError(
    const std::string& network_id) {
  network_state_notifier_->ShowMobileActivationErrorForGuid(network_id);
}

void NetworkConnectDelegateChromeOS::SetSystemTrayClient(
    ash::SystemTrayClient* system_tray_client) {
  network_state_notifier_->set_system_tray_client(system_tray_client);
}
