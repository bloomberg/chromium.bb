// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/child_accounts/child_account_test_utils.h"
#include "chrome/browser/chromeos/child_accounts/screen_time_controller.h"
#include "chrome/browser/chromeos/child_accounts/screen_time_controller_factory.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_override.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_test_utils.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/lock/screen_locker_tester.h"
#include "chrome/browser/chromeos/policy/login_policy_test_base.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

// Time delta representing the usage time limit warning time.
constexpr base::TimeDelta kUsageTimeLimitWarningTime =
    base::TimeDelta::FromMinutes(15);

class TestScreenTimeControllerObserver : public ScreenTimeController::Observer {
 public:
  TestScreenTimeControllerObserver() = default;
  ~TestScreenTimeControllerObserver() override = default;

  int usage_time_limit_warnings() const { return usage_time_limit_warnings_; }

 private:
  void UsageTimeLimitWarning() override { usage_time_limit_warnings_++; }

  int usage_time_limit_warnings_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestScreenTimeControllerObserver);
};

}  // namespace

namespace utils = time_limit_test_utils;

// Allows testing ScreenTimeController with UsageTimeStateNotifier enabled
// (instantiated with |true|) or disabled (instantiated with |false|).
class ScreenTimeControllerTest : public policy::LoginPolicyTestBase,
                                 public testing::WithParamInterface<bool> {
 public:
  ScreenTimeControllerTest() = default;

  ~ScreenTimeControllerTest() override = default;

  // policy::LoginPolicyTestBase:
  void SetUp() override {
    is_feature_enabled_ = GetParam();
    base::test::ScopedFeatureList feature_list;
    if (is_feature_enabled_) {
      feature_list.InitAndEnableFeature(features::kUsageTimeStateNotifier);
    } else {
      feature_list.InitAndDisableFeature(features::kUsageTimeStateNotifier);
    }

    // Recognize example.com (used by LoginPolicyTestBase) as non-enterprise
    // account.
    policy::BrowserPolicyConnector::SetNonEnterpriseDomainForTesting(
        "example.com");

    policy::LoginPolicyTestBase::SetUp();
  }

  void GetMandatoryPoliciesValue(base::DictionaryValue* policy) const override {
    // A basic starting policy.
    base::Value policy_content =
        utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
    policy->SetKey(policy::key::kUsageTimeLimit,
                   base::Value(utils::PolicyToString(policy_content)));
  }

  std::string GetIdToken() const override {
    return test::GetChildAccountOAuthIdToken();
  }

 protected:
  void LogInChildAndSetupClockWithTime(const char* time) {
    SetupTaskRunnerWithTime(utils::TimeFromString(time));
    SkipToLoginScreen();
    LogIn(kAccountId, kAccountPassword, test::kChildAccountServiceFlags);
    MockClockForActiveUser();
  }

  void SetupTaskRunnerWithTime(base::Time start_time) {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        start_time, base::TimeTicks::UnixEpoch());
  }

  void MockClockForActiveUser() {
    const user_manager::UserManager* const user_manager =
        user_manager::UserManager::Get();
    EXPECT_EQ(user_manager->GetActiveUser()->GetType(),
              user_manager::USER_TYPE_CHILD);
    child_profile_ =
        ProfileHelper::Get()->GetProfileByUser(user_manager->GetActiveUser());

    // Mock time for ScreenTimeController.
    ScreenTimeControllerFactory::GetForBrowserContext(child_profile_)
        ->SetClocksForTesting(task_runner_->GetMockClock(),
                              task_runner_->GetMockTickClock(), task_runner_);
  }

  bool IsAuthEnabled() {
    return ScreenLocker::default_screen_locker()->IsAuthEnabledForUser(
        AccountId::FromUserEmail(kAccountId));
  }

  void MockChildScreenTime(base::TimeDelta used_time) {
    Profile::FromBrowserContext(child_profile_)
        ->GetPrefs()
        ->SetInteger(prefs::kChildScreenTimeMilliseconds,
                     used_time.InMilliseconds());
  }

  bool IsLocked() {
    base::RunLoop().RunUntilIdle();
    return session_manager::SessionManager::Get()->IsScreenLocked();
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  Profile* child_profile_ = nullptr;
  bool is_feature_enabled_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenTimeControllerTest);
};

