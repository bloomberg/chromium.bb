// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/media_foundation_widevine_cdm_component_installer.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/version.h"
#include "base/win/security_util.h"
#include "base/win/sid.h"
#include "build/build_config.h"
#include "components/component_updater/component_updater_paths.h"
#include "content/public/browser/cdm_registry.h"
#include "content/public/common/cdm_info.h"
#include "media/base/win/mf_feature_checks.h"
#include "media/cdm/win/media_foundation_cdm.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: neifaoindggfcjicffkgpmnlppeffabd
const uint8_t kMediaFoundationWidevineCdmPublicKeySHA256[32] = {
    0xd4, 0x85, 0x0e, 0x8d, 0x36, 0x65, 0x29, 0x82, 0x55, 0xa6, 0xfc,
    0xdb, 0xff, 0x45, 0x50, 0x13, 0x5f, 0xed, 0x02, 0x65, 0xb5, 0x19,
    0x8e, 0xa0, 0x5b, 0x38, 0x38, 0xc9, 0x32, 0x3a, 0x9f, 0xf1};

base::FilePath GetCdmPath(const base::FilePath& install_dir) {
  return install_dir.AppendASCII(
      base::GetNativeLibraryName(kMediaFoundationWidevineCdmLibraryName));
}

// Name of the Widevine CDM architecture to avoid registering the wrong CDM.
const char kWidevineCdmArch[] =
#if defined(ARCH_CPU_X86)
    "x86";
#elif defined(ARCH_CPU_X86_64)
    "x64";
#elif defined(ARCH_CPU_ARMEL)
    "arm";
#elif defined(ARCH_CPU_ARM64)
    "arm64";
#else
#error This file should only be included for supported architecture.
#endif

}  // namespace

namespace component_updater {

bool MediaFoundationWidevineCdmComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool MediaFoundationWidevineCdmComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  return false;
}

// Set permission on `install_dir` so the CDM can be loaded in the LPAC process.
update_client::CrxInstaller::Result
MediaFoundationWidevineCdmComponentInstallerPolicy::OnCustomInstall(
    const base::Value& manifest,
    const base::FilePath& install_dir) {
  DVLOG(1) << __func__ << ": Set permission on " << install_dir;

  auto sids =
      base::win::Sid::FromNamedCapabilityVector({L"mediaFoundationCdmFiles"});

  bool success = false;
  if (sids) {
    success = base::win::GrantAccessToPath(
        install_dir, *sids, FILE_GENERIC_READ | FILE_GENERIC_EXECUTE,
        CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE);
  }

  return update_client::CrxInstaller::Result(
      success ? update_client::InstallError::NONE
              : update_client::InstallError::SET_PERMISSIONS_FAILED);
}

void MediaFoundationWidevineCdmComponentInstallerPolicy::OnCustomUninstall() {}

void MediaFoundationWidevineCdmComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  VLOG(1) << "Register Media Foundation Widevine CDM";
  content::CdmInfo cdm_info(
      kWidevineKeySystem, content::CdmInfo::Robustness::kHardwareSecure,
      /*capability=*/absl::nullopt, /*supports_sub_key_systems=*/false,
      kMediaFoundationWidevineCdmDisplayName, kMediaFoundationWidevineCdmType,
      version, GetCdmPath(install_dir));
  content::CdmRegistry::GetInstance()->RegisterCdm(cdm_info);
}

// Called during startup and installation before ComponentReady().
bool MediaFoundationWidevineCdmComponentInstallerPolicy::VerifyInstallation(
    const base::Value& manifest,
    const base::FilePath& install_dir) const {
  // TODO(crbug.com/1225681) Compare manifest version and DLL's version.
  return base::PathExists(GetCdmPath(install_dir));
}

// The relative install directory looks like:
// <user-data-dir>\MediaFoundationWidevineCdm\<arch>.
base::FilePath
MediaFoundationWidevineCdmComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath::FromUTF8Unsafe(
             kMediaFoundationWidevineCdmBaseDirection)
      .AppendASCII(kWidevineCdmArch);
}

void MediaFoundationWidevineCdmComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kMediaFoundationWidevineCdmPublicKeySHA256),
               std::end(kMediaFoundationWidevineCdmPublicKeySHA256));
}

std::string MediaFoundationWidevineCdmComponentInstallerPolicy::GetName()
    const {
  return kMediaFoundationWidevineCdmDisplayName;
}

update_client::InstallerAttributes
MediaFoundationWidevineCdmComponentInstallerPolicy::GetInstallerAttributes()
    const {
  return update_client::InstallerAttributes();
}

void RegisterMediaFoundationWidevineCdmComponent(
    component_updater::ComponentUpdateService* cus) {
  if (media::SupportMediaFoundationEncryptedPlayback()) {
    VLOG(1) << "Registering Media Foundation Widevine CDM component.";
    auto installer = base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<MediaFoundationWidevineCdmComponentInstallerPolicy>());
    installer->Register(cus, base::NullCallback());
  }
}

}  // namespace component_updater
