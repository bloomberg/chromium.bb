// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_DEVICE_EXTERNAL_PRINTERS_SETTINGS_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_DEVICE_EXTERNAL_PRINTERS_SETTINGS_BRIDGE_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/printing/external_printers_policies.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"

namespace chromeos {

class ExternalPrinters;

// Observe device settings changes and propagate changes to ExternalPrinters.
class DeviceExternalPrintersSettingsBridge {
 public:
  DeviceExternalPrintersSettingsBridge(const ExternalPrinterPolicies& policies,
                                       CrosSettings* settings);
  ~DeviceExternalPrintersSettingsBridge();

 private:
  // Retrieve initial values for device settings.
  void Initialize();

  // Handle update for the access mode policy.
  void AccessModeUpdated();

  // Handle updates for the blacklist policy.
  void BlacklistUpdated();

  // Handle updates for the whitelist policy.
  void WhitelistUpdated();

  CrosSettings* settings_;
  const ExternalPrinterPolicies policies_;
  std::unique_ptr<CrosSettings::ObserverSubscription> access_mode_subscription_;
  std::unique_ptr<CrosSettings::ObserverSubscription> blacklist_subscription_;
  std::unique_ptr<CrosSettings::ObserverSubscription> whitelist_subscription_;

  DISALLOW_COPY_AND_ASSIGN(DeviceExternalPrintersSettingsBridge);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_DEVICE_EXTERNAL_PRINTERS_SETTINGS_BRIDGE_H_
