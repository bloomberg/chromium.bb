// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/recovery_improved_component_installer.h"

#include "build/branding_buildflags.h"

// The recovery component is built and used by Google Chrome only.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

#include <iterator>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/component_updater/component_updater_utils.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/update_client/patcher.h"
#include "components/update_client/unzip/unzip_impl.h"

namespace component_updater {

constexpr base::TaskTraits
    RecoveryComponentActionHandler::kThreadPoolTaskTraits;
constexpr base::TaskTraits
    RecoveryComponentActionHandler::kThreadPoolTaskTraitsRunCommand;

RecoveryComponentActionHandler::RecoveryComponentActionHandler(
    const std::vector<uint8_t>& key_hash,
    crx_file::VerifierFormat verifier_format)
    : key_hash_(key_hash), verifier_format_(verifier_format) {}

RecoveryComponentActionHandler::~RecoveryComponentActionHandler() = default;

void RecoveryComponentActionHandler::Handle(const base::FilePath& action,
                                            const std::string& session_id,
                                            Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  crx_path_ = action;
  session_id_ = session_id;
  callback_ = std::move(callback);

  base::ThreadPool::CreateSequencedTaskRunner(kThreadPoolTaskTraits)
      ->PostTask(
          FROM_HERE,
          component_updater::IsPerUserInstall()
              ? base::BindOnce(&RecoveryComponentActionHandler::Unpack, this)
              : base::BindOnce(&RecoveryComponentActionHandler::Elevate, this,
                               std::move(callback_)));
}

void RecoveryComponentActionHandler::Unpack() {
  auto unzipper = base::MakeRefCounted<update_client::UnzipChromiumFactory>(
                      base::BindRepeating(&unzip::LaunchUnzipper))
                      ->Create();
  auto unpacker = base::MakeRefCounted<update_client::ComponentUnpacker>(
      key_hash_, crx_path_, nullptr, std::move(unzipper), nullptr,
      verifier_format_);
  unpacker->Unpack(
      base::BindOnce(&RecoveryComponentActionHandler::UnpackComplete, this));
}

void RecoveryComponentActionHandler::UnpackComplete(
    const update_client::ComponentUnpacker::Result& result) {
  if (result.error != update_client::UnpackerError::kNone) {
    DCHECK(!base::DirectoryExists(result.unpack_path));
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), false,
                       static_cast<int>(result.error), result.extended_error));
    return;
  }

  unpack_path_ = result.unpack_path;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&RecoveryComponentActionHandler::RunCommand,
                                this, MakeCommandLine(result.unpack_path)));
}

void RecoveryComponentActionHandler::RunCommand(
    const base::CommandLine& cmdline) {
  VLOG(1) << "run command: " << cmdline.GetCommandLineString();
  base::LaunchOptions options;
#if defined(OS_WIN)
  options.start_hidden = true;
#endif
  base::Process process = base::LaunchProcess(cmdline, options);
  base::ThreadPool::PostTask(
      FROM_HERE, kThreadPoolTaskTraitsRunCommand,
      base::BindOnce(&RecoveryComponentActionHandler::WaitForCommand, this,
                     std::move(process)));
}

void RecoveryComponentActionHandler::WaitForCommand(base::Process process) {
  int exit_code = 0;
  const base::TimeDelta kMaxWaitTime = base::TimeDelta::FromSeconds(600);
  const bool succeeded =
      process.WaitForExitWithTimeout(kMaxWaitTime, &exit_code);
  base::DeleteFileRecursively(unpack_path_);
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), succeeded, exit_code, 0));
}

// The SHA256 of the SubjectPublicKeyInfo used to sign the component CRX.
// The component id is: ihnlcenocehgdaegdmhbidjhnhdchfmm
constexpr uint8_t kRecoveryImprovedPublicKeySHA256[32] = {
    0x87, 0xdb, 0x24, 0xde, 0x24, 0x76, 0x30, 0x46, 0x3c, 0x71, 0x83,
    0x97, 0xd7, 0x32, 0x75, 0xcc, 0xd5, 0x7f, 0xec, 0x09, 0x60, 0x6d,
    0x20, 0xc3, 0x81, 0xd7, 0xce, 0x7b, 0x10, 0x15, 0x44, 0xd1};

bool RecoveryImprovedInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool RecoveryImprovedInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
RecoveryImprovedInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);
}

void RecoveryImprovedInstallerPolicy::OnCustomUninstall() {}

void RecoveryImprovedInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DVLOG(1) << "RecoveryImproved component is ready.";
}

// Called during startup and installation before ComponentReady().
bool RecoveryImprovedInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return true;
}

base::FilePath RecoveryImprovedInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("RecoveryImproved"));
}

void RecoveryImprovedInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kRecoveryImprovedPublicKeySHA256),
               std::end(kRecoveryImprovedPublicKeySHA256));
}

std::string RecoveryImprovedInstallerPolicy::GetName() const {
  return "Chrome Improved Recovery";
}

update_client::InstallerAttributes
RecoveryImprovedInstallerPolicy::GetInstallerAttributes() const {
  return {};
}

std::vector<std::string> RecoveryImprovedInstallerPolicy::GetMimeTypes() const {
  return {};
}

void RegisterRecoveryImprovedComponent(ComponentUpdateService* cus,
                                       PrefService* prefs) {
// TODO(sorin): enable recovery component for macOS. crbug/687231.
#if defined(OS_WIN)
  DVLOG(1) << "Registering RecoveryImproved component.";

  // |cus| keeps a reference to the |installer| in the CrxComponent instance.
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<RecoveryImprovedInstallerPolicy>(prefs),
      RecoveryComponentActionHandler::MakeActionHandler());
  installer->Register(cus, base::OnceClosure());
#endif
}

}  // namespace component_updater

#else
namespace component_updater {
void RegisterRecoveryImprovedComponent(ComponentUpdateService* cus,
                                       PrefService* prefs) {}
}  // namespace component_updater
#endif  // GOOGLE_CHROME_BRANDING
