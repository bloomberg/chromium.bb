// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_util.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_types.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/upstart/fake_upstart_client.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_features.h"
#include "components/arc/test/arc_util_test_support.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/display/types/display_constants.h"

namespace arc {
namespace {

// If an instance is created, based on the value passed to the constructor,
// EnableARC feature is enabled/disabled in the scope.
class ScopedArcFeature {
 public:
  explicit ScopedArcFeature(bool enabled) {
    constexpr char kArcFeatureName[] = "EnableARC";
    if (enabled) {
      feature_list.InitFromCommandLine(kArcFeatureName, std::string());
    } else {
      feature_list.InitFromCommandLine(std::string(), kArcFeatureName);
    }
  }
  ~ScopedArcFeature() = default;

 private:
  base::test::ScopedFeatureList feature_list;
  DISALLOW_COPY_AND_ASSIGN(ScopedArcFeature);
};

class ScopedRtVcpuFeature {
 public:
  ScopedRtVcpuFeature(bool dual_core_enabled, bool quad_core_enabled) {
    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;

    if (dual_core_enabled)
      enabled_features.push_back(kRtVcpuDualCore);
    else
      disabled_features.push_back(kRtVcpuDualCore);

    if (quad_core_enabled)
      enabled_features.push_back(kRtVcpuQuadCore);
    else
      disabled_features.push_back(kRtVcpuQuadCore);

    feature_list.InitWithFeatures(enabled_features, disabled_features);
  }
  ~ScopedRtVcpuFeature() = default;
  ScopedRtVcpuFeature(const ScopedRtVcpuFeature&) = delete;
  ScopedRtVcpuFeature& operator=(const ScopedRtVcpuFeature&) = delete;

 private:
  base::test::ScopedFeatureList feature_list;
};

// Fake user that can be created with a specified type.
class FakeUser : public user_manager::User {
 public:
  explicit FakeUser(user_manager::UserType user_type)
      : User(AccountId::FromUserEmailGaiaId("user@test.com", "1234567890")),
        user_type_(user_type) {}
  ~FakeUser() override = default;

  // user_manager::User:
  user_manager::UserType GetType() const override { return user_type_; }

 private:
  const user_manager::UserType user_type_;

  DISALLOW_COPY_AND_ASSIGN(FakeUser);
};

class ArcUtilTest : public testing::Test {
 public:
  ArcUtilTest() { chromeos::UpstartClient::InitializeFake(); }
  ArcUtilTest(const ArcUtilTest&) = delete;
  ArcUtilTest& operator=(const ArcUtilTest&) = delete;
  ~ArcUtilTest() override = default;

  void SetUp() override {
    run_loop_ = std::make_unique<base::RunLoop>();
    RemoveUpstartStartStopJobFailures();
  }

  void TearDown() override { run_loop_.reset(); }

 protected:
  void InjectUpstartStartJobFailure(const std::string& job_name_to_fail) {
    auto* upstart_client = chromeos::FakeUpstartClient::Get();
    upstart_client->set_start_job_cb(base::BindLambdaForTesting(
        [job_name_to_fail](const std::string& job_name,
                           const std::vector<std::string>& env) {
          // Return success unless |job_name| is |job_name_to_fail|.
          return job_name != job_name_to_fail;
        }));
  }

  void InjectUpstartStopJobFailure(const std::string& job_name_to_fail) {
    auto* upstart_client = chromeos::FakeUpstartClient::Get();
    upstart_client->set_stop_job_cb(base::BindLambdaForTesting(
        [job_name_to_fail](const std::string& job_name,
                           const std::vector<std::string>& env) {
          // Return success unless |job_name| is |job_name_to_fail|.
          return job_name != job_name_to_fail;
        }));
  }

  void StartRecordingUpstartOperations() {
    auto* upstart_client = chromeos::FakeUpstartClient::Get();
    upstart_client->set_start_job_cb(
        base::BindLambdaForTesting([this](const std::string& job_name,
                                          const std::vector<std::string>& env) {
          upstart_operations_.emplace_back(job_name, true);
          return true;
        }));
    upstart_client->set_stop_job_cb(
        base::BindLambdaForTesting([this](const std::string& job_name,
                                          const std::vector<std::string>& env) {
          upstart_operations_.emplace_back(job_name, false);
          return true;
        }));
  }

