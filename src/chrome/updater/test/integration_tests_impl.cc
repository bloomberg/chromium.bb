// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/integration_tests_impl.h"

#include <cstdlib>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/test/server.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/re2/src/re2/re2.h"

namespace updater {
namespace test {
namespace {

constexpr char kSelfUpdateCRXName[] = "updater_selfupdate.crx3";
#if BUILDFLAG(IS_MAC)
constexpr char kSelfUpdateCRXRun[] = PRODUCT_FULLNAME_STRING "_test.app";
constexpr char kDoNothingCRXName[] = "updater_qualification_app_dmg.crx";
constexpr char kDoNothingCRXRun[] = "updater_qualification_app_dmg.dmg";
#elif BUILDFLAG(IS_WIN)
constexpr char kSelfUpdateCRXRun[] = "UpdaterSetup_test.exe";
constexpr char kDoNothingCRXName[] = "updater_qualification_app_exe.crx";
constexpr char kDoNothingCRXRun[] = "qualification_app.exe";
#elif BUILDFLAG(IS_LINUX)
constexpr char kSelfUpdateCRXRun[] = "UpdaterSetup_test";
constexpr char kDoNothingCRXName[] = "updater_qualification_app.crx";
constexpr char kDoNothingCRXRun[] = "qualification_app";
#endif

std::string GetHashHex(const base::FilePath& file) {
  std::unique_ptr<crypto::SecureHash> hasher(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  base::MemoryMappedFile mmfile;
  EXPECT_TRUE(mmfile.Initialize(file));  // Note: This fails with an empty file.
  hasher->Update(mmfile.data(), mmfile.length());
  uint8_t actual_hash[crypto::kSHA256Length] = {0};
  hasher->Finish(actual_hash, sizeof(actual_hash));
  return base::HexEncode(actual_hash, sizeof(actual_hash));
}

std::string GetUpdateResponse(const std::string& app_id,
                              const std::string& codebase,
                              const base::Version& version,
                              const base::FilePath& update_file,
                              const std::string& run_action,
                              const std::string& arguments) {
  return base::StringPrintf(
      ")]}'\n"
      R"({"response":{)"
      R"(  "protocol":"3.1",)"
      R"(  "app":[)"
      R"(    {)"
      R"(      "appid":"%s",)"
      R"(      "status":"ok",)"
      R"(      "updatecheck":{)"
      R"(        "status":"ok",)"
      R"(        "urls":{"url":[{"codebase":"%s"}]},)"
      R"(        "manifest":{)"
      R"(          "version":"%s",)"
      R"(          "run":"%s",)"
      R"(          "arguments":"%s",)"
      R"(          "packages":{)"
      R"(            "package":[)"
      R"(              {"name":"%s","hash_sha256":"%s"})"
      R"(            ])"
      R"(          })"
      R"(        })"
      R"(      })"
      R"(    })"
      R"(  ])"
      R"(}})",
      app_id.c_str(), codebase.c_str(), version.GetString().c_str(),
      run_action.c_str(), arguments.c_str(),
      update_file.BaseName().AsUTF8Unsafe().c_str(),
      GetHashHex(update_file).c_str());
}

base::RepeatingCallback<bool(const std::string&)> GetScopePredicate(
    UpdaterScope scope) {
  return base::BindLambdaForTesting([scope](const std::string& request_body) {
    const bool is_match = [&scope, &request_body]() {
      const absl::optional<base::Value> doc =
          base::JSONReader::Read(request_body);
      if (!doc || !doc->is_dict())
        return false;
      const base::Value* object_request = doc->FindKey("request");
      if (!object_request || !object_request->is_dict())
        return false;
      const base::Value* value_ismachine = object_request->FindKey("ismachine");
      if (!value_ismachine || !value_ismachine->is_bool())
        return false;
      switch (scope) {
        case UpdaterScope::kSystem:
          return value_ismachine->GetBool();
        case UpdaterScope::kUser:
          return !value_ismachine->GetBool();
      }
    }();
    if (!is_match) {
      ADD_FAILURE() << R"(Request does not match "ismachine": )"
                    << request_body;
    }
    return is_match;
  });
}

}  // namespace

int CountDirectoryFiles(const base::FilePath& dir) {
  base::FileEnumerator it(dir, false, base::FileEnumerator::FILES);
  int res = 0;
  for (base::FilePath name = it.Next(); !name.empty(); name = it.Next())
    ++res;
  return res;
}

void RegisterApp(UpdaterScope scope, const std::string& app_id) {
  scoped_refptr<UpdateService> update_service = CreateUpdateServiceProxy(scope);
  RegistrationRequest registration;
  registration.app_id = app_id;
  registration.version = base::Version("0.1");
  base::RunLoop loop;
  update_service->RegisterApp(
      registration, base::BindOnce(base::BindLambdaForTesting(
                        [&loop](const RegistrationResponse& response) {
                          EXPECT_EQ(response.status_code, 0);
                          loop.Quit();
                        })));
  loop.Run();
}

void ExpectVersionActive(UpdaterScope scope, const std::string& version) {
  scoped_refptr<GlobalPrefs> prefs = CreateGlobalPrefs(scope);
  ASSERT_NE(prefs, nullptr) << "Failed to acquire GlobalPrefs.";
  EXPECT_EQ(prefs->GetActiveVersion(), version);
}

void ExpectVersionNotActive(UpdaterScope scope, const std::string& version) {
  scoped_refptr<GlobalPrefs> prefs = CreateGlobalPrefs(scope);
  ASSERT_NE(prefs, nullptr) << "Failed to acquire GlobalPrefs.";
  EXPECT_NE(prefs->GetActiveVersion(), version);
}

void Install(UpdaterScope scope) {
  const base::FilePath path = GetSetupExecutablePath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch(kInstallSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(scope, command_line, &exit_code));
  EXPECT_EQ(exit_code, 0);
}

void PrintLog(UpdaterScope scope) {
  std::string contents;
  absl::optional<base::FilePath> path = GetDataDirPath(scope);
  EXPECT_TRUE(path);
  if (path &&
      base::ReadFileToString(path->AppendASCII("updater.log"), &contents)) {
    VLOG(0) << "Contents of updater.log:";
    VLOG(0) << contents;
    VLOG(0) << "End contents of updater.log.";
  } else {
    VLOG(0) << "Failed to read updater.log file.";
  }
}

const testing::TestInfo* GetTestInfo() {
  return testing::UnitTest::GetInstance()->current_test_info();
}

base::FilePath GetLogDestinationDir() {
  // Fetch path to ${ISOLATED_OUTDIR} env var.
  // ResultDB reads logs and test artifacts info from there.
  const char* var = std::getenv("ISOLATED_OUTDIR");
  return var ? base::FilePath::FromUTF8Unsafe(var) : base::FilePath();
}

void CopyLog(const base::FilePath& src_dir) {
  // TODO(crbug.com/1159189): copy other test artifacts.
  base::FilePath dest_dir = GetLogDestinationDir();
  if (!dest_dir.empty() && base::PathExists(dest_dir) &&
      base::PathExists(src_dir)) {
    base::FilePath test_name_path = dest_dir.AppendASCII(base::StrCat(
        {GetTestInfo()->test_suite_name(), ".", GetTestInfo()->name()}));
    EXPECT_TRUE(base::CreateDirectory(test_name_path));

    base::FilePath dest_file_path = test_name_path.AppendASCII("updater.log");
    base::FilePath log_path = src_dir.AppendASCII("updater.log");
    VLOG(0) << "Copying updater.log file. From: " << log_path
            << ". To: " << dest_file_path;
    EXPECT_TRUE(base::CopyFile(log_path, dest_file_path));
  }
}

void RunWake(UpdaterScope scope, int expected_exit_code) {
  const absl::optional<base::FilePath> installed_executable_path =
      GetInstalledExecutablePath(scope);
  ASSERT_TRUE(installed_executable_path);
  EXPECT_TRUE(base::PathExists(*installed_executable_path));
  base::CommandLine command_line(*installed_executable_path);
  command_line.AppendSwitch(kWakeSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(scope, command_line, &exit_code));
  EXPECT_EQ(exit_code, expected_exit_code);
}

void RunWakeActive(UpdaterScope scope, int expected_exit_code) {
  // Find the active version.
  base::Version active_version;
  {
    scoped_refptr<UpdateService> service = CreateUpdateServiceProxy(scope);
    base::RunLoop loop;
    service->GetVersion(base::BindOnce(base::BindLambdaForTesting(
        [&loop, &active_version](const base::Version& version) {
          active_version = version;
          loop.Quit();
        })));
    loop.Run();
  }
  ASSERT_TRUE(active_version.IsValid());

  // Invoke the wake client of that version.
  base::CommandLine command_line(
      GetVersionedUpdaterFolderPathForVersion(scope, active_version)
          ->Append(GetExecutableRelativePath()));
  command_line.AppendSwitch(kWakeSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(scope, command_line, &exit_code));
  EXPECT_EQ(exit_code, expected_exit_code);
}

void Update(UpdaterScope scope, const std::string& app_id) {
  scoped_refptr<UpdateService> update_service = CreateUpdateServiceProxy(scope);
  base::RunLoop loop;
  update_service->Update(
      app_id, UpdateService::Priority::kForeground,
      UpdateService::PolicySameVersionUpdate::kNotAllowed, base::DoNothing(),
      base::BindOnce(base::BindLambdaForTesting(
          [&loop](UpdateService::Result result_unused) { loop.Quit(); })));
  loop.Run();
}

void UpdateAll(UpdaterScope scope) {
  scoped_refptr<UpdateService> update_service = CreateUpdateServiceProxy(scope);
  base::RunLoop loop;
  update_service->UpdateAll(
      base::DoNothing(),
      base::BindOnce(base::BindLambdaForTesting(
          [&loop](UpdateService::Result result_unused) { loop.Quit(); })));
  loop.Run();
}

void SetupFakeUpdaterPrefs(UpdaterScope scope, const base::Version& version) {
  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope);
  ASSERT_TRUE(global_prefs) << "No global prefs.";
  global_prefs->SetActiveVersion(version.GetString());
  global_prefs->SetSwapping(false);
  PrefsCommitPendingWrites(global_prefs->GetPrefService());

