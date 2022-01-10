// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <map>
#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/updater_scope.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace updater {
namespace test {
namespace {

using ::testing::EmptyTestEventListener;
using ::testing::Test;
using ::testing::TestCase;
using ::testing::TestEventListeners;
using ::testing::TestInfo;
using ::testing::TestPartResult;
using ::testing::UnitTest;

constexpr int kSuccess = 0;
constexpr int kUnknownSwitch = 101;
constexpr int kBadCommand = 102;

template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(const std::string&, Args...)> callback) {
  return base::BindLambdaForTesting([=](Args... args) {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(flag)) {
      return callback.Run(command_line->GetSwitchValueASCII(flag),
                          std::move(args)...);
    }
    LOG(ERROR) << "Missing switch: " << flag;
    return false;
  });
}

// Overload for bool switches, represented by literals "false" and "true".
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(bool, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        if (flag == "false" || flag == "true") {
          return callback.Run(flag == "true", std::move(args)...);
        }
        return false;
      }));
}

// Overload for int switches.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(int, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        int flag_int = -1;
        if (base::StringToInt(flag, &flag_int)) {
          return callback.Run(flag_int, std::move(args)...);
        }
        return false;
      }));
}

// Overload for GURL switches.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(const GURL&, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        return callback.Run(GURL(flag), std::move(args)...);
      }));
}

// Overload for FilePath switches.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(const base::FilePath&, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        return callback.Run(base::FilePath::FromUTF8Unsafe(flag),
                            std::move(args)...);
      }));
}

// Overload for Version switches.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(const base::Version&, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        return callback.Run(base::Version(flag), std::move(args)...);
      }));
}

template <typename Arg, typename... RemainingArgs>
base::RepeatingCallback<bool(RemainingArgs...)> WithArg(
    Arg arg,
    base::RepeatingCallback<bool(Arg, RemainingArgs...)> callback) {
  return base::BindRepeating(callback, arg);
}

// Adapts the input callback to take a shutdown callback as the final parameter.
template <typename... Args>
base::RepeatingCallback<bool(Args..., base::OnceCallback<void(int)>)>
WithShutdown(base::RepeatingCallback<int(Args...)> callback) {
  return base::BindLambdaForTesting(
      [=](Args... args, base::OnceCallback<void(int)> shutdown) {
        std::move(shutdown).Run(callback.Run(args...));
        return true;
      });
}

// Short-named wrapper around BindOnce.
template <typename... Args, typename... ProvidedArgs>
base::RepeatingCallback<bool(Args..., base::OnceCallback<void(int)>)> Wrap(
    int (*func)(Args...),
    ProvidedArgs... provided_args) {
  return WithShutdown(base::BindRepeating(func, provided_args...));
}

// Overload of Wrap for functions that return void. (Returns kSuccess.)
template <typename... Args>
base::RepeatingCallback<bool(Args..., base::OnceCallback<void(int)>)> Wrap(
    void (*func)(Args...)) {
  return WithShutdown(base::BindLambdaForTesting([=](Args... args) {
    func(args...);
    return kSuccess;
  }));
}

// Helper to shorten lines below.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSystemScope(
    base::RepeatingCallback<bool(UpdaterScope, Args...)> callback) {
  return WithArg(UpdaterScope::kSystem, callback);
}

class AppTestHelper : public App {
 private:
  ~AppTestHelper() override = default;
  void FirstTaskRun() override;
  void InitializeThreadPool() override;
};