  void RecreateRunLoop() { run_loop_ = std::make_unique<base::RunLoop>(); }

  base::RunLoop* run_loop() { return run_loop_.get(); }

  const std::vector<std::pair<std::string, bool>>& upstart_operations() const {
    return upstart_operations_;
  }

 private:
  void RemoveUpstartStartStopJobFailures() {
    auto* upstart_client = chromeos::FakeUpstartClient::Get();
    upstart_client->set_start_job_cb(
        chromeos::FakeUpstartClient::StartStopJobCallback());
    upstart_client->set_stop_job_cb(
        chromeos::FakeUpstartClient::StartStopJobCallback());
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  base::test::TaskEnvironment task_environment_;

  // List of upstart operations recorded. When it's "start" the boolean is set
  // to true.
  std::vector<std::pair<std::string, bool>> upstart_operations_;
};

TEST_F(ArcUtilTest, IsArcAvailable_None) {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  command_line->InitFromArgv({"", "--arc-availability=none"});
  EXPECT_FALSE(IsArcAvailable());

  // If --arc-availability flag is set to "none", even if Finch experiment is
  // turned on, ARC cannot be used.
  {
    ScopedArcFeature feature(true);
    EXPECT_FALSE(IsArcAvailable());
  }
}

// Test --arc-available with EnableARC feature combination.
TEST_F(ArcUtilTest, IsArcAvailable_Installed) {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  // If ARC is not installed, IsArcAvailable() should return false,
  // regardless of EnableARC feature.
  command_line->InitFromArgv({""});

  // Not available, by-default.
  EXPECT_FALSE(IsArcAvailable());
  EXPECT_FALSE(IsArcKioskAvailable());

  {
    ScopedArcFeature feature(true);
    EXPECT_FALSE(IsArcAvailable());
    EXPECT_FALSE(IsArcKioskAvailable());
  }
  {
    ScopedArcFeature feature(false);
    EXPECT_FALSE(IsArcAvailable());
    EXPECT_FALSE(IsArcKioskAvailable());
  }

  // If ARC is installed, IsArcAvailable() should return true when EnableARC
  // feature is set.
  command_line->InitFromArgv({"", "--arc-available"});

  // Not available, by-default, too.
  EXPECT_FALSE(IsArcAvailable());

  // ARC is available in kiosk mode if installed.
  EXPECT_TRUE(IsArcKioskAvailable());

  {
    ScopedArcFeature feature(true);
    EXPECT_TRUE(IsArcAvailable());
    EXPECT_TRUE(IsArcKioskAvailable());
  }
  {
    ScopedArcFeature feature(false);
    EXPECT_FALSE(IsArcAvailable());
    EXPECT_TRUE(IsArcKioskAvailable());
  }

  // If ARC is installed, IsArcAvailable() should return true when EnableARC
  // feature is set.
  command_line->InitFromArgv({"", "--arc-availability=installed"});

  // Not available, by-default, too.
  EXPECT_FALSE(IsArcAvailable());

  // ARC is available in kiosk mode if installed.
  EXPECT_TRUE(IsArcKioskAvailable());

  {
    ScopedArcFeature feature(true);
    EXPECT_TRUE(IsArcAvailable());
    EXPECT_TRUE(IsArcKioskAvailable());
  }
  {
    ScopedArcFeature feature(false);
    EXPECT_FALSE(IsArcAvailable());
    EXPECT_TRUE(IsArcKioskAvailable());
  }
}

TEST_F(ArcUtilTest, IsArcAvailable_OfficiallySupported) {
  // Regardless of FeatureList, IsArcAvailable() should return true.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--enable-arc"});
  EXPECT_TRUE(IsArcAvailable());
  EXPECT_TRUE(IsArcKioskAvailable());

  command_line->InitFromArgv({"", "--arc-availability=officially-supported"});
  EXPECT_TRUE(IsArcAvailable());
  EXPECT_TRUE(IsArcKioskAvailable());
}

TEST_F(ArcUtilTest, IsArcVmEnabled) {
  EXPECT_FALSE(IsArcVmEnabled());

  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--enable-arcvm"});
  EXPECT_TRUE(IsArcVmEnabled());
}

TEST_F(ArcUtilTest, IsArcVmRtVcpuEnabled) {
  {
    ScopedRtVcpuFeature feature(false, false);
    EXPECT_FALSE(IsArcVmRtVcpuEnabled(2));
    EXPECT_FALSE(IsArcVmRtVcpuEnabled(4));
    EXPECT_FALSE(IsArcVmRtVcpuEnabled(8));
  }
  {
    ScopedRtVcpuFeature feature(true, false);
    EXPECT_TRUE(IsArcVmRtVcpuEnabled(2));
    EXPECT_FALSE(IsArcVmRtVcpuEnabled(4));
    EXPECT_FALSE(IsArcVmRtVcpuEnabled(8));
  }
  {
    ScopedRtVcpuFeature feature(false, true);
    EXPECT_FALSE(IsArcVmRtVcpuEnabled(2));
    EXPECT_TRUE(IsArcVmRtVcpuEnabled(4));
    EXPECT_TRUE(IsArcVmRtVcpuEnabled(8));
  }
  {
    ScopedRtVcpuFeature feature(true, true);
    EXPECT_TRUE(IsArcVmRtVcpuEnabled(2));
    EXPECT_TRUE(IsArcVmRtVcpuEnabled(4));
    EXPECT_TRUE(IsArcVmRtVcpuEnabled(8));
  }
}

TEST_F(ArcUtilTest, IsArcVmUseHugePages) {
  EXPECT_FALSE(IsArcVmUseHugePages());

  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arcvm-use-hugepages"});
  EXPECT_TRUE(IsArcVmUseHugePages());
}

TEST_F(ArcUtilTest, IsArcVmDevConfIgnored) {
  EXPECT_FALSE(IsArcVmDevConfIgnored());

  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--ignore-arcvm-dev-conf"});
  EXPECT_TRUE(IsArcVmDevConfIgnored());
}

TEST_F(ArcUtilTest, GetArcVmUreadaheadMode) {
  constexpr char kArcMemProfile4GbName[] = "4G";
  constexpr char kArcMemProfile8GbName[] = "8G";
  auto callback_disabled = base::BindRepeating(&GetSystemMemoryInfoForTesting,
                                               kArcMemProfile4GbName);
  auto callback_readahead = base::BindRepeating(&GetSystemMemoryInfoForTesting,
                                                kArcMemProfile8GbName);
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({""});
  EXPECT_EQ(ArcVmUreadaheadMode::READAHEAD,
            GetArcVmUreadaheadMode(callback_readahead));
  EXPECT_EQ(ArcVmUreadaheadMode::DISABLED,
            GetArcVmUreadaheadMode(callback_disabled));

  command_line->InitFromArgv({"", "--arcvm-ureadahead-mode=generate"});
  EXPECT_EQ(ArcVmUreadaheadMode::GENERATE,
            GetArcVmUreadaheadMode(callback_readahead));

  command_line->InitFromArgv({"", "--arcvm-ureadahead-mode=disabled"});
  EXPECT_EQ(ArcVmUreadaheadMode::DISABLED,
            GetArcVmUreadaheadMode(callback_readahead));
}

// TODO(hidehiko): Add test for IsArcKioskMode().
// It depends on UserManager, but a utility to inject fake instance is
// available only in chrome/. To use it in components/, refactoring is needed.

TEST_F(ArcUtilTest, IsArcOptInVerificationDisabled) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({""});
  EXPECT_FALSE(IsArcOptInVerificationDisabled());

