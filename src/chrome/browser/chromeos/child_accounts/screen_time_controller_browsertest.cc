// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/login_policy_test_base.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/child_accounts/child_account_test_utils.h"
#include "chrome/browser/chromeos/child_accounts/screen_time_controller.h"
#include "chrome/browser/chromeos/child_accounts/screen_time_controller_factory.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {
namespace {

void WaitForScreenLock() {
  content::WindowedNotificationObserver lock_state_observer(
      chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
      content::NotificationService::AllSources());
  lock_state_observer.Wait();
}

}  // namespace

class ScreenTimeControllerTest : public policy::LoginPolicyTestBase {
 public:
  ScreenTimeControllerTest() {
    base::Time start_time;
    EXPECT_TRUE(base::Time::FromUTCString("1 Jan 2018 10:00:00", &start_time));
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        start_time, base::TimeTicks::UnixEpoch());
  }

  ~ScreenTimeControllerTest() override = default;

  // policy::LoginPolicyTestBase:
  void SetUp() override {
    // Recognize example.com (used by LoginPolicyTestBase) as non-enterprise
    // account.
    policy::BrowserPolicyConnector::SetNonEnterpriseDomainForTesting(
        "example.com");

    policy::LoginPolicyTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    policy::LoginPolicyTestBase::SetUpOnMainThread();

    SkipToLoginScreen();
    LogIn(kAccountId, kAccountPassword, test::kChildAccountServiceFlags);

    const user_manager::UserManager* const user_manager =
        user_manager::UserManager::Get();
    EXPECT_EQ(user_manager->GetActiveUser()->GetType(),
              user_manager::USER_TYPE_CHILD);
    child_profile_ =
        ProfileHelper::Get()->GetProfileByUser(user_manager->GetActiveUser());

    // Mock time for ScreenTimeController.
    ScreenTimeControllerFactory::GetForBrowserContext(child_profile_)
        ->SetClocksForTesting(task_runner_->GetMockClock(),
                              task_runner_->GetMockTickClock());
  }

  void GetMandatoryPoliciesValue(base::DictionaryValue* policy) const override {
    // A basic starting policy.
    constexpr char kUsageTimeLimit[] = R"({
      "time_usage_limit": {
        "reset_at": {
          "hour": 6, "minute": 0
        }
      }
    })";
    policy->SetKey("UsageTimeLimit", base::Value(kUsageTimeLimit));
  }

  std::string GetIdToken() const override {
    return test::GetChildAccountOAuthIdToken();
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  Profile* child_profile_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenTimeControllerTest);
};

// Tests a simple lock override.
IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest, LockOverride) {
  // Verify screen is unlocked.
  EXPECT_FALSE(session_manager::SessionManager::Get()->IsScreenLocked());

  // Wait one hour.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(1));
  EXPECT_FALSE(session_manager::SessionManager::Get()->IsScreenLocked());

  // Set new policy.
  int64_t created_at_millis =
      (task_runner_->Now() - base::Time::UnixEpoch()).InMilliseconds();
  std::string policy_value = base::StringPrintf(
      R"(
      {
        "time_usage_limit": {
          "reset_at": {
            "hour": 6, "minute": 0
          }
        },
        "overrides": [{
          "action": "LOCK",
          "created_at_millis": "%ld"
        }]
      })",
      created_at_millis);
  auto policy = std::make_unique<base::DictionaryValue>();
  policy->SetKey("UsageTimeLimit", base::Value(policy_value));
  user_policy_helper()->UpdatePolicy(*policy, base::DictionaryValue(),
                                     child_profile_);

  WaitForScreenLock();
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsScreenLocked());
}

}  // namespace chromeos
