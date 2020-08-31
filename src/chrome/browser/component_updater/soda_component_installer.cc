// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/soda_component_installer.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/soda/constants.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_task_traits.h"
#include "crypto/sha2.h"

using content::BrowserThread;

namespace component_updater {

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component id is: icnkogojpkfjeajonkmlplionaamopkf
const uint8_t kSODAPublicKeySHA256[32] = {
    0x82, 0xda, 0xe6, 0xe9, 0xfa, 0x59, 0x40, 0x9e, 0xda, 0xcb, 0xfb,
    0x8e, 0xd0, 0x0c, 0xef, 0xa5, 0xc0, 0x97, 0x00, 0x84, 0x1c, 0x21,
    0xa6, 0xae, 0xc8, 0x1b, 0x87, 0xfb, 0x12, 0x27, 0x28, 0xb1};

static_assert(base::size(kSODAPublicKeySHA256) == crypto::kSHA256Length,
              "Wrong hash length");

const char kSODAManifestName[] = "SODA Library";

}  // namespace

SODAComponentInstallerPolicy::SODAComponentInstallerPolicy(
    const OnSODAComponentReadyCallback& callback)
    : on_component_ready_callback_(callback) {}

SODAComponentInstallerPolicy::~SODAComponentInstallerPolicy() = default;

const std::string SODAComponentInstallerPolicy::GetExtensionId() {
  return crx_file::id_util::GenerateIdFromHash(kSODAPublicKeySHA256,
                                               sizeof(kSODAPublicKeySHA256));
}

void SODAComponentInstallerPolicy::UpdateSODAComponentOnDemand() {
  const std::string crx_id =
      component_updater::SODAComponentInstallerPolicy::GetExtensionId();
  g_browser_process->component_updater()->GetOnDemandUpdater().OnDemandUpdate(
      crx_id, component_updater::OnDemandUpdater::Priority::FOREGROUND,
      base::BindOnce([](update_client::Error error) {
        if (error != update_client::Error::NONE &&
            error != update_client::Error::UPDATE_IN_PROGRESS)
          LOG(ERROR) << "On demand update of the SODA component failed "
                        "with error: "
                     << static_cast<int>(error);
      }));
}

bool SODAComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir.Append(speech::kSodaBinaryRelativePath));
}

bool SODAComponentInstallerPolicy::SupportsGroupPolicyEnabledComponentUpdates()
    const {
  return true;
}

bool SODAComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
SODAComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void SODAComponentInstallerPolicy::OnCustomUninstall() {}

void SODAComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  on_component_ready_callback_.Run(install_dir);
}

base::FilePath SODAComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(speech::kSodaInstallationRelativePath);
}

void SODAComponentInstallerPolicy::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(kSODAPublicKeySHA256,
               kSODAPublicKeySHA256 + base::size(kSODAPublicKeySHA256));
}

std::string SODAComponentInstallerPolicy::GetName() const {
  return kSODAManifestName;
}

update_client::InstallerAttributes
SODAComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> SODAComponentInstallerPolicy::GetMimeTypes() const {
  return std::vector<std::string>();
}

void UpdateSODAInstallDirPref(PrefService* prefs,
                              const base::FilePath& install_dir) {
#if !defined(OS_ANDROID)
  prefs->SetFilePath(prefs::kSODAPath,
                     install_dir.Append(speech::kSodaBinaryRelativePath));
#endif
}

void RegisterSODAComponent(ComponentUpdateService* cus,
                           PrefService* prefs,
                           base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<SODAComponentInstallerPolicy>(base::BindRepeating(
          [](PrefService* prefs, const base::FilePath& install_dir) {
            base::PostTask(
                FROM_HERE, {BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
                base::BindOnce(&UpdateSODAInstallDirPref, prefs, install_dir));
          },
          prefs)));
  installer->Register(cus, std::move(callback));
}
}  // namespace component_updater
