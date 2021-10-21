// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TRAY_CLIENT_H_
#define ASH_PUBLIC_CPP_SYSTEM_TRAY_CLIENT_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Handles method calls delegated back to chrome from ash.
class ASH_PUBLIC_EXPORT SystemTrayClient {
 public:
  virtual ~SystemTrayClient() {}

  // Shows general settings UI.
  virtual void ShowSettings(int64_t display_id) = 0;

  // Shows settings related to Bluetooth devices (e.g. to add a device).
  virtual void ShowBluetoothSettings() = 0;

  // Shows the detailed settings for the Bluetooth device with ID |device_id|.
  virtual void ShowBluetoothSettings(const std::string& device_id) = 0;

  // Show the Bluetooth pairing dialog. When provided, |device_address| is the
  // unique device address that the dialog should attempt to pair with and
  // should be in the form "XX:XX:XX:XX:XX:XX". When |device_address| is not
  // provided the dialog will show the device list instead.
  virtual void ShowBluetoothPairingDialog(
      absl::optional<base::StringPiece> device_address) = 0;

  // Shows the settings related to date, timezone etc.
  virtual void ShowDateSettings() = 0;

  // Shows the dialog to set system time, date, and timezone.
  virtual void ShowSetTimeDialog() = 0;

  // Shows settings related to multiple displays.
  virtual void ShowDisplaySettings() = 0;

  // Shows settings related to power.
  virtual void ShowPowerSettings() = 0;

  // Shows OS settings related to privacy and security.
  virtual void ShowPrivacyAndSecuritySettings() = 0;

  // Shows the page that lets you disable performance tracing.
  virtual void ShowChromeSlow() = 0;

  // Shows settings related to input methods.
  virtual void ShowIMESettings() = 0;

  // Shows settings related to MultiDevice features.
  virtual void ShowConnectedDevicesSettings() = 0;

  // Shows settings related to tether network.
  virtual void ShowTetherNetworkSettings() = 0;

  // Shows settings related to Wi-Fi Sync v2.
  virtual void ShowWifiSyncSettings() = 0;

  // Shows the about chrome OS page and checks for updates after the page is
  // loaded.
  virtual void ShowAboutChromeOS() = 0;

  // Shows the Chromebook help app.
  virtual void ShowHelp() = 0;

  // Shows accessibility help.
  virtual void ShowAccessibilityHelp() = 0;

  // Shows the settings related to accessibility.
  virtual void ShowAccessibilitySettings() = 0;

  // Shows gesture education help.
  virtual void ShowGestureEducationHelp() = 0;

  // Shows the help center article for the stylus tool palette.
  virtual void ShowPaletteHelp() = 0;

  // Shows the settings related to the stylus tool palette.
  virtual void ShowPaletteSettings() = 0;

  // Shows information about public account mode.
  virtual void ShowPublicAccountInfo() = 0;

  // Shows information about enterprise enrolled devices.
  virtual void ShowEnterpriseInfo() = 0;

  // Shows UI to configure or activate the network specified by |network_id|,
  // which may include showing payment or captive portal UI when appropriate.
  virtual void ShowNetworkConfigure(const std::string& network_id) = 0;

  // Shows UI to create a new network connection. |type| is the ONC network type
  // (see onc_spec.md). TODO(stevenjb): Use NetworkType from onc.mojo (TBD).
  virtual void ShowNetworkCreate(const std::string& type) = 0;

  // Opens the cellular setup flow in OS Settings. |show_psim_flow| indicates
  // if we should navigate to the physical SIM setup flow or to the page that
  // allows the user to select which flow they wish to enter (pSIM or eSIM).
  virtual void ShowSettingsCellularSetup(bool show_psim_flow) = 0;

  // Opens SIM unlock dialog in OS Settings.
  virtual void ShowSettingsSimUnlock() = 0;

  // Shows the "add network" UI to create a third-party extension-backed VPN
  // connection (e.g. Cisco AnyConnect).
  virtual void ShowThirdPartyVpnCreate(const std::string& extension_id) = 0;

  // Launches Arc VPN provider.
  virtual void ShowArcVpnCreate(const std::string& app_id) = 0;

  // Shows settings related to networking. If |network_id| is empty, shows
  // general settings. Otherwise shows settings for the individual network.
  // On devices |network_id| is a GUID, but on Linux desktop and in tests it can
  // be any string.
  virtual void ShowNetworkSettings(const std::string& network_id) = 0;

  // Shows the MultiDevice setup flow dialog.
  virtual void ShowMultiDeviceSetup() = 0;

  // Attempts to restart the system for update.
  virtual void RequestRestartForUpdate() = 0;

  // Sets the UI locale to |locale_iso_code| and exit the session to take
  // effect.
  virtual void SetLocaleAndExit(const std::string& locale_iso_code) = 0;

 protected:
  SystemTrayClient() {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TRAY_CLIENT_H_
