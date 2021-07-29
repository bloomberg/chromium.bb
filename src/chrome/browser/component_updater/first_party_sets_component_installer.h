// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_FIRST_PARTY_SETS_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_FIRST_PARTY_SETS_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

class FirstPartySetsComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  // |on_sets_ready| will be called on the UI thread when the sets are ready. It
  // is exposed here for testing.
  explicit FirstPartySetsComponentInstallerPolicy(
      base::RepeatingCallback<void(const std::string&)> on_sets_ready);
  ~FirstPartySetsComponentInstallerPolicy() override;

  FirstPartySetsComponentInstallerPolicy(
      const FirstPartySetsComponentInstallerPolicy&) = delete;
  FirstPartySetsComponentInstallerPolicy operator=(
      const FirstPartySetsComponentInstallerPolicy&) = delete;

  // Calls the callback with the current First-Party Sets data, if the data
  // exists and can be read.
  static void ReconfigureAfterNetworkRestart(
      const base::RepeatingCallback<void(const std::string&)>&);

  static const char kDogfoodInstallerAttributeName[];

 private:
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsComponentInstallerTest,
                           LoadsSets_OnComponentReady);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsComponentInstallerTest,
                           LoadsSets_OnNetworkRestart);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsComponentInstallerTest,
                           IgnoreNewSets_OnComponentReady);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsComponentInstallerTest,
                           IgnoreNewSets_OnNetworkRestart);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsComponentInstallerTest,
                           GetInstallerAttributes_Disabled);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsComponentInstallerTest,
                           GetInstallerAttributes_NonDogfooder);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsComponentInstallerTest,
                           GetInstallerAttributes_Dogfooder);

  // The following methods override ComponentInstallerPolicy.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  static base::FilePath GetInstalledPath(const base::FilePath& base);

  base::RepeatingCallback<void(const std::string&)> on_sets_ready_;

  // `component_installed_` indicates whether ComponentReady has been called
  // once after registration. After the first call, ComponentReady will be no-op
  // for new versions delivered from Component Updater, i.e. new components will
  // be installed (kept on-disk) but not propagated to the NetworkService until
  // next browser startup.
  bool component_installed_ = false;
};

// Call once during startup to make the component update service aware of
// the First-Party Sets component.
void RegisterFirstPartySetsComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_FIRST_PARTY_SETS_COMPONENT_INSTALLER_H_
