// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/fake_eula_mixin.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/consolidated_consent_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/hardware_data_collection_screen_handler.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

const test::UIPath kEulaDialog = {"oobe-eula-md", "eulaDialog"};
const test::UIPath kAcceptEulaButton = {"oobe-eula-md", "acceptButton"};
const test::UIPath kAcceptHWDataCollectionButton = {"hw-data-collection",
                                                    "acceptButton"};
const test::UIPath kAcceptConsolidatedConsentButton = {"consolidated-consent",
                                                       "acceptButton"};
const test::UIPath kConsolidatedConsentDialog = {"consolidated-consent",
                                                 "loadedDialog"};

}  // namespace

// Boolean parameter represents OobeConsolidatedConsent feature state.
class LoginAfterUpdateToFlexTest : public LoginManagerTest,
                                   public LocalStateMixin::Delegate,
                                   public ::testing::WithParamInterface<bool> {
 public:
  LoginAfterUpdateToFlexTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kOobeConsolidatedConsent);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kOobeConsolidatedConsent);
    }
    login_manager_mixin_.AppendRegularUsers(2);
  }

  LoginAfterUpdateToFlexTest(const LoginAfterUpdateToFlexTest&) = delete;
  LoginAfterUpdateToFlexTest& operator=(const LoginAfterUpdateToFlexTest&) =
      delete;
  ~LoginAfterUpdateToFlexTest() override = default;

  // LoginManagerTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kRevenBranding);
    LoginManagerTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    settings_helper_.GetStubbedProvider()->Set(
        kDeviceOwner, base::Value(GetOwnerAccountId().GetUserEmail()));
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;
  }

  void TearDownOnMainThread() override {
    settings_helper_.RestoreRealDeviceSettingsProvider();
    LoginManagerTest::TearDownOnMainThread();
  }

  // LocalStateMixin::Delegate:
  void SetUpLocalState() override {
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(prefs::kOobeRevenUpdatedToFlex, true);
  }

  const AccountId& GetOwnerAccountId() {
    return login_manager_mixin_.users()[0].account_id;
  }

  const AccountId& GetRegularAccountId() {
    return login_manager_mixin_.users()[1].account_id;
  }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  FakeEulaMixin fake_eula_{&mixin_host_, embedded_test_server()};
  ScopedCrosSettingsTestHelper settings_helper_{
      /*create_settings_service=*/false};
  base::test::ScopedFeatureList scoped_feature_list_;
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_P(LoginAfterUpdateToFlexTest, DeviceOwner) {
  LoginUser(GetOwnerAccountId());
  if (!features::IsOobeConsolidatedConsentEnabled()) {
    OobeScreenWaiter(EulaView::kScreenId).Wait();
    // Wait until the webview has finished loading.
    test::OobeJS().CreateVisibilityWaiter(true, kEulaDialog)->Wait();
    test::OobeJS().TapOnPath(kAcceptEulaButton);
  } else {
    EXPECT_FALSE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
        prefs::kRevenOobeConsolidatedConsentAccepted));
    OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
    test::OobeJS()
        .CreateVisibilityWaiter(true, kConsolidatedConsentDialog)
        ->Wait();
    test::OobeJS().TapOnPath(kAcceptConsolidatedConsentButton);
    EXPECT_TRUE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
        prefs::kRevenOobeConsolidatedConsentAccepted));
  }
  OobeScreenWaiter(HWDataCollectionView::kScreenId).Wait();
  test::OobeJS().TapOnPath(kAcceptHWDataCollectionButton);
  test::WaitForPrimaryUserSessionStart();
}

IN_PROC_BROWSER_TEST_P(LoginAfterUpdateToFlexTest, RegularUser) {
  LoginUser(GetRegularAccountId());
  if (features::IsOobeConsolidatedConsentEnabled()) {
    EXPECT_FALSE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
        prefs::kRevenOobeConsolidatedConsentAccepted));
    OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
    test::OobeJS()
        .CreateVisibilityWaiter(true, kConsolidatedConsentDialog)
        ->Wait();
    test::OobeJS().TapOnPath(kAcceptConsolidatedConsentButton);
    EXPECT_TRUE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
        prefs::kRevenOobeConsolidatedConsentAccepted));
  }
  test::WaitForPrimaryUserSessionStart();
}

INSTANTIATE_TEST_SUITE_P(All, LoginAfterUpdateToFlexTest, testing::Bool());

}  // namespace ash