// Tests a simple lock override.
IN_PROC_BROWSER_TEST_P(ScreenTimeControllerTest, LockOverride) {
  LogInChildAndSetupClockWithTime("1 Jan 2018 10:00:00 GMT");
  ScreenLockerTester().Lock();

  // Verify user is able to log in.
  EXPECT_TRUE(IsAuthEnabled());

  // Wait one hour.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
  EXPECT_TRUE(IsAuthEnabled());

  // Set new policy.
  base::Value policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddOverride(&policy_content,
                     usage_time_limit::TimeLimitOverride::Action::kLock,
                     task_runner_->Now());

  base::Value policy(base::Value::Type::DICTIONARY);
  policy.SetKey(policy::key::kUsageTimeLimit,
                base::Value(utils::PolicyToString(policy_content)));

  user_policy_helper()->SetPolicyAndWait(
      policy, base::Value(base::Value::Type::DICTIONARY), child_profile_);

  EXPECT_FALSE(IsAuthEnabled());
}

// Tests an unlock override on a bedtime.
IN_PROC_BROWSER_TEST_P(ScreenTimeControllerTest, UnlockBedtime) {
  LogInChildAndSetupClockWithTime("5 Jan 2018 22:00:00 BRT");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("BRT"));

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 BRT");
  base::Value policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(&policy_content, utils::kFriday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kSaturday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  base::Value bedtime_policy(base::Value::Type::DICTIONARY);
  bedtime_policy.SetKey(policy::key::kUsageTimeLimit,
                        base::Value(utils::PolicyToString(policy_content)));
  user_policy_helper()->SetPolicyAndWait(
      bedtime_policy, base::Value(base::Value::Type::DICTIONARY),
      child_profile_);

  // Check that auth is disabled, since the bedtime has already started.
  EXPECT_FALSE(IsAuthEnabled());

  // Create unlock override and update the policy.
  utils::AddOverride(&policy_content,
                     usage_time_limit::TimeLimitOverride::Action::kUnlock,
                     task_runner_->Now());
  base::Value unlock_override_policy(base::Value::Type::DICTIONARY);
  unlock_override_policy.SetKey(
      policy::key::kUsageTimeLimit,
      base::Value(utils::PolicyToString(policy_content)));
  user_policy_helper()->SetPolicyAndWait(
      unlock_override_policy, base::Value(base::Value::Type::DICTIONARY),
      child_profile_);

  // Check that the unlock worked and auth is enabled.
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 6 AM and check that auth is still enabled.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(8));
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 9 PM and check that auth is disabled because bedtime started.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(15));
  EXPECT_FALSE(IsAuthEnabled());
}

// Tests an override with duration on a bedtime before it's locked.
IN_PROC_BROWSER_TEST_P(ScreenTimeControllerTest, OverrideBedtimeWithDuration) {
  LogInChildAndSetupClockWithTime("5 Jan 2018 20:45:00 PST");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("PST"));

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 PST");
  base::Value policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(&policy_content, utils::kFriday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kSaturday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  base::Value bedtime_policy(base::Value::Type::DICTIONARY);
  bedtime_policy.SetKey(policy::key::kUsageTimeLimit,
                        base::Value(utils::PolicyToString(policy_content)));
  user_policy_helper()->SetPolicyAndWait(
      bedtime_policy, base::Value(base::Value::Type::DICTIONARY),
      child_profile_);

  // Check that auth is enable, since the bedtime hasn't started.
  EXPECT_TRUE(IsAuthEnabled());

  // Create unlock override with a duration of 2 hours and update the policy.
  utils::AddOverrideWithDuration(
      &policy_content, usage_time_limit::TimeLimitOverride::Action::kUnlock,
      task_runner_->Now(), base::TimeDelta::FromHours(2));
  base::Value unlock_override_policy(base::Value::Type::DICTIONARY);
  unlock_override_policy.SetKey(
      policy::key::kUsageTimeLimit,
      base::Value(utils::PolicyToString(policy_content)));
  user_policy_helper()->SetPolicyAndWait(
      unlock_override_policy, base::Value(base::Value::Type::DICTIONARY),
      child_profile_);

  // Check that the unlock worked and auth is enabled.
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 10:15 PM and check that auth is still enabled.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(90));
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 10:45 PM and check that auth is disabled because the duration is
  // over.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(30));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 11 PM and check that auth is still disabled.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(15));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 6 AM and check that auth is still disabled.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(7));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 7 AM and check that auth is enable because bedtime is finished.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 9 PM and check that auth is disabled because bedtime started.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(14));
  EXPECT_FALSE(IsAuthEnabled());
}

