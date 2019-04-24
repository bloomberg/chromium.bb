// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/extensions/quick_unlock_private/quick_unlock_private_api.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/screens/gaia_view.h"
#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"
#include "chrome/browser/chromeos/login/screens/update_screen.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/test_predicate_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/update_engine_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"

namespace chromeos {
namespace {

class ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver {
 public:
  explicit ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver(
      extensions::QuickUnlockPrivateGetAuthTokenFunction::TestObserver*
          observer) {
    extensions::QuickUnlockPrivateGetAuthTokenFunction::SetTestObserver(
        observer);
  }
  ~ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver() {
    extensions::QuickUnlockPrivateGetAuthTokenFunction::SetTestObserver(
        nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver);
};

}  // namespace

class OobeInteractiveUITest
    : public OobeBaseTest,
      public extensions::QuickUnlockPrivateGetAuthTokenFunction::TestObserver,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  struct Parameters {
    bool is_tablet;
    bool is_quick_unlock_enabled;

    std::string ToString() const {
      return std::string("{is_tablet: ") + (is_tablet ? "true" : "false") +
             ", is_quick_unlock_enabled: " +
             (is_quick_unlock_enabled ? "true" : "false") + "}";
    }
  };

  OobeInteractiveUITest() = default;
  ~OobeInteractiveUITest() override = default;

  void SetUp() override {
    params_ = Parameters();
    std::tie(params_->is_tablet, params_->is_quick_unlock_enabled) = GetParam();
    LOG(INFO) << "OobeInteractiveUITest() started with params "
              << params_->ToString();
    OobeBaseTest::SetUp();
  }

  void TearDown() override {
    OobeBaseTest::TearDown();
    params_.reset();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);

    if (params_->is_tablet)
      command_line->AppendSwitch(ash::switches::kAshEnableTabletMode);
  }

