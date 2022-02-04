// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_INTEGRATION_TESTS_IMPL_H_
#define CHROME_UPDATER_TEST_INTEGRATION_TESTS_IMPL_H_

#include <string>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class CommandLine;
class Value;
class Version;
}  // namespace base

class GURL;

namespace updater {

enum class UpdaterScope;

namespace test {

class ScopedServer;

// Returns the path to the updater executable (in the build output directory).
base::FilePath GetSetupExecutablePath();

// Prints the updater.log file to stdout.
void PrintLog(UpdaterScope scope);

// Removes traces of the updater from the system. It is best to run this at the
// start of each test in case a previous crash or timeout on the machine running
// the test left the updater in an installed or partially installed state.
void Clean(UpdaterScope scope);

// Expects that the system is in a clean state, i.e. no updater is installed and
// no traces of an updater exist. Should be run at the start and end of each
// test.
void ExpectClean(UpdaterScope scope);

// Places the updater into test mode (use `url` as the update server and disable
// CUP).
void EnterTestMode(const GURL& url);

// Copies the logs to a location where they can be retrieved by ResultDB.
void CopyLog(const base::FilePath& src_dir);

// Waits for a given predicate to become true, testing it by polling. Returns
// true if the predicate becomes true before a timeout, otherwise returns false.
bool WaitFor(base::RepeatingCallback<bool()> predicate);

// Returns the path to the updater data dir.
absl::optional<base::FilePath> GetDataDirPath(UpdaterScope scope);

// Expects that the updater is installed on the system.
void ExpectInstalled(UpdaterScope scope);

// Installs the updater.
void Install(UpdaterScope scope);

// Expects that the updater is installed on the system and the launchd tasks
// are updated correctly.
void ExpectActiveUpdater(UpdaterScope scope);

void ExpectVersionActive(UpdaterScope scope, const std::string& version);
void ExpectVersionNotActive(UpdaterScope scope, const std::string& version);

// Uninstalls the updater. If the updater was installed during the test, it
// should be uninstalled before the end of the test to avoid having an actual
// live updater on the machine that ran the test.
void Uninstall(UpdaterScope scope);

// Runs the wake client and wait for it to exit. Assert that it exits with
// `exit_code`. The server should exit a few seconds after.
void RunWake(UpdaterScope scope, int exit_code);

// As RunWake, but runs the wake client for whatever version of the server is
// active, rather than kUpdaterVersion.
void RunWakeActive(UpdaterScope scope, int exit_code);

// Invokes the active instance's UpdateService::Update (via RPC) for an app.
void Update(UpdaterScope scope, const std::string& app_id);

// Invokes the active instance's UpdateService::UpdateAll (via RPC).
void UpdateAll(UpdaterScope scope);

// Runs the command and waits for it to exit or time out.
bool Run(UpdaterScope scope, base::CommandLine command_line, int* exit_code);

// Returns the path of the Updater executable.
absl::optional<base::FilePath> GetInstalledExecutablePath(UpdaterScope scope);

// Returns the folder path under which the executable for the fake updater
// should reside.
absl::optional<base::FilePath> GetFakeUpdaterInstallFolderPath(
    UpdaterScope scope,
    const base::Version& version);

// Creates Prefs with the fake updater version set as active.
void SetupFakeUpdaterPrefs(UpdaterScope scope, const base::Version& version);

// Creates an install folder on the system with the fake updater version.
void SetupFakeUpdaterInstallFolder(UpdaterScope scope,
                                   const base::Version& version);

// Sets up a fake updater on the system at a version lower than the test.
void SetupFakeUpdaterLowerVersion(UpdaterScope scope);

// Sets up a real updater on the system at a version lower than the test. The
// exact version of the updater is not defined.
void SetupRealUpdaterLowerVersion(UpdaterScope scope);

// Sets up a fake updater on the system at a version higher than the test.
void SetupFakeUpdaterHigherVersion(UpdaterScope scope);

// Expects that this version of updater is uninstalled from the system.
void ExpectCandidateUninstalled(UpdaterScope scope);

// Sets the active bit for `app_id`.
void SetActive(UpdaterScope scope, const std::string& app_id);

// Expects that the active bit for `app_id` is set.
void ExpectActive(UpdaterScope scope, const std::string& app_id);

// Expects that the active bit for `app_id` is unset.
void ExpectNotActive(UpdaterScope scope, const std::string& app_id);

void SetExistenceCheckerPath(UpdaterScope scope,
                             const std::string& app_id,
                             const base::FilePath& path);

void SetServerStarts(UpdaterScope scope, int value);

void ExpectAppUnregisteredExistenceCheckerPath(UpdaterScope scope,
                                               const std::string& app_id);

void ExpectAppVersion(UpdaterScope scope,
                      const std::string& app_id,
                      const base::Version& version);

void RegisterApp(UpdaterScope scope, const std::string& app_id);

void WaitForUpdaterExit(UpdaterScope scope);

#if BUILDFLAG(IS_WIN)
void ExpectInterfacesRegistered(UpdaterScope scope);
void ExpectLegacyUpdate3WebSucceeds(UpdaterScope scope,
                                    const std::string& app_id,
                                    int expected_final_state,
                                    int expected_error_code);
void ExpectLegacyProcessLauncherSucceeds(UpdaterScope scope);
void RunTestServiceCommand(const std::string& sub_command);

// Calls a function defined in test/service/win/rpc_client.py.
// Entries of the `arguments` dictionary should be the function's parameter
// name/value pairs.
void InvokeTestServiceFunction(
    const std::string& function_name,
    const base::flat_map<std::string, base::Value>& arguments);

void RunUninstallCmdLine(UpdaterScope scope);
#endif  // BUILDFLAG(IS_WIN)

// Returns the number of files in the directory, not including directories,
// links, or dot dot.
int CountDirectoryFiles(const base::FilePath& dir);

// Returns true if the `request_body_regex` partially matches `request_body`.
bool RequestMatcherRegex(const std::string& request_body_regex,
                         const std::string& request_body);

void ExpectSelfUpdateSequence(UpdaterScope scope, ScopedServer* test_server);

void ExpectUpdateSequence(UpdaterScope scope,
                          ScopedServer* test_server,
                          const std::string& app_id,
                          const base::Version& from_version,
                          const base::Version& to_version);

void StressUpdateService(UpdaterScope scope);

void CallServiceUpdate(UpdaterScope updater_scope,
                       const std::string& app_id,
                       bool same_version_update_allowed);

}  // namespace test
}  // namespace updater

#endif  // CHROME_UPDATER_TEST_INTEGRATION_TESTS_IMPL_H_
