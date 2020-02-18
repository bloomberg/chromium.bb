// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager_test_api.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/login/test/user_policy_mixin.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"

namespace em = enterprise_management;

namespace chromeos {
namespace {

struct Params {
  Params(std::string login_screen_isolate_origins,
         std::string user_policy_isolate_origins,
         bool user_policy_site_per_process,
         std::vector<std::string> user_flag_internal_names,
         bool ephemeral_users,
         bool expected_request_restart,
         std::vector<std::string> expected_switches_for_user,
         std::vector<std::string> expected_isolated_origins = {})
      : login_screen_isolate_origins(login_screen_isolate_origins),
        user_policy_isolate_origins(user_policy_isolate_origins),
        user_policy_site_per_process(user_policy_site_per_process),
        user_flag_internal_names(std::move(user_flag_internal_names)),
        ephemeral_users(ephemeral_users),
        expected_request_restart(expected_request_restart),
        expected_switches_for_user(expected_switches_for_user),
        expected_isolated_origins(expected_isolated_origins) {}

  friend std::ostream& operator<<(std::ostream& os, const Params& p) {
    os << "{" << std::endl
       << "  login_screen_isolate_origins: " << p.login_screen_isolate_origins
       << std::endl
       << "  user_policy_site_per_process: " << p.user_policy_site_per_process
       << std::endl
       << "  user_policy_isolate_origins: " << p.user_policy_isolate_origins
       << std::endl
       << "  user_flag_internal_names: "
       << base::JoinString(p.user_flag_internal_names, ", ") << std::endl
       << "  ephemeral_users: " << p.ephemeral_users << std::endl
       << "  expected_request_restart: " << p.expected_request_restart
       << std::endl
       << "  expected_switches_for_user: "
       << base::JoinString(p.expected_switches_for_user, ", ") << std::endl
       << "  expected_isolated_origins: "
       << base::JoinString(p.expected_isolated_origins, ", ") << std::endl
       << "}";
    return os;
  }

  // If non-empty, --isolate-origins=|login_screen_isolate_origins| will be
  // passed to the login manager chrome instance between policy flag sentinels.
  // Note: On Chrome OS, login_manager evaluates device policy and does this.
  std::string login_screen_isolate_origins;

  // If non-empty, the IsolateOrigins user policy will be simulated to be set
  // |user_policy_isolate_origins|.
  std::string user_policy_isolate_origins;

  // If true, the SitePerProcess user policy will be simulated to be set to
  // true.
  bool user_policy_site_per_process;

  std::vector<std::string> user_flag_internal_names;

  // If true, ephemeral users are enabled.
  bool ephemeral_users;

  // If true, the test case will expect that AttemptRestart has been called by
  // UserSessionManager.
  bool expected_request_restart;

  // When a restart was requested, the test case verifies that the flags passed
  // to |SessionManagerClient::SetFlagsForUser| match
  // |expected_switches_for_user|.
  std::vector<std::string> expected_switches_for_user;