  command_line->InitFromArgv({"", "--disable-arc-opt-in-verification"});
  EXPECT_TRUE(IsArcOptInVerificationDisabled());
}

TEST_F(ArcUtilTest, IsArcAllowedForUser) {
  user_manager::FakeUserManager* fake_user_manager =
      new user_manager::FakeUserManager();
  user_manager::ScopedUserManager scoped_user_manager(
      base::WrapUnique(fake_user_manager));
  TestingPrefServiceSimple local_state;
  fake_user_manager->set_local_state(&local_state);

  struct {
    user_manager::UserType user_type;
    bool expected_allowed;
  } const kTestCases[] = {
      {user_manager::USER_TYPE_REGULAR, true},
      {user_manager::USER_TYPE_GUEST, false},
      {user_manager::USER_TYPE_PUBLIC_ACCOUNT, true},
      {user_manager::USER_TYPE_SUPERVISED_DEPRECATED, false},
      {user_manager::USER_TYPE_KIOSK_APP, false},
      {user_manager::USER_TYPE_CHILD, true},
      {user_manager::USER_TYPE_ARC_KIOSK_APP, true},
      {user_manager::USER_TYPE_ACTIVE_DIRECTORY, true},
  };
  for (const auto& test_case : kTestCases) {
    const FakeUser user(test_case.user_type);
    EXPECT_EQ(test_case.expected_allowed, IsArcAllowedForUser(&user))
        << "User type=" << test_case.user_type;
  }

  // An ephemeral user is a logged in user but unknown to UserManager when
  // ephemeral policy is set.
  fake_user_manager->SetEphemeralUsersEnabled(true);
  fake_user_manager->UserLoggedIn(
      AccountId::FromUserEmailGaiaId("test@test.com", "9876543210"),
      "test@test.com-hash", false /* browser_restart */, false /* is_child */);
  const user_manager::User* ephemeral_user = fake_user_manager->GetActiveUser();
  ASSERT_TRUE(ephemeral_user);
  ASSERT_TRUE(fake_user_manager->IsUserCryptohomeDataEphemeral(
      ephemeral_user->GetAccountId()));

  // Ephemeral user is also allowed for ARC.
  EXPECT_TRUE(IsArcAllowedForUser(ephemeral_user));
}

TEST_F(ArcUtilTest, ArcStartModeDefault) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arc-availability=installed"});
  EXPECT_FALSE(ShouldArcAlwaysStart());
  EXPECT_FALSE(ShouldArcAlwaysStartWithNoPlayStore());
}

TEST_F(ArcUtilTest, ArcStartModeWithoutPlayStore) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv(
      {"", "--arc-availability=installed",
       "--arc-start-mode=always-start-with-no-play-store"});
  EXPECT_TRUE(ShouldArcAlwaysStart());
  EXPECT_TRUE(ShouldArcAlwaysStartWithNoPlayStore());
}

