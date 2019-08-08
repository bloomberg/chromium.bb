// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/login_screen_tester.h"
#include "chrome/browser/chromeos/login/test/oobe_auth_page_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_paths.h"
#include "net/dns/mock_host_resolver.h"

namespace chromeos {
namespace {

constexpr char kExistingUserEmail[] = "existing@gmail.com";
constexpr char kExistingUserGaiaId[] = "9876543210";

constexpr char kNewUserEmail[] = "new@gmail.com";
constexpr char kNewUserGaiaId[] = "0123456789";

class LoginUIShelfVisibilityTest : public MixinBasedInProcessBrowserTest {
 public:
  LoginUIShelfVisibilityTest() = default;
  ~LoginUIShelfVisibilityTest() override = default;

  void SetUp() override {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

    // Don't spin up the IO thread yet since no threads are allowed while
    // spawning sandbox host process. See crbug.com/322732.
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    MixinBasedInProcessBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }
  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

 private:
  LoginManagerMixin login_manager_mixin_{
      &mixin_host_,
      {AccountId::FromUserEmailGaiaId(kExistingUserEmail,
                                      kExistingUserGaiaId)}};
  FakeGaiaMixin fake_gaia_mixin_{&mixin_host_, embedded_test_server()};

  DISALLOW_COPY_AND_ASSIGN(LoginUIShelfVisibilityTest);
};

}  // namespace

// Verifies that shelf buttons are shown by default on login screen.
IN_PROC_BROWSER_TEST_F(LoginUIShelfVisibilityTest, DefaultVisibility) {
  EXPECT_TRUE(test::LoginScreenTester().IsGuestButtonShown());
  EXPECT_TRUE(test::LoginScreenTester().IsAddUserButtonShown());
}

// Verifies that guest button and add user button are hidden when Gaia
// dialog is shown.
IN_PROC_BROWSER_TEST_F(LoginUIShelfVisibilityTest, GaiaDialogOpen) {
  EXPECT_TRUE(test::LoginScreenTester().ClickAddUserButton());
  test::OobeGaiaPageWaiter().WaitUntilReady();
  EXPECT_FALSE(test::LoginScreenTester().IsGuestButtonShown());
  EXPECT_FALSE(test::LoginScreenTester().IsAddUserButtonShown());
}

// Verifies that guest button and add user button are hidden on post-login
// screens, after a user session is started.
IN_PROC_BROWSER_TEST_F(LoginUIShelfVisibilityTest, PostLoginScreen) {
  auto override = WizardController::ForceOfficialBuildForTesting();
  EXPECT_TRUE(test::LoginScreenTester().ClickAddUserButton());
  test::OobeGaiaPageWaiter().WaitUntilReady();
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetGaiaScreenView()
      ->ShowSigninScreenForTest(kNewUserEmail, kNewUserGaiaId,
                                FakeGaiaMixin::kEmptyUserServices);

  // Sync consent is the first post-login screen shown when a new user signs in.
  OobeScreenWaiter(OobeScreen::SCREEN_SYNC_CONSENT).Wait();

  EXPECT_FALSE(test::LoginScreenTester().IsGuestButtonShown());
  EXPECT_FALSE(test::LoginScreenTester().IsAddUserButtonShown());
}

}  // namespace chromeos
