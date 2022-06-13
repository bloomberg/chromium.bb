// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_DESKTOP_SHARING_HUB_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_DESKTOP_SHARING_HUB_COMPONENT_INSTALLER_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

class DesktopSharingHubComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  DesktopSharingHubComponentInstallerPolicy() = default;
  DesktopSharingHubComponentInstallerPolicy(
      const DesktopSharingHubComponentInstallerPolicy&) = delete;
  DesktopSharingHubComponentInstallerPolicy& operator=(
      const DesktopSharingHubComponentInstallerPolicy&) = delete;
  ~DesktopSharingHubComponentInstallerPolicy() override = default;

 private:
  // The following methods override ComponentInstallerPolicy.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::Value& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  static base::FilePath GetInstalledPath(const base::FilePath& base);
};

// Call once during startup to make the component update service aware of
// the File Type Policies component.
void RegisterDesktopSharingHubComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_DESKTOP_SHARING_HUB_COMPONENT_INSTALLER_H_
