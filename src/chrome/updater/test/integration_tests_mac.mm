// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants_builder.h"
#include "chrome/updater/launchd_util.h"
#import "chrome/updater/mac/mac_util.h"
#include "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "components/crx_file/crx_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace updater {
namespace test {
namespace {

Launchd::Domain LaunchdDomain(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kSystem:
      return Launchd::Domain::Local;
    case UpdaterScope::kUser:
      return Launchd::Domain::User;
  }
}

Launchd::Type LaunchdType(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kSystem:
      return Launchd::Type::Daemon;
    case UpdaterScope::kUser:
      return Launchd::Type::Agent;
  }
}

base::FilePath GetExecutablePath() {
  base::FilePath test_executable;
  if (!base::PathService::Get(base::FILE_EXE, &test_executable))
    return base::FilePath();
  return test_executable.DirName().Append(GetExecutableRelativePath());
}

absl::optional<base::FilePath> GetProductPath(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetLibraryFolderPath(scope);
  if (!path)
    return absl::nullopt;

  return path->AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

absl::optional<base::FilePath> GetActiveFile(UpdaterScope /*scope*/,
                                             const std::string& id) {
  // The active user is always managaged in the updater scope for the user.
  const absl::optional<base::FilePath> path =
      GetLibraryFolderPath(UpdaterScope::kUser);
  if (!path)
    return absl::nullopt;

  return path->AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(COMPANY_SHORTNAME_STRING "SoftwareUpdate")
      .AppendASCII("Actives")
      .AppendASCII(id);
}

void ExpectServiceAbsent(UpdaterScope scope, const std::string& service) {
  VLOG(0) << __func__ << " - scope: " << scope << ". service: " << service;
  bool success = false;
  base::RunLoop loop;
  PollLaunchctlList(scope, service, LaunchctlPresence::kAbsent,
                    base::Seconds(7),
                    base::BindLambdaForTesting([&](bool result) {
                      success = result;
                      loop.QuitClosure().Run();
                    }));
  loop.Run();
  EXPECT_TRUE(success) << service << " is unexpectedly present.";
}

}  // namespace

base::FilePath GetSetupExecutablePath() {
  // There is no metainstaller on mac, use the main executable for setup.
  return GetExecutablePath();
}

void EnterTestMode(const GURL& url) {
  ASSERT_TRUE(ExternalConstantsBuilder()
                  .SetUpdateURL(std::vector<std::string>{url.spec()})
                  .SetUseCUP(false)
                  .SetInitialDelay(0.1)
                  .SetServerKeepAliveSeconds(1)
                  .SetCrxVerifierFormat(crx_file::VerifierFormat::CRX3)
                  .Overwrite());
}

absl::optional<base::FilePath> GetDataDirPath(UpdaterScope scope) {
  absl::optional<base::FilePath> app_path =
      GetApplicationSupportDirectory(scope);
  if (!app_path) {
    VLOG(1) << "Failed to get Application support path.";
    return absl::nullopt;
  }

  return app_path->AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

void Clean(UpdaterScope scope) {
  Launchd::Domain launchd_domain = LaunchdDomain(scope);
  Launchd::Type launchd_type = LaunchdType(scope);

  absl::optional<base::FilePath> path = GetProductPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::DeletePathRecursively(*path));
  EXPECT_TRUE(Launchd::GetInstance()->DeletePlist(
      launchd_domain, launchd_type, updater::CopyWakeLaunchdName(scope)));
  EXPECT_TRUE(Launchd::GetInstance()->DeletePlist(
      launchd_domain, launchd_type,
      updater::CopyUpdateServiceInternalLaunchdName(scope)));
  EXPECT_TRUE(Launchd::GetInstance()->DeletePlist(
      launchd_domain, launchd_type,
      updater::CopyUpdateServiceLaunchdName(scope)));

  path = GetDataDirPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::DeletePathRecursively(*path));

  absl::optional<base::FilePath> keystone_path = GetKeystoneFolderPath(scope);
  EXPECT_TRUE(keystone_path);
  if (keystone_path)
    EXPECT_TRUE(base::DeletePathRecursively(*keystone_path));

  @autoreleasepool {
    RemoveJobFromLaunchd(scope, launchd_domain, launchd_type,
                         CopyWakeLaunchdName(scope));
    RemoveJobFromLaunchd(scope, launchd_domain, launchd_type,
                         CopyUpdateServiceLaunchdName(scope));
    RemoveJobFromLaunchd(scope, launchd_domain, launchd_type,
                         CopyUpdateServiceInternalLaunchdName(scope));
  }

  // Also clean up any other versions of the updater that are around.
  base::CommandLine launchctl(base::FilePath("/bin/launchctl"));
  launchctl.AppendArg("list");
  std::string out;
  ASSERT_TRUE(base::GetAppOutput(launchctl, &out));
  for (const auto& token : base::SplitStringPiece(out, base::kWhitespaceASCII,
                                                  base::TRIM_WHITESPACE,
                                                  base::SPLIT_WANT_NONEMPTY)) {
    if (base::StartsWith(token, MAC_BUNDLE_IDENTIFIER_STRING)) {
      std::string out_rm;
      base::CommandLine launchctl_rm(base::FilePath("/bin/launchctl"));
      launchctl_rm.AppendArg("remove");
      launchctl_rm.AppendArg(token);
      ASSERT_TRUE(base::GetAppOutput(launchctl_rm, &out_rm));
    }
  }
}

