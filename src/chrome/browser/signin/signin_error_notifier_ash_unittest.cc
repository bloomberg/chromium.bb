// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_error_notifier_ash.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_notifier_factory_ash.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

const char kTestGaiaId[] = "gaia_id";
const char kTestEmail[] = "email@example.com";

// Notification ID corresponding to kProfileSigninNotificationId +
// kTestAccountId.
const char kNotificationId[] = "chrome://settings/signin/testing_profile";

class SigninErrorNotifierTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    mock_user_manager_ = new chromeos::MockUserManager();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(mock_user_manager_));

    SigninErrorNotifierFactory::GetForProfile(GetProfile());
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
  }

  void SetAuthError(const GoogleServiceAuthError& error) {
    // TODO(https://crbug.com/836212): Do not use the delegate directly, because
    // it is internal API.
    ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile());
    std::string account_id =
        AccountTrackerServiceFactory::GetForProfile(profile())->SeedAccountInfo(
            kTestGaiaId, kTestEmail);
    if (!token_service->RefreshTokenIsAvailable(account_id))
      token_service->UpdateCredentials(account_id, "refresh_token");
    token_service->GetDelegate()->UpdateAuthError(account_id, error);
  }

 protected:
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  chromeos::MockUserManager* mock_user_manager_;  // Not owned.
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
};

TEST_F(SigninErrorNotifierTest, NoNotification) {
  EXPECT_FALSE(display_service_->GetNotification(kNotificationId));
}

TEST_F(SigninErrorNotifierTest, ErrorReset) {
  EXPECT_FALSE(display_service_->GetNotification(kNotificationId));

  SetAuthError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_TRUE(display_service_->GetNotification(kNotificationId));

  SetAuthError(GoogleServiceAuthError::AuthErrorNone());
  EXPECT_FALSE(display_service_->GetNotification(kNotificationId));
}

TEST_F(SigninErrorNotifierTest, ErrorTransition) {
  SetAuthError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  base::Optional<message_center::Notification> notification =
      display_service_->GetNotification(kNotificationId);
  ASSERT_TRUE(notification);
  base::string16 message = notification->message();
  EXPECT_FALSE(message.empty());

  // Now set another auth error.
  SetAuthError(GoogleServiceAuthError(
      GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE));

  notification = display_service_->GetNotification(kNotificationId);
  ASSERT_TRUE(notification);
  base::string16 new_message = notification->message();
  EXPECT_FALSE(new_message.empty());

  ASSERT_NE(new_message, message);
}

// Verify that SigninErrorNotifier ignores certain errors.
TEST_F(SigninErrorNotifierTest, AuthStatusEnumerateAllErrors) {
  typedef struct {
    GoogleServiceAuthError::State error_state;
    bool is_error;
  } ErrorTableEntry;

  ErrorTableEntry table[] = {
    { GoogleServiceAuthError::NONE, false },
    { GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS, true },
    { GoogleServiceAuthError::USER_NOT_SIGNED_UP, true },
    { GoogleServiceAuthError::CONNECTION_FAILED, false },
    { GoogleServiceAuthError::CAPTCHA_REQUIRED, true },
    { GoogleServiceAuthError::ACCOUNT_DELETED, true },
    { GoogleServiceAuthError::ACCOUNT_DISABLED, true },
    { GoogleServiceAuthError::SERVICE_UNAVAILABLE, false },
    { GoogleServiceAuthError::TWO_FACTOR, true },
    { GoogleServiceAuthError::REQUEST_CANCELED, false },
    { GoogleServiceAuthError::HOSTED_NOT_ALLOWED_DEPRECATED, false },
    { GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE, true },
    { GoogleServiceAuthError::SERVICE_ERROR, true },
    { GoogleServiceAuthError::WEB_LOGIN_REQUIRED, true },
  };
  static_assert(arraysize(table) == GoogleServiceAuthError::NUM_STATES,
      "table size should match number of auth error types");

  for (size_t i = 0; i < arraysize(table); ++i) {
    if (GoogleServiceAuthError::IsDeprecated(table[i].error_state))
      continue;

    SetAuthError(GoogleServiceAuthError(table[i].error_state));
    base::Optional<message_center::Notification> notification =
        display_service_->GetNotification(kNotificationId);
    ASSERT_EQ(table[i].is_error, !!notification);
    if (table[i].is_error) {
      EXPECT_FALSE(notification->title().empty());
      EXPECT_FALSE(notification->message().empty());
      EXPECT_EQ((size_t)1, notification->buttons().size());
    }
    SetAuthError(GoogleServiceAuthError::AuthErrorNone());
  }
}

}  // namespace