// Tests an override with duration on a daily limit before it's locked.
IN_PROC_BROWSER_TEST_P(ScreenTimeControllerTest,
                       OverrideDailyLimitWithDuration) {
  LogInChildAndSetupClockWithTime("1 Jan 2018 10:00:00 BRT");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("BRT"));

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 BRT");
  base::Value policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(&policy_content, utils::kMonday,
                           base::TimeDelta::FromHours(2), last_updated);
  base::Value daily_limit_policy(base::Value::Type::DICTIONARY);
  daily_limit_policy.SetKey(policy::key::kUsageTimeLimit,
                            base::Value(utils::PolicyToString(policy_content)));
  user_policy_helper()->SetPolicyAndWait(
      daily_limit_policy, base::Value(base::Value::Type::DICTIONARY),
      child_profile_);

  // Check that auth is enabled at 10 AM with 0 usage time.
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 12 PM with 1:50 hours of usage time.
  MockChildScreenTime(base::TimeDelta::FromMinutes(110));
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(2));
  EXPECT_TRUE(IsAuthEnabled());

  // Create unlock override with a duration of 1 hour and update the policy.
  utils::AddOverrideWithDuration(
      &policy_content, usage_time_limit::TimeLimitOverride::Action::kUnlock,
      task_runner_->Now(), base::TimeDelta::FromHours(1));
  base::Value unlock_override_policy(base::Value::Type::DICTIONARY);
  unlock_override_policy.SetKey(
      policy::key::kUsageTimeLimit,
      base::Value(utils::PolicyToString(policy_content)));
  user_policy_helper()->SetPolicyAndWait(
      unlock_override_policy, base::Value(base::Value::Type::DICTIONARY),
      child_profile_);

  // Check that the unlock worked and auth is enabled.
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 12:30 PM with 2:20 hours of usage time and check that auth is
  // still enabled.
  MockChildScreenTime(base::TimeDelta::FromMinutes(140));
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(30));
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 1 PM and check that auth is disabled because the duration is
  // over.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(30));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 5 AM and check that auth is still disabled.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(16));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 6 AM and check that auth is enabled.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
  EXPECT_TRUE(IsAuthEnabled());
}

// Tests an unlock override with duration on a bedtime.
IN_PROC_BROWSER_TEST_P(ScreenTimeControllerTest, UnlockBedtimeWithDuration) {
  LogInChildAndSetupClockWithTime("5 Jan 2018 22:00:00 GMT");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("GMT"));

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 GMT");
  base::Value policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(&policy_content, utils::kFriday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kSaturday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  base::Value bedtime_policy(base::Value::Type::DICTIONARY);
  bedtime_policy.SetKey(policy::key::kUsageTimeLimit,
                        base::Value(utils::PolicyToString(policy_content)));
  user_policy_helper()->SetPolicyAndWait(
      bedtime_policy, base::Value(base::Value::Type::DICTIONARY),
      child_profile_);

  // Check that auth is disabled, since the bedtime has already started.
  EXPECT_FALSE(IsAuthEnabled());

  // Create unlock override with a duration of 2 hours and update the policy.
  utils::AddOverrideWithDuration(
      &policy_content, usage_time_limit::TimeLimitOverride::Action::kUnlock,
      task_runner_->Now(), base::TimeDelta::FromHours(2));
  base::Value unlock_override_policy(base::Value::Type::DICTIONARY);
  unlock_override_policy.SetKey(
      policy::key::kUsageTimeLimit,
      base::Value(utils::PolicyToString(policy_content)));
  user_policy_helper()->SetPolicyAndWait(
      unlock_override_policy, base::Value(base::Value::Type::DICTIONARY),
      child_profile_);

  // Check that the unlock worked and auth is enabled.
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 11:30 PM and check that auth is still enabled.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(90));
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 12 AM and check that auth is disabled because the duration is
  // over.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(30));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 6 AM and check that auth is still disabled because bedtime ends
  // at 7 AM.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(6));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 7 AM and check that auth is enable because bedtime is finished.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 9 PM and check that auth is disabled because bedtime started.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(14));
  EXPECT_FALSE(IsAuthEnabled());
}