void ExpectClean(UpdaterScope scope) {
  Launchd::Domain launchd_domain = LaunchdDomain(scope);
  Launchd::Type launchd_type = LaunchdType(scope);

  // Files must not exist on the file system.
  absl::optional<base::FilePath> path = GetProductPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_FALSE(base::PathExists(*path));
  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(
      launchd_domain, launchd_type, updater::CopyWakeLaunchdName(scope)));
  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(
      launchd_domain, launchd_type,
      updater::CopyUpdateServiceInternalLaunchdName(scope)));
  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(
      launchd_domain, launchd_type,
      updater::CopyUpdateServiceLaunchdName(scope)));

  path = GetDataDirPath(scope);
  EXPECT_TRUE(path);
  if (path && base::PathExists(*path)) {
    // If the path exists, then expect only the log file to be present.
    int count = CountDirectoryFiles(*path);
    EXPECT_LT(count, 2);
    if (count == 1)
      EXPECT_TRUE(base::PathExists(path->AppendASCII("updater.log")));
  }
  // Keystone must not exist on the file system.
  absl::optional<base::FilePath> keystone_path = GetKeystoneFolderPath(scope);
  EXPECT_TRUE(keystone_path);
  if (keystone_path)
    EXPECT_FALSE(base::PathExists(*keystone_path));

  ExpectServiceAbsent(scope, GetUpdateServiceLaunchdName(scope));
  ExpectServiceAbsent(scope, GetUpdateServiceInternalLaunchdName(scope));
}

void ExpectInstalled(UpdaterScope scope) {
  Launchd::Domain launchd_domain = LaunchdDomain(scope);
  Launchd::Type launchd_type = LaunchdType(scope);

  // Files must exist on the file system.
  absl::optional<base::FilePath> path = GetProductPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::PathExists(*path));

  EXPECT_TRUE(Launchd::GetInstance()->PlistExists(launchd_domain, launchd_type,
                                                  CopyWakeLaunchdName(scope)));
  EXPECT_TRUE(Launchd::GetInstance()->PlistExists(
      launchd_domain, launchd_type,
      CopyUpdateServiceInternalLaunchdName(scope)));
}

void ExpectActiveUpdater(UpdaterScope scope) {
  Launchd::Domain launchd_domain = LaunchdDomain(scope);
  Launchd::Type launchd_type = LaunchdType(scope);

  // Files must exist on the file system.
  absl::optional<base::FilePath> path = GetProductPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::PathExists(*path));

  EXPECT_TRUE(Launchd::GetInstance()->PlistExists(
      launchd_domain, launchd_type, CopyUpdateServiceLaunchdName(scope)));
}

absl::optional<base::FilePath> GetInstalledExecutablePath(UpdaterScope scope) {
  return GetUpdaterExecutablePath(scope);
}

void ExpectCandidateUninstalled(UpdaterScope scope) {
  Launchd::Domain launchd_domain = LaunchdDomain(scope);
  Launchd::Type launchd_type = LaunchdType(scope);

  absl::optional<base::FilePath> versioned_folder_path =
      GetVersionedUpdaterFolderPath(scope);
  EXPECT_TRUE(versioned_folder_path);
  if (versioned_folder_path)
    EXPECT_FALSE(base::PathExists(*versioned_folder_path));

  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(launchd_domain, launchd_type,
                                                   CopyWakeLaunchdName(scope)));
  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(
      launchd_domain, launchd_type,
      CopyUpdateServiceInternalLaunchdName(scope)));
}

void Uninstall(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetExecutablePath();
  ASSERT_TRUE(path);
  base::CommandLine command_line(*path);
  command_line.AppendSwitch(kUninstallSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(scope, command_line, &exit_code));
  EXPECT_EQ(exit_code, 0);
}

absl::optional<base::FilePath> GetFakeUpdaterInstallFolderPath(
    UpdaterScope scope,
    const base::Version& version) {
  return GetExecutableFolderPathForVersion(scope, version);
}

void SetActive(UpdaterScope scope, const std::string& app_id) {
  const absl::optional<base::FilePath> path = GetActiveFile(scope, app_id);
  ASSERT_TRUE(path);
  VLOG(0) << "Actives file: " << *path;
  base::File::Error err = base::File::FILE_OK;
  EXPECT_TRUE(base::CreateDirectoryAndGetError(path->DirName(), &err))
      << "Error: " << err;
  EXPECT_TRUE(base::WriteFile(*path, ""));
}