  ASSERT_EQ(version.GetString(), global_prefs->GetActiveVersion());
}

void SetupFakeUpdaterInstallFolder(UpdaterScope scope,
                                   const base::Version& version) {
  const absl::optional<base::FilePath> folder_path =
      GetFakeUpdaterInstallFolderPath(scope, version);
  ASSERT_TRUE(folder_path);
  ASSERT_TRUE(base::CreateDirectory(*folder_path));
}

void SetupFakeUpdater(UpdaterScope scope, const base::Version& version) {
  SetupFakeUpdaterPrefs(scope, version);
  SetupFakeUpdaterInstallFolder(scope, version);
}

void SetupFakeUpdaterVersion(UpdaterScope scope, int offset) {
  ASSERT_NE(offset, 0);
  std::vector<uint32_t> components =
      base::Version(kUpdaterVersion).components();
  base::CheckedNumeric<uint32_t> new_version = components[0];
  new_version += offset;
  ASSERT_TRUE(new_version.AssignIfValid(&components[0]));
  SetupFakeUpdater(scope, base::Version(std::move(components)));
}

void SetupFakeUpdaterLowerVersion(UpdaterScope scope) {
  SetupFakeUpdaterVersion(scope, -1);
}

void SetupFakeUpdaterHigherVersion(UpdaterScope scope) {
  SetupFakeUpdaterVersion(scope, 1);
}

void SetExistenceCheckerPath(UpdaterScope scope,
                             const std::string& app_id,
                             const base::FilePath& path) {
  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope);
  base::MakeRefCounted<PersistedData>(global_prefs->GetPrefService())
      ->SetExistenceCheckerPath(app_id, path);
  PrefsCommitPendingWrites(global_prefs->GetPrefService());
}

void SetServerStarts(UpdaterScope scope, int value) {
  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope);
  for (int i = 0; i <= value; ++i) {
    global_prefs->CountServerStarts();
  }
  PrefsCommitPendingWrites(global_prefs->GetPrefService());
}

