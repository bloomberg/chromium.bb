// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CROWD_DENY_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CROWD_DENY_COMPONENT_INSTALLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

class CrowdDenyComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  CrowdDenyComponentInstallerPolicy() = default;
  ~CrowdDenyComponentInstallerPolicy() override = default;

 private:
  static base::FilePath GetInstalledPath(const base::FilePath& base);

  // ComponentInstallerPolicy:
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  std::vector<std::string> GetMimeTypes() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  DISALLOW_COPY_AND_ASSIGN(CrowdDenyComponentInstallerPolicy);
};

// Call once during startup to make the component update service aware of the
// Crowd Deny component.
void RegisterCrowdDenyComponent(ComponentUpdateService* cus,
                                const base::FilePath& user_data_dir);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CROWD_DENY_COMPONENT_INSTALLER_H_
