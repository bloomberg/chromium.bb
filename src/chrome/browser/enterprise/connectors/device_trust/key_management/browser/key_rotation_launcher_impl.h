// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_ROTATION_LAUNCHER_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_ROTATION_LAUNCHER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"

namespace policy {
class BrowserDMTokenStorage;
class DeviceManagementService;
}  // namespace policy

namespace enterprise_connectors {

class KeyRotationLauncherImpl : public KeyRotationLauncher {
 public:
  KeyRotationLauncherImpl(
      policy::BrowserDMTokenStorage* dm_token_storage,
      policy::DeviceManagementService* device_management_service);
  ~KeyRotationLauncherImpl() override;

  // KeyRotationLauncher:
  void LaunchKeyRotation(const std::string& nonce,
                         KeyRotationCommand::Callback callback) override;

 private:
  raw_ptr<policy::BrowserDMTokenStorage> dm_token_storage_;
  raw_ptr<policy::DeviceManagementService> device_management_service_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_KEY_ROTATION_LAUNCHER_IMPL_H_