// Tests an unlock override with duration on a daily limit.
IN_PROC_BROWSER_TEST_P(ScreenTimeControllerTest, UnlockDailyLimitWithDuration) {
  LogInChildAndSetupClockWithTime("1 Jan 2018 10:00:00 PST");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("PST"));

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 PST");
  base::Value policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(&policy_content, utils::kMonday,
                           base::TimeDelta::FromHours(2), last_updated);
  base::Value daily_limit_policy(base::Value::Type::DICTIONARY);
  daily_limit_policy.SetKey(policy::key::kUsageTimeLimit,
                            base::Value(utils::PolicyToString(policy_content)));
  user_policy_helper()->SetPolicyAndWait(
      daily_limit_policy, base::Value(base::Value::Type::DICTIONARY),
      child_profile_);

  // Check that auth is enabled at 10 AM with 0 usage time.
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 12 PM with 2 hours of usage time and check if auth is disabled.
  MockChildScreenTime(base::TimeDelta::FromHours(2));
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(2));
  EXPECT_FALSE(IsAuthEnabled());

  // Create unlock override with a duration of 1 hour and update the policy.
  utils::AddOverrideWithDuration(
      &policy_content, usage_time_limit::TimeLimitOverride::Action::kUnlock,
      task_runner_->Now(), base::TimeDelta::FromHours(1));
  base::Value unlock_override_policy(base::Value::Type::DICTIONARY);
  unlock_override_policy.SetKey(
      policy::key::kUsageTimeLimit,
      base::Value(utils::PolicyToString(policy_content)));
  user_policy_helper()->SetPolicyAndWait(
      unlock_override_policy, base::Value(base::Value::Type::DICTIONARY),
      child_profile_);

  // Check that the unlock worked and auth is enabled.
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 12:30 PM with 2:30 hours of usage time and check that auth is
  // still enabled.
  MockChildScreenTime(base::TimeDelta::FromMinutes(150));
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(30));
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 1 PM and check that auth is disabled because the duration is
  // over.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(30));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 5 AM and check that auth is still disabled.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(16));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 6 AM and check that auth is enabled.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
  EXPECT_TRUE(IsAuthEnabled());
}

// Tests the default time window limit.
IN_PROC_BROWSER_TEST_P(ScreenTimeControllerTest, DefaultBedtime) {
  LogInChildAndSetupClockWithTime("1 Jan 2018 10:00:00 GMT");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("GMT"));

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 GMT");
  base::Value policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(&policy_content, utils::kMonday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kTuesday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kWednesday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kThursday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kFriday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kSaturday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kSunday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);

  base::Value policy(base::Value::Type::DICTIONARY);
  policy.SetKey(policy::key::kUsageTimeLimit,
                base::Value(utils::PolicyToString(policy_content)));

  user_policy_helper()->SetPolicyAndWait(
      policy, base::Value(base::Value::Type::DICTIONARY), child_profile_);

  // Iterate over a week checking that the device is locked properly everyday.
  for (int i = 0; i < 7; i++) {
    // Verify that auth is enabled at 10 AM.
    EXPECT_TRUE(IsAuthEnabled());

    // Verify that auth is enabled at 8 PM.
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(10));
    EXPECT_TRUE(IsAuthEnabled());

    // Verify that the auth was disabled at 9 PM (start of bedtime).
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
    EXPECT_FALSE(IsAuthEnabled());

    // Forward to 7 AM and check that auth was re-enabled (end of bedtime).
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(10));
    EXPECT_TRUE(IsAuthEnabled());

    // Forward to 10 AM.
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(3));
  }
}