void ExpectAppUnregisteredExistenceCheckerPath(UpdaterScope scope,
                                               const std::string& app_id) {
  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope);
  auto persisted_data =
      base::MakeRefCounted<PersistedData>(global_prefs->GetPrefService());
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("")).value(),
            persisted_data->GetExistenceCheckerPath(app_id).value());
}

void ExpectAppVersion(UpdaterScope scope,
                      const std::string& app_id,
                      const base::Version& version) {
  const base::Version app_version =
      base::MakeRefCounted<PersistedData>(
          CreateGlobalPrefs(scope)->GetPrefService())
          ->GetProductVersion(app_id);
  EXPECT_TRUE(app_version.IsValid() && version == app_version);
}

bool Run(UpdaterScope scope, base::CommandLine command_line, int* exit_code) {
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait_process;
  command_line.AppendSwitch(kEnableLoggingSwitch);
  command_line.AppendSwitchASCII(kLoggingModuleSwitch,
                                 kLoggingModuleSwitchValue);
  if (scope == UpdaterScope::kSystem) {
    command_line.AppendSwitch(kSystemSwitch);
    command_line = MakeElevated(command_line);
  }
  VLOG(0) << " Run command: " << command_line.GetCommandLineString();
  base::Process process = base::LaunchProcess(command_line, {});
  if (!process.IsValid())
    return false;

  // TODO(crbug.com/1096654): Get the timeout from TestTimeouts.
  return process.WaitForExitWithTimeout(base::Seconds(45), exit_code);
}