  // List of origins that should be isolated (via policy or via cmdline flag).
  std::vector<std::string> expected_isolated_origins;
};

// Defines the test cases that will be executed.
const Params kTestCases[] = {
    // 0. No site isolation in device or user policy - no restart expected.
    Params(std::string() /* login_screen_isolate_origins */,
           std::string() /* user_policy_isolate_origins */,
           false /* user_policy_site_per_process */,
           {} /* user_flag_internal_names */,
           false /* ephemeral_users */,
           false /* expected_request_restart */,
           {} /* expected_switches_for_user */),

    // 1. SitePerProcess opt-out through about://flags - restart expected.
    Params(
        std::string() /* login_screen_isolate_origins */,
        std::string() /* user_policy_isolate_origins */,
        false /* user_policy_site_per_process */,
        {"site-isolation-trial-opt-out@1"} /* user_flag_internal_names */,
        false /* ephemeral_users */,
        true /* expected_request_restart */,
        {"--disable-site-isolation-trials"} /* expected_switches_for_user */),

    // 2. SitePerProcess forced through user policy - opt-out through
    // about://flags entry expected to be ignored.
    Params(std::string() /* login_screen_isolate_origins */,
           std::string() /* user_policy_isolate_origins */,
           true /* user_policy_site_per_process */,
           {"site-isolation-trial-opt-out@1"} /* user_flag_internal_names */,
           false /* ephemeral_users */,
           false /* expected_request_restart */,
           {} /* expected_switches_for_user */),

    // 3. IsolateOrigins in user policy only - no restart expected, because
    //    IsolateOrigins from the user policy should be picked up by
    //    SiteIsolationPrefsObserver (without requiring injection of the
    //    --isolate-origins cmdline switch).
    Params(std::string() /* login_screen_isolate_origins */,
           "https://example.com" /* user_policy_isolate_origins */,
           false /* user_policy_site_per_process */,
           {} /* user_flag_internal_names */,
           false /* ephemeral_users */,
           false /* expected_request_restart */,
           {} /* expected_switches_for_user */,
           {"https://example.com"} /* expected_isolated_origins */)};

constexpr char kTestUserAccountId[] = "username@examle.com";
constexpr char kTestUserGaiaId[] = "1111111111";
constexpr char kTestUserPassword[] = "password";
constexpr char kEmptyServices[] = "[]";

class SiteIsolationFlagHandlingTest
    : public OobeBaseTest,
      public ::testing::WithParamInterface<Params> {
 protected:
  SiteIsolationFlagHandlingTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    SessionManagerClient::InitializeFakeInMemory();

    // Mark that chrome restart can be requested.
    // Note that AttemptRestart() is mocked out in UserSessionManager through
    // |SetAttemptRestartClosureInTests| (set up in SetUpOnMainThread).
    FakeSessionManagerClient::Get()->set_supports_browser_restart(true);

    std::unique_ptr<ScopedDevicePolicyUpdate> update =
        device_state_.RequestDevicePolicyUpdate();
    update->policy_payload()
        ->mutable_ephemeral_users_enabled()
        ->set_ephemeral_users_enabled(GetParam().ephemeral_users);
    update.reset();

    std::unique_ptr<ScopedUserPolicyUpdate> user_policy_update =
        user_policy_.RequestPolicyUpdate();
    if (GetParam().user_policy_site_per_process) {
      user_policy_update->policy_payload()
          ->mutable_siteperprocess()
          ->mutable_policy_options()
          ->set_mode(em::PolicyOptions::MANDATORY);
      user_policy_update->policy_payload()->mutable_siteperprocess()->set_value(
          true);
    }

    if (!GetParam().user_policy_isolate_origins.empty()) {
      user_policy_update->policy_payload()
          ->mutable_isolateorigins()
          ->mutable_policy_options()
          ->set_mode(em::PolicyOptions::MANDATORY);
      user_policy_update->policy_payload()->mutable_isolateorigins()->set_value(
          GetParam().user_policy_isolate_origins);
    }
    user_policy_update.reset();

    OobeBaseTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    fake_gaia_.SetupFakeGaiaForLogin(kTestUserAccountId, kTestUserGaiaId,
                                     FakeGaiaMixin::kFakeRefreshToken);

    OobeBaseTest::SetUpOnMainThread();

    // Mock out chrome restart.
    test::UserSessionManagerTestApi session_manager_test_api(
        UserSessionManager::GetInstance());
    session_manager_test_api.SetAttemptRestartClosureInTests(
        base::BindRepeating(
            &SiteIsolationFlagHandlingTest::AttemptRestartCalled,
            base::Unretained(this)));

    // Observe for user session start.
    user_session_started_observer_ = std::make_unique<SessionStateWaiter>();
  }

  ChromeUserManagerImpl* GetChromeUserManager() const {
    return static_cast<ChromeUserManagerImpl*>(
        user_manager::UserManager::Get());
  }

  bool HasAttemptRestartBeenCalled() const { return attempt_restart_called_; }

