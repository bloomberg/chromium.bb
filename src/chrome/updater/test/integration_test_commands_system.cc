// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif  // OS_WIN

namespace updater {
namespace test {

class IntegrationTestCommandsSystem : public IntegrationTestCommands {
 public:
  IntegrationTestCommandsSystem() = default;

  void PrintLog() const override { RunCommand("print_log"); }

  void CopyLog() const override {
    const absl::optional<base::FilePath> path = GetDataDirPath(updater_scope_);
    ASSERT_TRUE(path);
    if (path)
      updater::test::CopyLog(*path);
  }

  void Clean() const override { RunCommand("clean"); }

  void ExpectClean() const override { RunCommand("expect_clean"); }

  void Install() const override { RunCommand("install"); }

  void ExpectInstalled() const override { RunCommand("expect_installed"); }

  void Uninstall() const override { RunCommand("uninstall"); }

  void ExpectCandidateUninstalled() const override {
    RunCommand("expect_candidate_uninstalled");
  }

  void EnterTestMode(const GURL& url) const override {
    RunCommand("enter_test_mode", {Param("url", url.spec())});
  }

  void ExpectUpdateSequence(ScopedServer* test_server,
                            const std::string& app_id,
                            const base::Version& from_version,
                            const base::Version& to_version) const override {
    updater::test::ExpectUpdateSequence(updater_scope_, test_server, app_id,
                                        from_version, to_version);
  }

  void ExpectVersionActive(const std::string& version) const override {
    RunCommand("expect_version_active", {Param("version", version)});
  }

  void ExpectVersionNotActive(const std::string& version) const override {
    RunCommand("expect_version_not_active", {Param("version", version)});
  }

  void ExpectActiveUpdater() const override {
    RunCommand("expect_active_updater");
  }

  void ExpectActive(const std::string& app_id) const override {
    updater::test::ExpectActive(updater_scope_, app_id);
  }

  void ExpectNotActive(const std::string& app_id) const override {
    updater::test::ExpectNotActive(updater_scope_, app_id);
  }

  void SetupFakeUpdaterHigherVersion() const override {
    RunCommand("setup_fake_updater_higher_version");
  }

  void SetupFakeUpdaterLowerVersion() const override {
    RunCommand("setup_fake_updater_lower_version");
  }

  void SetExistenceCheckerPath(const std::string& app_id,
                               const base::FilePath& path) const override {
    RunCommand("set_existence_checker_path",
               {Param("app_id", app_id), Param("path", path.MaybeAsASCII())});
  }

  void SetServerStarts(int value) const override {
    RunCommand("set_first_registration_counter",
               {Param("value", base::NumberToString(value))});
  }

  void ExpectAppUnregisteredExistenceCheckerPath(
      const std::string& app_id) const override {
    RunCommand("expect_app_unregistered_existence_checker_path",
               {Param("app_id", app_id)});
  }

  void ExpectAppVersion(const std::string& app_id,
                        const base::Version& version) const override {
    RunCommand("expect_app_version", {Param("app_id", app_id),
                                      Param("version", version.GetString())});
  }

  void SetActive(const std::string& app_id) const override {
    updater::test::SetActive(updater_scope_, app_id);
  }

  void RunWake(int expected_exit_code) const override {
    RunCommand("run_wake",
               {Param("exit_code", base::NumberToString(expected_exit_code))});
  }

  void Update(const std::string& app_id) const override {
    RunCommand("update", {Param("app_id", app_id)});
  }

  void UpdateAll() const override { RunCommand("update_all", {}); }

  void RegisterApp(const std::string& app_id) const override {
    RunCommand("register_app", {Param("app_id", app_id)});
  }

  void WaitForServerExit() const override {
    updater::test::WaitForServerExit(updater_scope_);
  }

#if defined(OS_WIN)
  void ExpectInterfacesRegistered() const override {
    RunCommand("expect_interfaces_registered");
  }

