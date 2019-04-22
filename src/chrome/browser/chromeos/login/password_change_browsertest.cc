// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager_test_api.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chromeos/login/auth/stub_authenticator.h"
#include "chromeos/login/auth/stub_authenticator_builder.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

namespace {

constexpr char kTestUser[] = "test-user1@gmail.com";
constexpr char kTestUserGaiaId[] = "test-user1@gmail.com";
constexpr char kPassword[] = "test user password";

// Waits for the window that hosts OOBE UI changes visibility to target value.
// When waiting for the OOBE UI window to be hidden, it handles the window
// getting destroyed. Window getting destroyed while waiting for the window
// to become visible will stop the waiter, but will cause a test failure.
class OobeUiWindowVisibilityWaiter : public aura::WindowObserver {
 public:
  explicit OobeUiWindowVisibilityWaiter(bool target_visibilty)
      : target_visibility_(target_visibilty) {}
  ~OobeUiWindowVisibilityWaiter() override = default;

  void Wait() {
    aura::Window* window = GetWindow();
    if (!window && !target_visibility_)
      return;

    DCHECK(window);
    if (target_visibility_ == window->IsVisible())
      return;

    base::RunLoop run_loop;
    wait_stop_closure_ = run_loop.QuitClosure();
    window_observer_.Add(window);
    run_loop.Run();
  }

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (visible != target_visibility_)
      return;
    window_observer_.RemoveAll();
    std::move(wait_stop_closure_).Run();
  }

  void OnWindowDestroyed(aura::Window* window) override {
    EXPECT_FALSE(target_visibility_);
    window_observer_.RemoveAll();
    std::move(wait_stop_closure_).Run();
  }

 private:
  aura::Window* GetWindow() {
    LoginDisplayHost* host = LoginDisplayHost::default_host();
    if (!host || !host->GetOobeWebContents())
      return nullptr;
    return host->GetOobeWebContents()->GetTopLevelNativeWindow();
  }

  const bool target_visibility_;
  base::OnceClosure wait_stop_closure_;
  ScopedObserver<aura::Window, OobeUiWindowVisibilityWaiter> window_observer_{
      this};
};

// Used to wait for session manager to become active.
class SessionStartWaiter : public session_manager::SessionManagerObserver {
 public:
  SessionStartWaiter() = default;
  ~SessionStartWaiter() override = default;

  void Wait() {
    if (!session_manager::SessionManager::Get()->IsUserSessionBlocked())
      return;
    session_observer_.Add(session_manager::SessionManager::Get());

    base::RunLoop run_loop;
    session_active_callback_ = run_loop.QuitClosure();
    run_loop.Run();

    session_observer_.RemoveAll();
  }

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override {
    if (!session_manager::SessionManager::Get()->IsUserSessionBlocked() &&
        session_active_callback_) {
      std::move(session_active_callback_).Run();
    }
  }

 private:
  base::OnceClosure session_active_callback_;
  ScopedObserver<session_manager::SessionManager, SessionStartWaiter>
      session_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(SessionStartWaiter);
};

}  // namespace

class PasswordChangeTest : public LoginManagerTest {
 public:
  PasswordChangeTest()
      : LoginManagerTest(false, false),
        test_account_id_(
            AccountId::FromUserEmailGaiaId(kTestUser, kTestUserGaiaId)) {
    set_force_webui_login(false);
  }
  ~PasswordChangeTest() override = default;

  UserContext GetTestUserContext() {
    const user_manager::User* user =
        user_manager::UserManager::Get()->FindUser(test_account_id_);
    UserContext user_context(*user);
    user_context.SetKey(Key(kPassword));
    user_context.SetUserIDHash(test_account_id_.GetUserEmail());
    return user_context;
  }