TEST_F(ArcUtilTest, ScaleFactorToDensity) {
  // Test all standard scale factors
  EXPECT_EQ(160, GetLcdDensityForDeviceScaleFactor(1.0f));
  EXPECT_EQ(160, GetLcdDensityForDeviceScaleFactor(1.25f));
  EXPECT_EQ(213, GetLcdDensityForDeviceScaleFactor(1.6f));
  EXPECT_EQ(240, GetLcdDensityForDeviceScaleFactor(display::kDsf_1_777));
  EXPECT_EQ(240, GetLcdDensityForDeviceScaleFactor(display::kDsf_1_8));
  EXPECT_EQ(240, GetLcdDensityForDeviceScaleFactor(2.0f));
  EXPECT_EQ(280, GetLcdDensityForDeviceScaleFactor(display::kDsf_2_252));
  EXPECT_EQ(280, GetLcdDensityForDeviceScaleFactor(2.4f));
  EXPECT_EQ(320, GetLcdDensityForDeviceScaleFactor(display::kDsf_2_666));

  // Bad scale factors shouldn't blow up.
  EXPECT_EQ(160, GetLcdDensityForDeviceScaleFactor(0.5f));
  EXPECT_EQ(160, GetLcdDensityForDeviceScaleFactor(-0.1f));
  EXPECT_EQ(180, GetLcdDensityForDeviceScaleFactor(1.5f));
  EXPECT_EQ(1200, GetLcdDensityForDeviceScaleFactor(10.f));

  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--arc-scale=280"});
  EXPECT_EQ(280, GetLcdDensityForDeviceScaleFactor(1.234f));

  command_line->InitFromArgv({"", "--arc-scale=120"});
  EXPECT_EQ(120, GetLcdDensityForDeviceScaleFactor(1.234f));

  command_line->InitFromArgv({"", "--arc-scale=abc"});
  EXPECT_EQ(240, GetLcdDensityForDeviceScaleFactor(2.0));
}