// Tests the default time window limit.
IN_PROC_BROWSER_TEST_P(ScreenTimeControllerTest, DefaultDailyLimit) {
  LogInChildAndSetupClockWithTime("1 Jan 2018 10:00:00 GMT");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("GMT"));

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 GMT");
  base::Value policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(&policy_content, utils::kMonday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(&policy_content, utils::kTuesday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(&policy_content, utils::kWednesday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(&policy_content, utils::kThursday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(&policy_content, utils::kFriday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(&policy_content, utils::kSaturday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(&policy_content, utils::kSunday,
                           base::TimeDelta::FromHours(3), last_updated);

  base::Value policy(base::Value::Type::DICTIONARY);
  policy.SetKey(policy::key::kUsageTimeLimit,
                base::Value(utils::PolicyToString(policy_content)));

  user_policy_helper()->SetPolicyAndWait(
      policy, base::Value(base::Value::Type::DICTIONARY), child_profile_);

  // Iterate over a week checking that the device is locked properly
  // every day.
  for (int i = 0; i < 7; i++) {
    // Check that auth is enabled at 10 AM with 0 usage time.
    EXPECT_TRUE(IsAuthEnabled());

    // Check that auth is enabled after forwarding to 1 PM and using the device
    // for 2 hours.
    MockChildScreenTime(base::TimeDelta::FromHours(2));
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(3));
    EXPECT_TRUE(IsAuthEnabled());

    // Check that auth is enabled after forwarding to 2 PM with no extra usage.
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
    EXPECT_TRUE(IsAuthEnabled());

    // Check that auth is disabled after forwarding to 3 PM and using the device
    // for 3 hours.
    MockChildScreenTime(base::TimeDelta::FromHours(3));
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
    EXPECT_FALSE(IsAuthEnabled());

    // Forward to 6 AM, reset the usage time and check that auth was re-enabled.
    MockChildScreenTime(base::TimeDelta::FromHours(0));
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(15));
    EXPECT_TRUE(IsAuthEnabled());

    // Forward to 10 AM.
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(4));
  }
}

// Tests that the bedtime locks an active session when it is reached.
IN_PROC_BROWSER_TEST_P(ScreenTimeControllerTest, ActiveSessionBedtime) {
  LogInChildAndSetupClockWithTime("1 Jan 2018 10:00:00 PST");

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("PST"));

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 PST");
  base::Value policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(&policy_content, utils::kMonday,
                            utils::CreateTime(23, 0), utils::CreateTime(8, 0),
                            last_updated);

  base::Value policy(base::Value::Type::DICTIONARY);
  policy.SetKey(policy::key::kUsageTimeLimit,
                base::Value(utils::PolicyToString(policy_content)));

  user_policy_helper()->SetPolicyAndWait(
      policy, base::Value(base::Value::Type::DICTIONARY), child_profile_);

  // Verify that device is unlocked at 10 AM.
  EXPECT_FALSE(IsLocked());

  // Verify that device is still unlocked at 10 PM.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(12));
  EXPECT_FALSE(IsLocked());

  // Verify that device is locked at 11 PM (start of bedtime).
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
  EXPECT_TRUE(IsLocked());

  // Forward to 8 AM and check that auth was re-enabled (end of bedtime).
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(9));
  EXPECT_TRUE(IsAuthEnabled());
}