void AppTestHelper::FirstTaskRun() {
  std::map<std::string,
           base::RepeatingCallback<bool(base::OnceCallback<void(int)>)>>
      commands =
  {
    // To add additional commands, first Wrap a pointer to the target
    // function (which should be declared in integration_tests_impl.h), and
    // then use the With* helper functions to provide its arguments.
    {"clean", WithSystemScope(Wrap(&Clean))},
    {"enter_test_mode", WithSwitch("url", Wrap(&EnterTestMode))},
    {"expect_active_updater", WithSystemScope(Wrap(&ExpectActiveUpdater))},
    {"expect_app_unregistered_existence_checker_path",
     WithSwitch("app_id", WithSystemScope(Wrap(
                              &ExpectAppUnregisteredExistenceCheckerPath)))},
    {"expect_app_version",
     WithSwitch("version", WithSwitch("app_id", WithSystemScope(
                                                    Wrap(&ExpectAppVersion))))},
    {"expect_candidate_uninstalled",
     WithSystemScope(Wrap(&ExpectCandidateUninstalled))},
    {"expect_clean", WithSystemScope(Wrap(&ExpectClean))},
    {"expect_installed", WithSystemScope(Wrap(&ExpectInstalled))},
#if defined(OS_WIN)
    {"expect_interfaces_registered",
     WithSystemScope(Wrap(&ExpectInterfacesRegistered))},
    {"expect_legacy_update3web_succeeds",
     WithSwitch("expected_error_code",
                WithSwitch("expected_final_state",
                           WithSwitch("app_id",
                                      WithSystemScope(Wrap(
                                          &ExpectLegacyUpdate3WebSucceeds)))))},
    {"expect_legacy_process_launcher_succeeds",
     WithSystemScope(Wrap(&ExpectLegacyProcessLauncherSucceeds))},
    {"run_uninstall_cmd_line", WithSystemScope(Wrap(&RunUninstallCmdLine))},
#endif  // OS_WIN
    {"expect_version_active",
     WithSwitch("version", WithSystemScope(Wrap(&ExpectVersionActive)))},
    {"expect_version_not_active",
     WithSwitch("version", WithSystemScope(Wrap(&ExpectVersionNotActive)))},
    {"install", WithSystemScope(Wrap(&Install))},
    {"print_log", WithSystemScope(Wrap(&PrintLog))},
    {"run_wake", WithSwitch("exit_code", WithSystemScope(Wrap(&RunWake)))},
    {"update", WithSwitch("app_id", WithSystemScope(Wrap(&Update)))},
    {"update_all", WithSystemScope(Wrap(&UpdateAll))},
    {"register_app", WithSwitch("app_id", WithSystemScope(Wrap(&RegisterApp)))},
    {"set_existence_checker_path",
     WithSwitch("path",
                (WithSwitch("app_id",
                            WithSystemScope(Wrap(&SetExistenceCheckerPath)))))},
    {"setup_fake_updater_higher_version",
     WithSystemScope(Wrap(&SetupFakeUpdaterHigherVersion))},
    {"setup_fake_updater_lower_version",
     WithSystemScope(Wrap(&SetupFakeUpdaterLowerVersion))},
    {"set_first_registration_counter",
     WithSwitch("value", WithSystemScope(Wrap(&SetServerStarts)))},
    {"stress_update_service", WithSystemScope(Wrap(&StressUpdateService))},
    {"uninstall", WithSystemScope(Wrap(&Uninstall))},
    {"call_service_update",
     WithSwitch(
         "same_version_update_allowed",
         WithSwitch("app_id", WithSystemScope(Wrap(&CallServiceUpdate))))},
  };

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  for (const auto& entry : commands) {
    if (command_line->HasSwitch(entry.first)) {
      base::ScopedAllowBlockingForTesting allow_blocking;
      if (!entry.second.Run(base::BindOnce(&AppTestHelper::Shutdown, this))) {
        Shutdown(kBadCommand);
      }
      return;
    }
  }

  LOG(ERROR) << "No supported switch provided. Command: "
             << command_line->GetCommandLineString();
  Shutdown(kUnknownSwitch);
}

void AppTestHelper::InitializeThreadPool() {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("test_helper");
}

scoped_refptr<App> MakeAppTestHelper() {
  return base::MakeRefCounted<AppTestHelper>();
}

// Provides custom formatting for the unit test output.
class TersePrinter : public EmptyTestEventListener {
 private:
  // Called before any test activity starts.
  void OnTestProgramStart(const UnitTest& /*unit_test*/) override {}

  // Called after all test activities have ended.
  void OnTestProgramEnd(const UnitTest& unit_test) override {
    std::cout << "Command " << (unit_test.Passed() ? "SUCCEEDED" : "FAILED")
              << "." << std::endl
              << std::flush;
  }

  // Called before a test starts.
  void OnTestStart(const TestInfo& /*test_info*/) override {}

  // Called after a failed assertion or a SUCCEED() invocation. Prints a
  // backtrace showing the failure.
  void OnTestPartResult(const TestPartResult& test_part_result) override {
    std::cout << (test_part_result.failed() ? "*** Failure" : "Success")
              << " in : " << test_part_result.file_name() << ":"
              << test_part_result.line_number() << std::endl
              << test_part_result.message() << std::endl
              << std::flush;
  }

  // Called after a test ends.
  void OnTestEnd(const TestInfo& /*test_info*/) override {}
};

int IntegrationTestsHelperMain(int argc, char** argv) {
  base::PlatformThread::SetName("IntegrationTestsHelperMain");
  base::CommandLine::Init(argc, argv);

  // `test_suite` must be defined before setting log items.
  base::TestSuite test_suite(argc, argv);
  logging::SetLogItems(/*enable_process_id=*/true,
                       /*enable_thread_id=*/true,
                       /*enable_timestamp=*/true,
                       /*enable_tickcount=*/false);
#if defined(OS_WIN)
  auto scoped_com_initializer =
      std::make_unique<base::win::ScopedCOMInitializer>(
          base::win::ScopedCOMInitializer::kMTA);
#endif
  chrome::RegisterPathProvider();
  TestEventListeners& listeners = UnitTest::GetInstance()->listeners();
  delete listeners.Release(listeners.default_result_printer());
  listeners.Append(new TersePrinter);
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}

class TestHelperCommandRunner : public ::testing::Test {
 private:
  void TearDown() override {
    // Avoids reporting the thread pool instance leak in gtest.
    base::ThreadPoolInstance::Get()->JoinForTesting();
    base::ThreadPoolInstance::Set(nullptr);
  }
};

// Do not disable this test when encountering integration tests failures.
// This is not a unit test. It just wraps the execution of an integration test
// command, which is typical a step of an integration test.
TEST_F(TestHelperCommandRunner, Run) {
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  EXPECT_EQ(MakeAppTestHelper()->Run(), 0);
}

}  // namespace
}  // namespace test
}  // namespace updater

// Wraps the execution of one integration test command in a unit test. The test
// commands contain gtest assertions, therefore the invocation of test commands
// must occur within the scope of a unit test of a gtest program. The test
// helper defines a unit test "TestHelperCommandRunner.Run", which runs the
// actual test command. Returns 0 if the test command succeeded.
int main(int argc, char** argv) {
  return updater::test::IntegrationTestsHelperMain(argc, argv);
}