  void ExpectLegacyUpdate3WebSucceeds(const std::string& app_id,
                                      int expected_final_state,
                                      int expected_error_code) const override {
    RunCommand("expect_legacy_update3web_succeeds",
               {Param("app_id", app_id),
                Param("expected_final_state",
                      base::NumberToString(expected_final_state)),
                Param("expected_error_code",
                      base::NumberToString(expected_error_code))});
  }

  void ExpectLegacyProcessLauncherSucceeds() const override {
    RunCommand("expect_legacy_process_launcher_succeeds");
  }

  void RunUninstallCmdLine() const override {
    RunCommand("run_uninstall_cmd_line");
  }

  void SetUpTestService() const override {
    updater::test::RunTestServiceCommand("setup");
  }

  void TearDownTestService() const override {
    updater::test::RunTestServiceCommand("teardown");
  }
#endif  // OS_WIN

  base::FilePath GetDifferentUserPath() const override {
#if defined(OS_MAC)
    // The updater_tests executable is owned by non-root.
    return base::PathService::CheckedGet(base::FILE_EXE);
#else
    NOTREACHED() << __func__ << ": not implemented.";
    return base::FilePath();
#endif
  }

  void StressUpdateService() const override {
    RunCommand("stress_update_service");
  }

  void CallServiceUpdate(const std::string& app_id,
                         UpdateService::PolicySameVersionUpdate
                             policy_same_version_update) const override {
    RunCommand("call_service_update",
               {Param("app_id", app_id),
                Param("same_version_update_allowed",
                      policy_same_version_update ==
                              UpdateService::PolicySameVersionUpdate::kAllowed
                          ? "true"
                          : "false")});
  }

 private:
  ~IntegrationTestCommandsSystem() override = default;

  struct Param {
    Param(const std::string& name, const std::string& value)
        : name(name), value(value) {}
    std::string name;
    std::string value;
  };

  // Invokes the test helper command by running a unit test from the
  // "updater_integration_tests_helper" program. The program returns 0 if
  // the unit test passes.
  void RunCommand(const std::string& command_switch,
                  const std::vector<Param>& params) const {
    const base::CommandLine command_line =
        *base::CommandLine::ForCurrentProcess();
    base::FilePath path(command_line.GetProgram());
    EXPECT_TRUE(base::PathExists(path));
    path = path.DirName();
    EXPECT_TRUE(base::PathExists(path));
    path = MakeAbsoluteFilePath(path);
    path = path.Append(FILE_PATH_LITERAL("updater_integration_tests_helper"));
#if defined(OS_WIN)
    path = path.AddExtension(L"exe");
#endif
    EXPECT_TRUE(base::PathExists(path));

    base::CommandLine helper_command(path);
    helper_command.AppendSwitch(command_switch);
    for (const Param& param : params) {
      helper_command.AppendSwitchASCII(param.name, param.value);
    }

    // Avoids the test runner banner about test debugging.
    helper_command.AppendSwitch("single-process-tests");
    helper_command.AppendSwitchASCII("gtest_filter",
                                     "TestHelperCommandRunner.Run");
    helper_command.AppendSwitchASCII("gtest_brief", "1");

    int exit_code = -1;
    ASSERT_TRUE(Run(updater_scope_, helper_command, &exit_code));

    // A failure here indicates that the integration test helper
    // process ran but the invocation of the test helper command was not
    // successful for a number of reasons.
    // If the `exit_code` is 1 then there were failed assertions in
    // the code invoked by the test command. This is the most common case.
    // Other exit codes mean that the helper command is not defined or the
    // helper command line syntax is wrong for some reason.
    EXPECT_EQ(exit_code, 0);
  }

  void RunCommand(const std::string& command_switch) const {
    RunCommand(command_switch, {});
  }

  static const UpdaterScope updater_scope_;
};

const UpdaterScope IntegrationTestCommandsSystem::updater_scope_ =
    GetTestScope();

scoped_refptr<IntegrationTestCommands> CreateIntegrationTestCommandsSystem() {
  return base::MakeRefCounted<IntegrationTestCommandsSystem>();
}

}  // namespace test
}  // namespace updater
