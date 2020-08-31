// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/smart_dim_component_installer.h"

#include <cstddef>
#include <tuple>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/version.h"
#include "chrome/browser/chromeos/power/ml/smart_dim/download_worker.h"
#include "chrome/browser/chromeos/power/ml/smart_dim/metrics.h"
#include "chrome/browser/chromeos/power/ml/smart_dim/ml_agent.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/component_updater/component_updater_service.h"
#include "content/public/browser/browser_thread.h"

namespace {

using ::chromeos::power::ml::ComponentFileContents;
using ::chromeos::power::ml::ComponentVersionType;
using ::chromeos::power::ml::LoadComponentEvent;
using ::chromeos::power::ml::LogComponentVersionType;
using ::chromeos::power::ml::LogLoadComponentEvent;

const base::FilePath::CharType kSmartDimFeaturePreprocessorConfigFileName[] =
    FILE_PATH_LITERAL("example_preprocessor_config.pb");
const base::FilePath::CharType kSmartDimModelFileName[] =
    FILE_PATH_LITERAL("mlservice-model-smart_dim.tflite");
const base::FilePath::CharType kSmartDimMetaJsonFileName[] =
    FILE_PATH_LITERAL("smart_dim_meta.json");

const char kDefaultVersion[] = "2019.11.12.0";

constexpr base::FeatureParam<std::string> kVersion{
    &chromeos::features::kSmartDimExperimentalComponent,
    "smart_dim_experimental_version", kDefaultVersion};

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: ghiclnejioiofblmbphpgbhaojnkempa
const uint8_t kSmartDimPublicKeySHA256[32] = {
    0x67, 0x82, 0xbd, 0x49, 0x8e, 0x8e, 0x51, 0xbc, 0x1f, 0x7f, 0x61,
    0x70, 0xe9, 0xda, 0x4c, 0xf0, 0x30, 0x2e, 0x24, 0x4d, 0x68, 0x17,
    0x19, 0xad, 0x26, 0x6e, 0xd0, 0x33, 0x03, 0xb3, 0xe5, 0xff};

const char kMLSmartDimManifestName[] = "Smart Dim";


// Read files from the component to strings, should be called from a blocking
// task runner.
base::Optional<ComponentFileContents> ReadComponentFiles(
    const base::FilePath& meta_json_path,
    const base::FilePath& preprocessor_pb_path,
    const base::FilePath& model_path) {
  std::string metadata_json, preprocessor_proto, model_flatbuffer;
  if (!base::ReadFileToString(meta_json_path, &metadata_json) ||
      !base::ReadFileToString(preprocessor_pb_path, &preprocessor_proto) ||
      !base::ReadFileToString(model_path, &model_flatbuffer)) {
    DLOG(ERROR) << "Failed reading component files.";
    return base::nullopt;
  }

  return std::make_tuple(std::move(metadata_json),
                         std::move(preprocessor_proto),
                         std::move(model_flatbuffer));
}

void UpdateSmartDimMlAgent(
    const base::Optional<ComponentFileContents>& result) {
  if (result == base::nullopt) {
    LogLoadComponentEvent(LoadComponentEvent::kReadComponentFilesError);
    return;
  }

  chromeos::power::ml::SmartDimMlAgent::GetInstance()->OnComponentReady(
      result.value());
}

}  // namespace

namespace component_updater {

SmartDimComponentInstallerPolicy::SmartDimComponentInstallerPolicy(
    std::string expected_version)
    : expected_version_(expected_version) {}

SmartDimComponentInstallerPolicy::~SmartDimComponentInstallerPolicy() = default;

bool SmartDimComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool SmartDimComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
SmartDimComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void SmartDimComponentInstallerPolicy::OnCustomUninstall() {}

void SmartDimComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If IsDownloadWorkerReady(), newly downloaded components will take effect
  // on next reboot. This makes sure the updating happens at most once.
  if (chromeos::power::ml::SmartDimMlAgent::GetInstance()
          ->IsDownloadWorkerReady()) {
    DVLOG(1) << "Download_worker in SmartDimMlAgent is ready, does nothing.";
    return;
  }

  DCHECK(!install_dir.empty());
  DVLOG(1) << "Component ready, version " << version.GetString() << " in "
           << install_dir.value();

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          &ReadComponentFiles, install_dir.Append(kSmartDimMetaJsonFileName),
          install_dir.Append(kSmartDimFeaturePreprocessorConfigFileName),
          install_dir.Append(kSmartDimModelFileName)),
      base::BindOnce(&UpdateSmartDimMlAgent));
}

// Called during startup and installation before ComponentReady().
bool SmartDimComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  // Get component version from manifest and compare to the expected_version_.
  // Note: versions should not be treated as simple strings, for example,
  // base::Version("2020.02.06") == base::Version("2020.2.6").
  const auto* version_value = manifest.FindKey("version");
  DCHECK(version_value);
  const base::Version component_version(version_value->GetString());
  const base::Version expected_version(expected_version_);
  if (component_version != expected_version) {
    DVLOG(1) << "Version " << component_version
             << " doesn't match expected_version " << expected_version;
    return false;
  }
  // No need to actually validate the pb and tflite files here, since we'll do
  // the checking in UpdateSmartDimMlAgent.
  return base::PathExists(
             install_dir.Append(kSmartDimFeaturePreprocessorConfigFileName)) &&
         base::PathExists(install_dir.Append(kSmartDimModelFileName)) &&
         base::PathExists(install_dir.Append(kSmartDimMetaJsonFileName));
}

base::FilePath SmartDimComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("SmartDim"));
}

void SmartDimComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  DCHECK(hash);
  hash->assign(kSmartDimPublicKeySHA256,
               kSmartDimPublicKeySHA256 + base::size(kSmartDimPublicKeySHA256));
}

std::string SmartDimComponentInstallerPolicy::GetName() const {
  return kMLSmartDimManifestName;
}

update_client::InstallerAttributes
SmartDimComponentInstallerPolicy::GetInstallerAttributes() const {
  update_client::InstallerAttributes attrs;
  // Append a '$' for exact matching.
  attrs["targetversionprefix"] = expected_version_ + "$";
  return attrs;
}

std::vector<std::string> SmartDimComponentInstallerPolicy::GetMimeTypes()
    const {
  return std::vector<std::string>();
}

void RegisterSmartDimComponent(ComponentUpdateService* cus) {
  if (!base::FeatureList::IsEnabled(chromeos::features::kSmartDimNewMlAgent))
    return;

  DVLOG(1) << "Registering smart dim component.";
  const std::string expected_version = kVersion.Get();

  if (expected_version.empty()) {
    LogComponentVersionType(ComponentVersionType::kEmpty);
    DLOG(ERROR) << "expected_version is empty.";
    return;
  }

  if (expected_version == kDefaultVersion)
    LogComponentVersionType(ComponentVersionType::kDefault);
  else
    LogComponentVersionType(ComponentVersionType::kExperimental);

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<SmartDimComponentInstallerPolicy>(expected_version));
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