  // Called when chrome requests a restarted.
  void AttemptRestartCalled() {
    user_session_started_observer_.reset();
    attempt_restart_called_ = true;
  }

  // This will be set to |true| when chrome has requested a restart.
  bool attempt_restart_called_ = false;

  // This is important because ephemeral users only work on enrolled machines.
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  UserPolicyMixin user_policy_{
      &mixin_host_,
      AccountId::FromUserEmailGaiaId(kTestUserAccountId, kTestUserGaiaId)};

  LoginManagerMixin::TestUserInfo user_{
      AccountId::FromUserEmailGaiaId(kTestUserAccountId, kTestUserGaiaId)};
  LoginManagerMixin login_manager_{&mixin_host_, {user_}};

  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};

  // Observes for user session start.
  std::unique_ptr<SessionStateWaiter> user_session_started_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SiteIsolationFlagHandlingTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_P(SiteIsolationFlagHandlingTest, PRE_FlagHandlingTest) {
  chromeos::LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<chromeos::GaiaScreenHandler>()
      ->ShowSigninScreenForTest(kTestUserAccountId, kTestUserPassword,
                                kEmptyServices);
  user_session_started_observer_->Wait();

  if (!GetParam().user_flag_internal_names.empty()) {
    Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUserUnsafe(
        user_manager::UserManager::Get()->GetActiveUser());
    ASSERT_TRUE(profile);
    flags_ui::PrefServiceFlagsStorage flags_storage(profile->GetPrefs());
    std::set<std::string> flags_to_set;
    for (const std::string& flag_to_set : GetParam().user_flag_internal_names)
      flags_to_set.insert(flag_to_set);
    EXPECT_TRUE(flags_storage.SetFlags(flags_to_set));
    flags_storage.CommitPendingWrites();
  }
}

IN_PROC_BROWSER_TEST_P(SiteIsolationFlagHandlingTest, FlagHandlingTest) {
  // Skip tests where expected_request_restart is true.
  // See crbug.com/990817 for more details.
  if (GetParam().expected_request_restart)
    return;

  // Start user sign-in. We can't use |LoginPolicyTestBase::LogIn|, because
  // it waits for a user session start unconditionally, which will not happen if
  // chrome requests a restart to set user-session flags.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  OobeBaseTest::WaitForSigninScreen();

  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(kTestUserAccountId, kTestUserPassword,
                                kEmptyServices);

  // Wait for either the user session to start, or for restart to be requested
  // (whichever happens first).
  user_session_started_observer_->Wait();

  EXPECT_EQ(GetParam().expected_request_restart, HasAttemptRestartBeenCalled());

  // Verify that expected origins are isolated...
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  for (const std::string& origin_str : GetParam().expected_isolated_origins) {
    url::Origin origin = url::Origin::Create(GURL(origin_str));
    EXPECT_TRUE(policy->IsGloballyIsolatedOriginForTesting(origin));
  }

  if (!HasAttemptRestartBeenCalled())
    return;

  // Also verify flags if chrome was restarted.
  AccountId test_account_id =
      AccountId::FromUserEmailGaiaId(kTestUserAccountId, kTestUserGaiaId);
  std::vector<std::string> switches_for_user;
  bool has_switches_for_user = FakeSessionManagerClient::Get()->GetFlagsForUser(
      cryptohome::CreateAccountIdentifierFromAccountId(test_account_id),
      &switches_for_user);
  EXPECT_TRUE(has_switches_for_user);

  // Remove flag sentinels. Keep whatever is between those sentinels, to
  // verify that we don't pass additional parameters in there.
  base::EraseIf(switches_for_user, [](const std::string& flag) {
    return flag == "--flag-switches-begin" || flag == "--flag-switches-end";
  });
  EXPECT_EQ(GetParam().expected_switches_for_user, switches_for_user);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SiteIsolationFlagHandlingTest,
                         ::testing::ValuesIn(kTestCases));

}  // namespace chromeos