  void SetUpInProcessBrowserTestFixture() override {
    OobeBaseTest::SetUpInProcessBrowserTestFixture();

    if (params_->is_quick_unlock_enabled)
      quick_unlock::EnableForTesting();
  }

  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHost::default_host()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&chrome::AttemptExit));
      RunUntilBrowserProcessQuits();
    }

    OobeBaseTest::TearDownOnMainThread();
  }

  // QuickUnlockPrivateGetAuthTokenFunction::TestObserver:
  void OnGetAuthTokenCalled(const std::string& password) override {
    quick_unlock_private_get_auth_token_password_ = password;
  }

  void WaitForLoginDisplayHostShutdown() {
    if (!LoginDisplayHost::default_host())
      return;

    LOG(INFO)
        << "OobeInteractiveUITest: Waiting for LoginDisplayHost to shut down.";
    test::TestPredicateWaiter(base::BindRepeating([]() {
      return !LoginDisplayHost::default_host();
    })).Wait();
    LOG(INFO) << "OobeInteractiveUITest: LoginDisplayHost is down.";
  }

  void WaitForOobeWelcomeScreen() {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources());
    observer.Wait();

    test::OobeJS()
        .CreateWaiter("Oobe.getInstance().currentScreen.id == 'connect'")
        ->Wait();
  }

  void RunWelcomeScreenChecks() {
#if defined(GOOGLE_CHROME_BUILD)
    constexpr int kNumberOfVideosPlaying = 1;
#else
    constexpr int kNumberOfVideosPlaying = 0;
#endif

    test::OobeJS().ExpectTrue("!$('oobe-welcome-md').$.welcomeScreen.hidden");
    test::OobeJS().ExpectTrue(
        "$('oobe-welcome-md').$.accessibilityScreen.hidden");
    test::OobeJS().ExpectTrue("$('oobe-welcome-md').$.languageScreen.hidden");
    test::OobeJS().ExpectTrue("$('oobe-welcome-md').$.timezoneScreen.hidden");

    test::OobeJS().ExpectEQ(
        "(() => {let cnt = 0; for (let v of "
        "$('oobe-welcome-md').$.welcomeScreen.root.querySelectorAll('video')) "
        "{  cnt += v.paused ? 0 : 1; }; return cnt; })()",
        kNumberOfVideosPlaying);
  }

  void TapWelcomeNext() {
    test::OobeJS().ExecuteAsync(
        "$('oobe-welcome-md').$.welcomeScreen.$.welcomeNextButton.click()");
  }

  void WaitForNetworkSelectionScreen() {
    test::OobeJS()
        .CreateWaiter(
            "Oobe.getInstance().currentScreen.id == 'network-selection'")
        ->Wait();
    LOG(INFO)
        << "OobeInteractiveUITest: Switched to 'network-selection' screen.";
  }

  void RunNetworkSelectionScreenChecks() {
    test::OobeJS().ExpectTrue(
        "!$('oobe-network-md').$.networkDialog.querySelector('oobe-next-button'"
        ").disabled");
  }

  void TapNetworkSelectionNext() {
    test::OobeJS().ExecuteAsync(
        "$('oobe-network-md').$.networkDialog.querySelector('oobe-next-button')"
        ".click()");
  }

  void WaitForEulaScreen() {
    test::OobeJS()
        .CreateWaiter("Oobe.getInstance().currentScreen.id == 'eula'")
        ->Wait();
    LOG(INFO) << "OobeInteractiveUITest: Switched to 'eula' screen.";
  }

  void RunEulaScreenChecks() {
    // Wait for actual EULA to appear.
    test::OobeJS()
        .CreateWaiter("!$('oobe-eula-md').$.eulaDialog.hidden")
        ->Wait();
    test::OobeJS().ExpectTrue("!$('oobe-eula-md').$.acceptButton.disabled");
  }

  void TapEulaAccept() {
    test::OobeJS().ExecuteAsync("$('oobe-eula-md').$.acceptButton.click();");
  }

  void WaitForUpdateScreen() {
    test::OobeJS()
        .CreateWaiter("Oobe.getInstance().currentScreen.id == 'update'")
        ->Wait();
    test::OobeJS().CreateWaiter("!$('update').hidden")->Wait();

    LOG(INFO) << "OobeInteractiveUITest: Switched to 'update' screen.";
  }

  void ExitUpdateScreenNoUpdate() {
    UpdateScreen* screen = static_cast<UpdateScreen*>(
        WizardController::default_controller()->GetScreen(
            OobeScreen::SCREEN_OOBE_UPDATE));
    UpdateEngineClient::Status status;
    status.status = UpdateEngineClient::UPDATE_STATUS_ERROR;
    screen->UpdateStatusChanged(status);
  }

  void WaitForGaiaSignInScreen() {
    test::OobeJS()
        .CreateWaiter("Oobe.getInstance().currentScreen.id == 'gaia-signin'")
        ->Wait();
    LOG(INFO) << "OobeInteractiveUITest: Switched to 'gaia-signin' screen.";
  }

  void LogInAsRegularUser() {
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetGaiaScreenView()
        ->ShowSigninScreenForTest(FakeGaiaMixin::kFakeUserEmail,
                                  FakeGaiaMixin::kFakeUserPassword,
                                  FakeGaiaMixin::kEmptyUserServices);
    LOG(INFO) << "OobeInteractiveUITest: Logged in.";
  }

  void WaitForSyncConsentScreen() {
    LOG(INFO) << "OobeInteractiveUITest: Waiting for 'sync-consent' screen.";
    test::OobeJS()
        .CreateWaiter("Oobe.getInstance().currentScreen.id == 'sync-consent'")
        ->Wait();
  }

  void ExitScreenSyncConsent() {
    SyncConsentScreen* screen = static_cast<SyncConsentScreen*>(
        WizardController::default_controller()->GetScreen(
            OobeScreen::SCREEN_SYNC_CONSENT));

    screen->SetProfileSyncDisabledByPolicyForTesting(true);
    screen->OnStateChanged(nullptr);
    LOG(INFO) << "OobeInteractiveUITest: Waiting for 'sync-consent' screen "
                 "to close.";
    test::CreatePredicateOrOobeDestroyedWaiter(
        "Oobe.getInstance().currentScreen.id != 'sync-consent'")
        ->Wait();
  }

  void WaitForFingerprintScreen() {
    LOG(INFO)
        << "OobeInteractiveUITest: Waiting for 'fingerprint-setup' screen.";
    test::OobeJS()
        .CreateWaiter(
            "Oobe.getInstance().currentScreen.id == 'fingerprint-setup'")
        ->Wait();
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
                 "to show.";
    test::OobeJS().CreateWaiter("!$('fingerprint-setup').hidden")->Wait();
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
                 "to initializes.";
    test::OobeJS().CreateWaiter("!$('fingerprint-setup-impl').hidden")->Wait();
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
                 "to show setupFingerprint.";
    test::OobeJS()
        .CreateWaiter("!$('fingerprint-setup-impl').$.setupFingerprint.hidden")
        ->Wait();
  }

  void RunFingerprintScreenChecks() {
    test::OobeJS().ExpectTrue("!$('fingerprint-setup').hidden");
    test::OobeJS().ExpectTrue("!$('fingerprint-setup-impl').hidden");
    test::OobeJS().ExpectTrue(
        "!$('fingerprint-setup-impl').$.setupFingerprint.hidden");
    test::OobeJS().ExecuteAsync(
        "$('fingerprint-setup-impl').$.showSensorLocationButton.click()");
    test::OobeJS().ExpectTrue(
        "$('fingerprint-setup-impl').$.setupFingerprint.hidden");
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup "
                 "to switch to placeFinger.";
    test::OobeJS()
        .CreateWaiter("!$('fingerprint-setup-impl').$.placeFinger.hidden")
        ->Wait();
  }

  void ExitFingerprintPinSetupScreen() {
    test::OobeJS().ExpectTrue(
        "!$('fingerprint-setup-impl').$.placeFinger.hidden");
    // This might be the last step in flow. Synchronious execute gets stuck as
    // WebContents may be destroyed in the process. So it may never return.
    // So we use ExecuteAsync() here.
    test::OobeJS().ExecuteAsync(
        "$('fingerprint-setup-impl').$.setupFingerprintLater.click()");
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
                 "to close.";
    test::CreatePredicateOrOobeDestroyedWaiter(
        "Oobe.getInstance().currentScreen.id != 'fingerprint-setup'")
        ->Wait();
    LOG(INFO) << "OobeInteractiveUITest: 'fingerprint-setup' screen done.";
  }

  void WaitForDiscoverScreen() {
    test::OobeJS()
        .CreateWaiter("Oobe.getInstance().currentScreen.id == 'discover'")
        ->Wait();
    LOG(INFO) << "OobeInteractiveUITest: Switched to 'discover' screen.";
  }

  void RunDiscoverScreenChecks() {
    test::OobeJS().ExpectTrue("!$('discover').hidden");
    test::OobeJS().ExpectTrue("!$('discover-impl').hidden");
    test::OobeJS().ExpectTrue(
        "!$('discover-impl').root.querySelector('discover-pin-setup-module')."
        "hidden");
    test::OobeJS().ExpectTrue(
        "!$('discover-impl').root.querySelector('discover-pin-setup-module').$."
        "setup.hidden");
    EXPECT_TRUE(quick_unlock_private_get_auth_token_password_.has_value());
    EXPECT_EQ(quick_unlock_private_get_auth_token_password_,
              FakeGaiaMixin::kFakeUserPassword);
  }

  void ExitDiscoverPinSetupScreen() {
    // This might be the last step in flow. Synchronious execute gets stuck as
    // WebContents may be destroyed in the process. So it may never return.
    // So we use ExecuteAsync() here.
    test::OobeJS().ExecuteAsync(
        "$('discover-impl').root.querySelector('discover-pin-setup-module')."
        "$.setupSkipButton.click()");
    test::CreatePredicateOrOobeDestroyedWaiter(
        "Oobe.getInstance().currentScreen.id != 'discover'")
        ->Wait();
    LOG(INFO) << "OobeInteractiveUITest: 'discover' screen done.";
  }

  void WaitForUserImageScreen() {
    test::OobeJS()
        .CreateWaiter("Oobe.getInstance().currentScreen.id == 'user-image'")
        ->Wait();

    LOG(INFO) << "OobeInteractiveUITest: Switched to 'user-image' screen.";
  }

  void SimpleEndToEnd();

  base::Optional<std::string> quick_unlock_private_get_auth_token_password_;
  base::Optional<Parameters> params_;

 private:
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};

  DISALLOW_COPY_AND_ASSIGN(OobeInteractiveUITest);
};

