// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/existing_user_controller_base_test.h"
#include "chrome/browser/ash/login/saml/password_sync_token_checkers_collection.h"
#include "chrome/browser/ash/login/saml/password_sync_token_fetcher.h"
#include "chrome/browser/ash/login/saml/password_sync_token_login_checker.h"
#include "chrome/browser/ash/login/ui/mock_login_display.h"
#include "chrome/browser/ash/login/ui/mock_login_display_host.h"
#include "chrome/browser/ash/login/users/mock_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::Return;

const char kSamlToken1[] = "saml-token-1";
const char kSamlToken2[] = "saml-token-2";

constexpr base::TimeDelta kLoginOnlineShortDelay = base::Seconds(10);

}  // namespace

class ExistingUserControllerForcedOnlineAuthTest
    : public ExistingUserControllerBaseTest {
 public:
  ExistingUserControllerForcedOnlineAuthTest() {
    mock_login_display_host_ = std::make_unique<MockLoginDisplayHost>();
    mock_login_display_ = std::make_unique<MockLoginDisplay>();
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    existing_user_controller_ = std::make_unique<ExistingUserController>();

    ON_CALL(*mock_login_display_host_, GetLoginDisplay())
        .WillByDefault(Return(mock_login_display_.get()));
    ON_CALL(*mock_login_display_host_, GetExistingUserController())
        .WillByDefault(Return(existing_user_controller_.get()));
  }

  ExistingUserController* existing_user_controller() const {
    return ExistingUserController::current_controller();
  }

  int password_sync_token_checkers_size() {
    if (!existing_user_controller()->sync_token_checkers_)
      return 0;
    return existing_user_controller()
        ->sync_token_checkers_->sync_token_checkers_.size();
  }

  PasswordSyncTokenLoginChecker* get_password_sync_token_checker(
      std::string token) {
    return existing_user_controller()
        ->sync_token_checkers_->sync_token_checkers_[token]
        .get();
  }

  void set_hide_user_names_on_signin() {
    settings_helper_.SetBoolean(kAccountsPrefShowUserNamesOnSignIn, false);
  }

 private:
  std::unique_ptr<MockLoginDisplayHost> mock_login_display_host_;
  std::unique_ptr<MockLoginDisplay> mock_login_display_;

  // Required by ExistingUserController:
  ScopedCrosSettingsTestHelper settings_helper_;
  ArcKioskAppManager arc_kiosk_app_manager_;

  std::unique_ptr<ExistingUserController> existing_user_controller_;
};

// Tests creation of password sync token checker for 2 SAML users. Only one of
// them has local copy of password sync token.
TEST_F(ExistingUserControllerForcedOnlineAuthTest,
       SyncTokenCheckersCreationWithOneToken) {
  user_manager::known_user::SetPasswordSyncToken(saml_login_account1_id_,
                                                 kSamlToken1);
  set_hide_user_names_on_signin();
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account1_id_);
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account2_id_);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  EXPECT_EQ(password_sync_token_checkers_size(), 1);
  get_password_sync_token_checker(kSamlToken1)->OnTokenVerified(true);
  task_environment_.FastForwardBy(kLoginOnlineShortDelay);
  EXPECT_TRUE(get_password_sync_token_checker(kSamlToken1)->IsCheckPending());
}

// Tests creation of password sync token checker for 2 SAML users with password
// sync tokens.
TEST_F(ExistingUserControllerForcedOnlineAuthTest,
       SyncTokenCheckersCreationWithTwoTokens) {
  user_manager::known_user::SetPasswordSyncToken(saml_login_account1_id_,
                                                 kSamlToken1);
  user_manager::known_user::SetPasswordSyncToken(saml_login_account2_id_,
                                                 kSamlToken2);
  set_hide_user_names_on_signin();
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account1_id_);
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account2_id_);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  EXPECT_EQ(password_sync_token_checkers_size(), 2);
  get_password_sync_token_checker(kSamlToken1)
      ->OnApiCallFailed(PasswordSyncTokenFetcher::ErrorType::kServerError);
  task_environment_.FastForwardBy(kLoginOnlineShortDelay);
  EXPECT_TRUE(get_password_sync_token_checker(kSamlToken1)->IsCheckPending());
}

// Tests sync token checkers removal in case of failed token validation.
TEST_F(ExistingUserControllerForcedOnlineAuthTest,
       SyncTokenCheckersInvalidPasswordForTwoUsers) {
  user_manager::known_user::SetPasswordSyncToken(saml_login_account1_id_,
                                                 kSamlToken1);
  user_manager::known_user::SetPasswordSyncToken(saml_login_account2_id_,
                                                 kSamlToken2);
  set_hide_user_names_on_signin();
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account1_id_);
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account2_id_);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  EXPECT_EQ(password_sync_token_checkers_size(), 2);
  get_password_sync_token_checker(kSamlToken1)->OnTokenVerified(false);
  get_password_sync_token_checker(kSamlToken2)->OnTokenVerified(false);
  EXPECT_EQ(password_sync_token_checkers_size(), 0);
}

// Sync token checkers are not owned by ExistingUserController if user pods are
// visible.
TEST_F(ExistingUserControllerForcedOnlineAuthTest,
       NoSyncTokenCheckersWhenPodsVisible) {
  user_manager::known_user::SetPasswordSyncToken(saml_login_account1_id_,
                                                 kSamlToken1);
  user_manager::known_user::SetPasswordSyncToken(saml_login_account2_id_,
                                                 kSamlToken2);
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account1_id_);
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account2_id_);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  EXPECT_EQ(password_sync_token_checkers_size(), 0);
}

}  // namespace ash