void ExpectActive(UpdaterScope scope, const std::string& app_id) {
  const absl::optional<base::FilePath> path = GetActiveFile(scope, app_id);
  ASSERT_TRUE(path);
  EXPECT_TRUE(base::PathExists(*path));
  EXPECT_TRUE(base::PathIsWritable(*path));
}

void ExpectNotActive(UpdaterScope scope, const std::string& app_id) {
  const absl::optional<base::FilePath> path = GetActiveFile(scope, app_id);
  ASSERT_TRUE(path);
  EXPECT_FALSE(base::PathExists(*path));
  EXPECT_FALSE(base::PathIsWritable(*path));
}

void WaitForUpdaterExit(UpdaterScope /*scope*/) {
  ASSERT_TRUE(WaitFor(base::BindRepeating([]() {
    std::string ps_stdout;
    EXPECT_TRUE(base::GetAppOutput({"ps", "ax", "-o", "command"}, &ps_stdout));
    if (ps_stdout.find(GetExecutablePath().BaseName().AsUTF8Unsafe()) ==
        std::string::npos) {
      return true;
    }
    return false;
  })));
}

void SetupRealUpdaterLowerVersion(UpdaterScope scope) {
  base::FilePath source_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path));
  base::FilePath old_updater_path =
      source_path.Append("third_party").Append("updater");
#if BUILDFLAG(CHROMIUM_BRANDING)
#if defined(ARCH_CPU_ARM64)
  old_updater_path =
      old_updater_path.Append("chromium_mac_arm64").Append("updater");
#elif defined(ARCH_CPU_X86_64)
  old_updater_path =
      old_updater_path.Append("chromium_mac_amd64").Append("updater");
#endif
#elif BUILDFLAG(GOOGLE_CHROME_BRANDING)
  old_updater_path = old_updater_path.Append("chrome_mac_universal");
#endif
  base::CommandLine command_line(
      old_updater_path.Append(PRODUCT_FULLNAME_STRING "_test.app")
          .Append("Contents")
          .Append("MacOS")
          .Append(PRODUCT_FULLNAME_STRING "_test"));
  command_line.AppendSwitch(kInstallSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(scope, command_line, &exit_code));
  ASSERT_EQ(exit_code, 0);
}

void SetupFakeLegacyUpdaterData(UpdaterScope scope) {
  base::FilePath test_ticket_store_path;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_TEST_DATA, &test_ticket_store_path));
  test_ticket_store_path =
      test_ticket_store_path.Append(FILE_PATH_LITERAL("updater"))
          .Append(FILE_PATH_LITERAL("Keystone.legacy.ticketstore"));

  base::FilePath keystone_ticket_store_path =
      GetKeystoneFolderPath(scope)->Append(FILE_PATH_LITERAL("TicketStore"));
  ASSERT_TRUE(base::CreateDirectory(keystone_ticket_store_path));
  ASSERT_TRUE(base::CopyFile(test_ticket_store_path,
                             keystone_ticket_store_path.Append(
                                 FILE_PATH_LITERAL("Keystone.ticketstore"))));
}

void ExpectLegacyUpdaterDataMigrated(UpdaterScope scope) {
  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope);
  auto persisted_data =
      base::MakeRefCounted<PersistedData>(global_prefs->GetPrefService());

  // Keystone should not be migrated.
  EXPECT_FALSE(
      persisted_data->GetProductVersion("com.google.keystone").IsValid());

  // Uninstalled app should not be migrated.
  EXPECT_FALSE(
      persisted_data->GetProductVersion("com.chromium.NonExistApp").IsValid());

  // App Kipple.
  const std::string kKippleApp = "com.chromium.kipple";
  EXPECT_EQ(persisted_data->GetProductVersion(kKippleApp),
            base::Version("1.2.3.4"));
  EXPECT_EQ(persisted_data->GetExistenceCheckerPath(kKippleApp),
            base::FilePath("/"));
  EXPECT_TRUE(persisted_data->GetAP(kKippleApp).empty());
  EXPECT_TRUE(persisted_data->GetBrandCode(kKippleApp).empty());
  EXPECT_TRUE(persisted_data->GetBrandPath(kKippleApp).empty());
  EXPECT_TRUE(persisted_data->GetFingerprint(kKippleApp).empty());

  // App PopularApp.
  const std::string kPopularApp = "com.chromium.PopularApp";
  EXPECT_EQ(persisted_data->GetProductVersion(kPopularApp),
            base::Version("101.100.1000.9999"));
  EXPECT_EQ(persisted_data->GetExistenceCheckerPath(kPopularApp),
            base::FilePath("/"));
  EXPECT_EQ(persisted_data->GetAP(kPopularApp), "GOOG");
  EXPECT_TRUE(persisted_data->GetBrandCode(kKippleApp).empty());
  EXPECT_EQ(persisted_data->GetBrandPath(kPopularApp), base::FilePath("/"));
  EXPECT_TRUE(persisted_data->GetFingerprint(kPopularApp).empty());
}

}  // namespace test
}  // namespace updater