void OobeInteractiveUITest::SimpleEndToEnd() {
  ASSERT_TRUE(params_.has_value());
  ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver scoped_observer(this);

  WaitForOobeWelcomeScreen();
  RunWelcomeScreenChecks();
  TapWelcomeNext();

  WaitForNetworkSelectionScreen();
  RunNetworkSelectionScreenChecks();
  TapNetworkSelectionNext();

#if defined(GOOGLE_CHROME_BUILD)
  WaitForEulaScreen();
  RunEulaScreenChecks();
  TapEulaAccept();
#endif

  WaitForUpdateScreen();
  ExitUpdateScreenNoUpdate();
  WaitForGaiaSignInScreen();

  LogInAsRegularUser();

#if defined(GOOGLE_CHROME_BUILD)
  WaitForSyncConsentScreen();
  ExitScreenSyncConsent();
#endif

  if (quick_unlock::IsEnabledForTesting()) {
    WaitForFingerprintScreen();
    RunFingerprintScreenChecks();
    ExitFingerprintPinSetupScreen();
  }

  if (TabletModeClient::Get()->tablet_mode_enabled()) {
    WaitForDiscoverScreen();
    RunDiscoverScreenChecks();
    ExitDiscoverPinSetupScreen();
  }

  WaitForLoginDisplayHostShutdown();
}

// Flaky on MSAN/ASAN/LSAN: crbug.com/891277, crbug.com/891484.
// Flaky on normal builds: crbug.com/936041
IN_PROC_BROWSER_TEST_P(OobeInteractiveUITest, DISABLED_SimpleEndToEnd) {
  SimpleEndToEnd();
}

INSTANTIATE_TEST_SUITE_P(OobeInteractiveUITestImpl,
                         OobeInteractiveUITest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  //  namespace chromeos