// Tests that the daily limit locks the device when it is reached.
IN_PROC_BROWSER_TEST_P(ScreenTimeControllerTest, ActiveSessionDailyLimit) {
  LogInChildAndSetupClockWithTime("1 Jan 2018 10:00:00 PST");

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("PST"));

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 PST");
  base::Value policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(&policy_content, utils::kMonday,
                           base::TimeDelta::FromHours(1), last_updated);

  base::Value policy(base::Value::Type::DICTIONARY);
  policy.SetKey(policy::key::kUsageTimeLimit,
                base::Value(utils::PolicyToString(policy_content)));

  user_policy_helper()->SetPolicyAndWait(
      policy, base::Value(base::Value::Type::DICTIONARY), child_profile_);

  // Verify that device is unlocked at 10 AM.
  EXPECT_FALSE(IsLocked());

  // Forward 1 hour to 11 AM and add 1 hour of usage and verify that device is
  // locked (start of daily limit).
  MockChildScreenTime(base::TimeDelta::FromHours(1));
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
  EXPECT_TRUE(IsLocked());

  // Forward to 6 AM, reset the usage time and check that auth was re-enabled.
  MockChildScreenTime(base::TimeDelta::FromHours(0));
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(19));
  EXPECT_TRUE(IsAuthEnabled());
}

// Tests bedtime during timezone changes.
IN_PROC_BROWSER_TEST_P(ScreenTimeControllerTest, BedtimeOnTimezoneChange) {
  LogInChildAndSetupClockWithTime("3 Jan 2018 10:00:00 GMT-0600");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("GMT-0600"));

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("3 Jan 2018 0:00 GMT-0600");
  base::Value policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(&policy_content, utils::kWednesday,
                            utils::CreateTime(19, 0), utils::CreateTime(7, 0),
                            last_updated);

  base::Value policy(base::Value::Type::DICTIONARY);
  policy.SetKey(policy::key::kUsageTimeLimit,
                base::Value(utils::PolicyToString(policy_content)));

  user_policy_helper()->SetPolicyAndWait(
      policy, base::Value(base::Value::Type::DICTIONARY), child_profile_);

  // Verify that auth is enabled at 10 AM.
  EXPECT_TRUE(IsAuthEnabled());

  // Verify that auth is enabled at 6 PM.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(8));
  EXPECT_TRUE(IsAuthEnabled());

  // Verify that the auth is disabled at 7 PM (start of bedtime).
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
  EXPECT_FALSE(IsAuthEnabled());

  // Change timezone, so that local time goes back to 6 PM and check that auth
  // is enabled since bedtime has not started yet.
  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("GMT-0700"));
  EXPECT_TRUE(IsAuthEnabled());

  // Verify that auth is disabled at 7 PM (start of bedtime).
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
  EXPECT_FALSE(IsAuthEnabled());

  // Change timezone, so that local time goes forward to 7 AM and check that
  // auth is enabled since bedtime has ended in the new local time.
  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("GMT+0500"));
  EXPECT_TRUE(IsAuthEnabled());
}

// Tests bedtime during timezone changes that make the clock go back in time.
IN_PROC_BROWSER_TEST_P(ScreenTimeControllerTest,
                       BedtimeOnEastToWestTimezoneChanges) {
  LogInChildAndSetupClockWithTime("3 Jan 2018 8:00:00 GMT+1300");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("GMT+1300"));

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("3 Jan 2018 0:00 GMT+1300");
  base::Value policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(&policy_content, utils::kTuesday,
                            utils::CreateTime(20, 0), utils::CreateTime(7, 0),
                            last_updated);

  base::Value policy(base::Value::Type::DICTIONARY);
  policy.SetKey(policy::key::kUsageTimeLimit,
                base::Value(utils::PolicyToString(policy_content)));

  user_policy_helper()->SetPolicyAndWait(
      policy, base::Value(base::Value::Type::DICTIONARY), child_profile_);

  // Verify that auth is disabled at 8 AM.
  EXPECT_TRUE(IsAuthEnabled());

  // Change timezone so that local time goes back to 6 AM and check that auth is
  // disable, since the tuesday's bedtime is not over yet.
  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("GMT+1100"));
  EXPECT_FALSE(IsAuthEnabled());

  // Change timezone so that local time goes back to 7 PM on Tuesday and check
  // that auth is enabled, because the bedtime has not started yet in the
  // new local time.
  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("GMT"));
  EXPECT_TRUE(IsAuthEnabled());

  // Verify that auth is disabled at 8 PM (start of bedtime).
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
  EXPECT_FALSE(IsAuthEnabled());
}

