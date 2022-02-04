// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_system_tray_client.h"

namespace ash {

TestSystemTrayClient::TestSystemTrayClient() = default;

TestSystemTrayClient::~TestSystemTrayClient() = default;

void TestSystemTrayClient::ShowSettings(int64_t display_id) {}

void TestSystemTrayClient::ShowBluetoothSettings() {
  show_bluetooth_settings_count_++;
}

void TestSystemTrayClient::ShowBluetoothSettings(const std::string& device_id) {
  show_bluetooth_settings_count_++;
  last_bluetooth_settings_device_id_ = device_id;
}

void TestSystemTrayClient::ShowBluetoothPairingDialog(
    absl::optional<base::StringPiece> device_address) {
  show_bluetooth_pairing_dialog_count_++;
}

void TestSystemTrayClient::ShowDateSettings() {}

void TestSystemTrayClient::ShowSetTimeDialog() {}

void TestSystemTrayClient::ShowDisplaySettings() {}

void TestSystemTrayClient::ShowDarkModeSettings() {}

void TestSystemTrayClient::ShowStorageSettings() {}

void TestSystemTrayClient::ShowPowerSettings() {}

void TestSystemTrayClient::ShowChromeSlow() {}

void TestSystemTrayClient::ShowIMESettings() {}

void TestSystemTrayClient::ShowConnectedDevicesSettings() {
  show_connected_devices_settings_count_++;
}

void TestSystemTrayClient::ShowTetherNetworkSettings() {}

void TestSystemTrayClient::ShowWifiSyncSettings() {
  show_wifi_sync_settings_count_++;
}

void TestSystemTrayClient::ShowAboutChromeOS() {}

void TestSystemTrayClient::ShowHelp() {}

void TestSystemTrayClient::ShowAccessibilityHelp() {}

void TestSystemTrayClient::ShowAccessibilitySettings() {}

void TestSystemTrayClient::ShowGestureEducationHelp() {}

void TestSystemTrayClient::ShowPaletteHelp() {}

void TestSystemTrayClient::ShowPaletteSettings() {}

void TestSystemTrayClient::ShowPrivacyAndSecuritySettings() {
  show_os_settings_privacy_and_security_count_++;
}

void TestSystemTrayClient::ShowPublicAccountInfo() {}

void TestSystemTrayClient::ShowEnterpriseInfo() {}

void TestSystemTrayClient::ShowNetworkConfigure(const std::string& network_id) {
}

void TestSystemTrayClient::ShowNetworkCreate(const std::string& type) {}

void TestSystemTrayClient::ShowSettingsCellularSetup(bool show_psim_flow) {}

void TestSystemTrayClient::ShowSettingsSimUnlock() {
  ++show_sim_unlock_settings_count_;
}

void TestSystemTrayClient::ShowThirdPartyVpnCreate(
    const std::string& extension_id) {}

void TestSystemTrayClient::ShowArcVpnCreate(const std::string& app_id) {}

void TestSystemTrayClient::ShowNetworkSettings(const std::string& network_id) {}

void TestSystemTrayClient::ShowMultiDeviceSetup() {
  show_multi_device_setup_count_++;
}

void TestSystemTrayClient::ShowFirmwareUpdate() {
  show_firmware_update_count_++;
}

void TestSystemTrayClient::RequestRestartForUpdate() {}

void TestSystemTrayClient::SetLocaleAndExit(
    const std::string& locale_iso_code) {}

void TestSystemTrayClient::ShowAccessCodeCastingDialog() {}

}  // namespace ash
