// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CROW_DOMAIN_LIST_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CROW_DOMAIN_LIST_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
class Value;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

class CrowDomainListComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  CrowDomainListComponentInstallerPolicy() = default;
  CrowDomainListComponentInstallerPolicy(
      const CrowDomainListComponentInstallerPolicy&) = delete;
  CrowDomainListComponentInstallerPolicy& operator=(
      const CrowDomainListComponentInstallerPolicy&) = delete;
  ~CrowDomainListComponentInstallerPolicy() override = default;

 private:
  // ComponentInstallerPolicy:
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
// the Crow Domains List configuration component.
void RegisterCrowDomainListComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CROW_DOMAIN_LIST_COMPONENT_INSTALLER_H_