// Tests if call the observers for usage time limit warning.
IN_PROC_BROWSER_TEST_P(ScreenTimeControllerTest, CallObservers) {
  if (!is_feature_enabled_)
    return;
  LogInChildAndSetupClockWithTime("1 Jan 2018 10:00:00 PST");

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
      base::UTF8ToUTF16("PST"));

  // Set new policy with 3 hours of time usage limit.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 PST");
  base::Value policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(&policy_content, utils::kMonday,
                           base::TimeDelta::FromHours(3), last_updated);

  base::Value policy(base::Value::Type::DICTIONARY);
  policy.SetKey(policy::key::kUsageTimeLimit,
                base::Value(utils::PolicyToString(policy_content)));

  user_policy_helper()->SetPolicyAndWait(
      policy, base::Value(base::Value::Type::DICTIONARY), child_profile_);

  TestScreenTimeControllerObserver observer;
  ScreenTimeControllerFactory::GetForBrowserContext(child_profile_)
      ->AddObserver(&observer);

  base::TimeDelta current_screen_time;
  base::TimeDelta last_screen_time;

  // Check that observer was not called at 10 AM.
  EXPECT_EQ(0, observer.usage_time_limit_warnings());

  // Check that observer was not called after child used device for 2 hours and
  // forward to 12 AM.
  last_screen_time = base::TimeDelta();
  current_screen_time = base::TimeDelta::FromHours(2);
  MockChildScreenTime(current_screen_time);
  task_runner_->FastForwardBy(current_screen_time - last_screen_time);
  EXPECT_EQ(0, observer.usage_time_limit_warnings());

  // Check that observer was not called after using the device for
  // 3 hours - |kUsageTimeLimitWarningTime| - 1 second. Forward to
  // 1 PM - |kUsageTimeLimitWarningTime| - 1 second.
  last_screen_time = current_screen_time;
  current_screen_time = base::TimeDelta::FromHours(3) -
                        kUsageTimeLimitWarningTime -
                        base::TimeDelta::FromSeconds(1);
  MockChildScreenTime(current_screen_time);
  task_runner_->FastForwardBy(current_screen_time - last_screen_time);
  EXPECT_EQ(0, observer.usage_time_limit_warnings());

  // Check that observer was called after using the device for
  // 3 hours - |kUsageTimeLimitWarningTime| + 1 second. Forward to
  // 1 PM - |kUsageTimeLimitWarningTime| + 1 second.
  last_screen_time = current_screen_time;
  current_screen_time = base::TimeDelta::FromHours(3) -
                        kUsageTimeLimitWarningTime +
                        base::TimeDelta::FromSeconds(1);
  MockChildScreenTime(current_screen_time);
  task_runner_->FastForwardBy(current_screen_time - last_screen_time);
  EXPECT_EQ(1, observer.usage_time_limit_warnings());

  // Check that observer was not called after using the device for 3 hours.
  // Forward to 1 PM.
  last_screen_time = current_screen_time;
  current_screen_time = base::TimeDelta::FromHours(3);
  MockChildScreenTime(current_screen_time);
  task_runner_->FastForwardBy(current_screen_time - last_screen_time);
  EXPECT_EQ(1, observer.usage_time_limit_warnings());

  // Forward to 6 AM, reset the usage time.
  MockChildScreenTime(base::TimeDelta::FromHours(0));
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(17));

  // Forward to 10 AM.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(4));
  EXPECT_EQ(1, observer.usage_time_limit_warnings());

  ScreenTimeControllerFactory::GetForBrowserContext(child_profile_)
      ->RemoveObserver(&observer);
}

INSTANTIATE_TEST_SUITE_P(, ScreenTimeControllerTest, testing::Bool());

}  // namespace chromeos