bool WaitFor(base::RepeatingCallback<bool()> predicate) {
  base::TimeTicks deadline =
      base::TimeTicks::Now() + TestTimeouts::action_max_timeout();
  while (base::TimeTicks::Now() < deadline) {
    if (predicate.Run())
      return true;
    base::PlatformThread::Sleep(base::Milliseconds(200));
  }
  return false;
}

bool RequestMatcherRegex(const std::string& request_body_regex,
                         const std::string& request_body) {
  if (!re2::RE2::PartialMatch(request_body, request_body_regex)) {
    ADD_FAILURE() << "Request with body: " << request_body
                  << " did not match expected regex " << request_body_regex;
    return false;
  }
  return true;
}

void ExpectSelfUpdateSequence(UpdaterScope scope, ScopedServer* test_server) {
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &test_data_path));
  base::FilePath crx_path = test_data_path.AppendASCII(kSelfUpdateCRXName);
  ASSERT_TRUE(base::PathExists(crx_path));

  // First request: update check.
  test_server->ExpectOnce(
      {base::BindRepeating(
           RequestMatcherRegex,
           base::StringPrintf(R"(.*"appid":"%s".*)", kUpdaterAppId)),
       GetScopePredicate(scope)},
      GetUpdateResponse(
          kUpdaterAppId, test_server->base_url().spec(),
          base::Version(kUpdaterVersion), crx_path, kSelfUpdateCRXRun,
          base::StrCat({"--update",
                        scope == UpdaterScope::kSystem ? " --system" : ""})));

  // Second request: update download.
  std::string crx_bytes;
  base::ReadFileToString(crx_path, &crx_bytes);
  test_server->ExpectOnce({base::BindRepeating(RequestMatcherRegex, "")},
                          crx_bytes);

  // Third request: event ping.
  test_server->ExpectOnce(
      {base::BindRepeating(
           RequestMatcherRegex,
           base::StringPrintf(R"(.*"eventresult":1,"eventtype":3,)"
                              R"("nextversion":"%s",.*)",
                              kUpdaterVersion)),
       GetScopePredicate(scope)},
      ")]}'\n");
}