  // Sets up UserSessionManager to use stub authenticator that reports a
  // password change, and attempts login.
  // Password changed OOBE dialog is expected to show up after calling this.
  void SetUpStubAuthentcatorAndAttemptLogin(const std::string& old_password) {
    UserContext user_context = GetTestUserContext();

    auto authenticator_builder =
        std::make_unique<StubAuthenticatorBuilder>(user_context);
    authenticator_builder->SetUpPasswordChange(
        old_password,
        base::BindRepeating(&PasswordChangeTest::HandleDataRecoveryStatusChange,
                            base::Unretained(this)));
    test::UserSessionManagerTestApi(UserSessionManager::GetInstance())
        .InjectAuthenticatorBuilder(std::move(authenticator_builder));

    ExistingUserController::current_controller()->SetDisplayEmail(
        test_account_id_.GetUserEmail());
    ExistingUserController::current_controller()->Login(user_context,
                                                        SigninSpecifics());
  }

 protected:
  AccountId test_account_id_;
  StubAuthenticator::DataRecoveryStatus data_recovery_status_ =
      StubAuthenticator::DataRecoveryStatus::kNone;

 private:
  void HandleDataRecoveryStatusChange(
      StubAuthenticator::DataRecoveryStatus status) {
    EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kNone,
              data_recovery_status_);
    data_recovery_status_ = status;
  }
};

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, PRE_MigrateOldCryptohome) {
  RegisterUser(test_account_id_);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, MigrateOldCryptohome) {
  SetUpStubAuthentcatorAndAttemptLogin("old user password");
  OobeScreenWaiter(OobeScreen::SCREEN_PASSWORD_CHANGED).Wait();
  OobeUiWindowVisibilityWaiter(true).Wait();
  test::OobeJS().CreateVisibilityWaiter(
      true, {"gaia-password-changed", "oldPasswordCard"});

  // Fill out and submit the old password passed to the stub authenticator.
  test::OobeJS().TypeIntoPath("old user password",
                              {"gaia-password-changed", "oldPasswordInput"});
  test::OobeJS().TapOnPath(
      {"gaia-password-changed", "oldPasswordInputForm", "button"});

  // User session should start, and whole OOBE screen is expected to be hidden,
  OobeUiWindowVisibilityWaiter(false).Wait();
  EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kRecovered,
            data_recovery_status_);

  SessionStartWaiter().Wait();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, PRE_RetryOnWrongPassword) {
  RegisterUser(test_account_id_);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, RetryOnWrongPassword) {
  SetUpStubAuthentcatorAndAttemptLogin("old user password");
  OobeScreenWaiter(OobeScreen::SCREEN_PASSWORD_CHANGED).Wait();
  OobeUiWindowVisibilityWaiter(true).Wait();
  test::OobeJS().CreateVisibilityWaiter(
      true, {"gaia-password-changed", "oldPasswordCard"});

  // Fill out and submit the old password passed to the stub authenticator.
  test::OobeJS().TypeIntoPath("incorrect old user password",
                              {"gaia-password-changed", "oldPasswordInput"});
  test::OobeJS().TapOnPath(
      {"gaia-password-changed", "oldPasswordInputForm", "button"});

  // Expect the UI to report failure.
  test::OobeJS()
      .CreateWaiter(test::GetOobeElementPath(
                        {"gaia-password-changed", "oldPasswordInput"}) +
                    ".isInvalid")
      ->Wait();
  test::OobeJS().ExpectEnabledPath(
      {"gaia-password-changed", "oldPasswordCard"});

  EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kRecoveryFailed,
            data_recovery_status_);

  data_recovery_status_ = StubAuthenticator::DataRecoveryStatus::kNone;

  // Submit the correct password.
  test::OobeJS().TypeIntoPath("old user password",
                              {"gaia-password-changed", "oldPasswordInput"});
  test::OobeJS().TapOnPath(
      {"gaia-password-changed", "oldPasswordInputForm", "button"});

  // User session should start, and whole OOBE screen is expected to be hidden,
  OobeUiWindowVisibilityWaiter(false).Wait();
  EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kRecovered,
            data_recovery_status_);

  SessionStartWaiter().Wait();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, PRE_SkipDataRecovery) {
  RegisterUser(test_account_id_);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, SkipDataRecovery) {
  SetUpStubAuthentcatorAndAttemptLogin("old user password");
  OobeScreenWaiter(OobeScreen::SCREEN_PASSWORD_CHANGED).Wait();
  OobeUiWindowVisibilityWaiter(true).Wait();
  test::OobeJS().CreateVisibilityWaiter(
      true, {"gaia-password-changed", "oldPasswordCard"});

  // Click forgot password link.
  test::OobeJS().TapOnPath({"gaia-password-changed", "forgot-password-link"});

  test::OobeJS().CreateVisibilityWaiter(
      false, {"gaia-password-changed", "oldPasswordCard"});

  test::OobeJS().ExpectVisiblePath({"gaia-password-changed", "try-again-link"});
  test::OobeJS().ExpectVisiblePath(
      {"gaia-password-changed", "proceedAnywayBtn"});

  // Click "Proceed anyway".
  test::OobeJS().TapOnPath({"gaia-password-changed", "proceedAnywayBtn"});

  // User session should start, and whole OOBE screen is expected to be hidden,
  OobeUiWindowVisibilityWaiter(false).Wait();
  EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kResynced,
            data_recovery_status_);

  SessionStartWaiter().Wait();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, PRE_TryAgainAfterForgetLinkClick) {
  RegisterUser(test_account_id_);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, TryAgainAfterForgetLinkClick) {
  SetUpStubAuthentcatorAndAttemptLogin("old user password");
  OobeScreenWaiter(OobeScreen::SCREEN_PASSWORD_CHANGED).Wait();
  OobeUiWindowVisibilityWaiter(true).Wait();
  test::OobeJS().CreateVisibilityWaiter(
      true, {"gaia-password-changed", "oldPasswordCard"});

  // Click forgot password link.
  test::OobeJS().TapOnPath({"gaia-password-changed", "forgot-password-link"});

  test::OobeJS().CreateVisibilityWaiter(
      false, {"gaia-password-changed", "oldPasswordCard"});

  test::OobeJS().ExpectVisiblePath({"gaia-password-changed", "try-again-link"});
  test::OobeJS().ExpectVisiblePath(
      {"gaia-password-changed", "proceedAnywayBtn"});

  // Go back to old password input by clicking Try Again.
  test::OobeJS().TapOnPath({"gaia-password-changed", "try-again-link"});

  test::OobeJS().CreateVisibilityWaiter(
      true, {"gaia-password-changed", "oldPasswordCard"});

  // Enter and submit the correct password.
  test::OobeJS().TypeIntoPath("old user password",
                              {"gaia-password-changed", "oldPasswordInput"});
  test::OobeJS().TapOnPath(
      {"gaia-password-changed", "oldPasswordInputForm", "button"});

  // User session should start, and whole OOBE screen is expected to be hidden,
  OobeUiWindowVisibilityWaiter(false).Wait();
  EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kRecovered,
            data_recovery_status_);

  SessionStartWaiter().Wait();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, PRE_ClosePasswordChangedDialog) {
  RegisterUser(test_account_id_);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, ClosePasswordChangedDialog) {
  SetUpStubAuthentcatorAndAttemptLogin("old user password");
  OobeScreenWaiter(OobeScreen::SCREEN_PASSWORD_CHANGED).Wait();
  test::OobeJS().CreateVisibilityWaiter(
      true, {"gaia-password-changed", "oldPasswordCard"});

  test::OobeJS().TypeIntoPath("old user password",
                              {"gaia-password-changed", "oldPasswordInput"});
  // Click the close button.
  test::OobeJS().TapOnPath(
      {"gaia-password-changed", "navigation", "closeButton"});

  OobeUiWindowVisibilityWaiter(false).Wait();
  EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kNone,
            data_recovery_status_);

  ExistingUserController::current_controller()->Login(GetTestUserContext(),
                                                      SigninSpecifics());
  OobeUiWindowVisibilityWaiter(true).Wait();
  OobeScreenWaiter(OobeScreen::SCREEN_PASSWORD_CHANGED).Wait();
}

}  // namespace chromeos