TEST_F(ArcUtilTest, ConfigureUpstartJobs_Success) {
  std::deque<JobDesc> jobs{
      JobDesc{"Job_2dA", UpstartOperation::JOB_STOP, {}},
      JobDesc{"Job_2dB", UpstartOperation::JOB_STOP_AND_START, {}},
      JobDesc{"Job_2dC", UpstartOperation::JOB_START, {}},
  };
  bool result = false;
  StartRecordingUpstartOperations();
  ConfigureUpstartJobs(jobs,
                       base::BindLambdaForTesting([&result, this](bool r) {
                         result = r;
                         run_loop()->Quit();
                       }));
  run_loop()->Run();
  EXPECT_TRUE(result);

  auto ops = upstart_operations();
  ASSERT_EQ(4u, ops.size());
  EXPECT_EQ(ops[0].first, "Job_2dA");
  EXPECT_FALSE(ops[0].second);
  EXPECT_EQ(ops[1].first, "Job_2dB");
  EXPECT_FALSE(ops[1].second);
  EXPECT_EQ(ops[2].first, "Job_2dB");
  EXPECT_TRUE(ops[2].second);
  EXPECT_EQ(ops[3].first, "Job_2dC");
  EXPECT_TRUE(ops[3].second);
}

TEST_F(ArcUtilTest, ConfigureUpstartJobs_StopFail) {
  std::deque<JobDesc> jobs{
      JobDesc{"Job_2dA", UpstartOperation::JOB_STOP, {}},
      JobDesc{"Job_2dB", UpstartOperation::JOB_STOP_AND_START, {}},
      JobDesc{"Job_2dC", UpstartOperation::JOB_START, {}},
  };
  // Confirm that failing to stop a job is ignored.
  bool result = false;
  InjectUpstartStopJobFailure("Job_2dA");
  ConfigureUpstartJobs(jobs,
                       base::BindLambdaForTesting([&result, this](bool r) {
                         result = r;
                         run_loop()->Quit();
                       }));
  run_loop()->Run();
  EXPECT_TRUE(result);

  // Do the same for the second task.
  RecreateRunLoop();
  result = false;
  InjectUpstartStopJobFailure("Job_2dB");
  ConfigureUpstartJobs(jobs,
                       base::BindLambdaForTesting([&result, this](bool r) {
                         result = r;
                         run_loop()->Quit();
                       }));
  run_loop()->Run();
  EXPECT_TRUE(result);
}

TEST_F(ArcUtilTest, ConfigureUpstartJobs_StartFail) {
  std::deque<JobDesc> jobs{
      JobDesc{"Job_2dA", UpstartOperation::JOB_STOP, {}},
      JobDesc{"Job_2dB", UpstartOperation::JOB_STOP_AND_START, {}},
      JobDesc{"Job_2dC", UpstartOperation::JOB_START, {}},
  };
  // Confirm that failing to start a job is not ignored.
  bool result = true;
  InjectUpstartStartJobFailure("Job_2dB");
  ConfigureUpstartJobs(jobs,
                       base::BindLambdaForTesting([&result, this](bool r) {
                         result = r;
                         run_loop()->Quit();
                       }));
  run_loop()->Run();
  EXPECT_FALSE(result);

  // Do the same for the third task.
  RecreateRunLoop();
  result = true;
  InjectUpstartStartJobFailure("Job_2dC");
  ConfigureUpstartJobs(std::move(jobs),
                       base::BindLambdaForTesting([&result, this](bool r) {
                         result = r;
                         run_loop()->Quit();
                       }));
  run_loop()->Run();
  EXPECT_FALSE(result);
}

}  // namespace
}  // namespace arc