void ExpectUpdateSequence(UpdaterScope scope,
                          ScopedServer* test_server,
                          const std::string& app_id,
                          const base::Version& from_version,
                          const base::Version& to_version) {
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));
  base::FilePath crx_path = test_data_path.Append(FILE_PATH_LITERAL("updater"))
                                .AppendASCII(kDoNothingCRXName);
  ASSERT_TRUE(base::PathExists(crx_path));

  // First request: update check.
  test_server->ExpectOnce(
      {base::BindRepeating(
           RequestMatcherRegex,
           base::StringPrintf(R"(.*"appid":"%s".*)", app_id.c_str())),
       GetScopePredicate(scope)},
      GetUpdateResponse(app_id, test_server->base_url().spec(), to_version,
                        crx_path, kDoNothingCRXRun, {}));

  // Second request: update download.
  std::string crx_bytes;
  base::ReadFileToString(crx_path, &crx_bytes);
  test_server->ExpectOnce({base::BindRepeating(RequestMatcherRegex, "")},
                          crx_bytes);

  // Third request: event ping.
  test_server->ExpectOnce(
      {base::BindRepeating(
           RequestMatcherRegex,
           base::StringPrintf(R"(.*"eventresult":1,"eventtype":3,)"
                              R"("nextversion":"%s","previousversion":"%s".*)",
                              to_version.GetString().c_str(),
                              from_version.GetString().c_str())),
       GetScopePredicate(scope)},
      ")]}'\n");
}

// Runs multiple cycles of instantiating the update service, calling
// `GetVersion()`, then releasing the service interface.
void StressUpdateService(UpdaterScope scope) {
  base::RunLoop loop;

  // Number of times to run the cycle of instantiating the service.
  int n = 10;

  // Delay in milliseconds between successive cycles.
  const int kDelayBetweenLoopsMS = 0;

  // Runs on the main sequence.
  auto loop_closure = [&]() {
    if (--n)
      return false;
    loop.Quit();
    return true;
  };

  // Creates a task runner, and runs the service instance on it.
  using LoopClosure = decltype(loop_closure);
  auto stress_runner = [scope, loop_closure]() {
    // `task_runner` is always bound on the main sequence.
    struct Local {
      static void GetVersion(
          UpdaterScope scope,
          scoped_refptr<base::SequencedTaskRunner> task_runner,
          LoopClosure loop_closure) {
        auto service_task_runner =
            base::ThreadPool::CreateSingleThreadTaskRunner(
                {}, base::SingleThreadTaskRunnerThreadMode::DEDICATED);
        service_task_runner->PostDelayedTask(
            FROM_HERE,
            base::BindLambdaForTesting([scope, task_runner, loop_closure]() {
              auto update_service = CreateUpdateServiceProxy(scope);
              update_service->GetVersion(
                  base::BindOnce(GetVersionCallback, scope, update_service,
                                 task_runner, loop_closure));
            }),
            base::Milliseconds(kDelayBetweenLoopsMS));
      }

      static void GetVersionCallback(
          UpdaterScope scope,
          scoped_refptr<UpdateService> /*update_service*/,
          scoped_refptr<base::SequencedTaskRunner> task_runner,
          LoopClosure loop_closure,
          const base::Version& version) {
        EXPECT_EQ(version, base::Version(kUpdaterVersion));
        task_runner->PostTask(
            FROM_HERE,
            base::BindLambdaForTesting([scope, task_runner, loop_closure]() {
              if (loop_closure()) {
                return;
              }
              GetVersion(scope, task_runner, loop_closure);
            }));
      }
    };

    Local::GetVersion(scope, base::SequencedTaskRunnerHandle::Get(),
                      loop_closure);
  };

  stress_runner();
  loop.Run();
}

void CallServiceUpdate(UpdaterScope updater_scope,
                       const std::string& app_id,
                       bool same_version_update_allowed) {
  UpdateService::PolicySameVersionUpdate policy_same_version_update =
      same_version_update_allowed
          ? UpdateService::PolicySameVersionUpdate::kAllowed
          : UpdateService::PolicySameVersionUpdate::kNotAllowed;

  scoped_refptr<UpdateService> service_proxy =
      CreateUpdateServiceProxy(updater_scope);

  base::RunLoop loop;
  service_proxy->Update(
      app_id, UpdateService::Priority::kForeground, policy_same_version_update,
      base::BindLambdaForTesting([](const UpdateService::UpdateState&) {}),
      base::BindLambdaForTesting([&](UpdateService::Result result) {
        EXPECT_EQ(result, UpdateService::Result::kSuccess);
        loop.Quit();
      }));

  loop.Run();
}

}  // namespace test
}  // namespace updater
