// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/pin_storage_prefs.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/userdataauth/fake_userdataauth_client.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_unlock {
namespace {

constexpr char kTestUserEmail[] = "test-user@gmail.com";
constexpr char kTestUserGaiaId[] = "1234567890";

class PinMigrationTest : public LoginManagerTest {
 public:
  PinMigrationTest() = default;

  PinMigrationTest(const PinMigrationTest&) = delete;
  PinMigrationTest& operator=(const PinMigrationTest&) = delete;

  ~PinMigrationTest() override = default;

  void SetUp() override {
    // Initialize UserDataAuthClient and configure it for testing. It will be
    // destroyed in ChromeBrowserMain.
    UserDataAuthClient::InitializeFake();
    FakeUserDataAuthClient::Get()->set_supports_low_entropy_credentials(true);

    LoginManagerTest::SetUp();
  }
};

// Step 1/3: Register a new user.
IN_PROC_BROWSER_TEST_F(PinMigrationTest, PRE_PRE_Migrate) {
  RegisterUser(AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId));
  StartupUtils::MarkOobeCompleted();
}

// Step 2/3: Log the user in, add a prefs-based PIN.
IN_PROC_BROWSER_TEST_F(PinMigrationTest, PRE_Migrate) {
  AccountId test_account =
      AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
  LoginUser(test_account);

  // Register PIN.
  QuickUnlockStorage* storage =
      QuickUnlockFactory::GetForAccountId(test_account);
  ASSERT_TRUE(storage);
  storage->pin_storage_prefs()->SetPin("111111");

  // Validate PIN is set.
  base::RunLoop run_loop;
  absl::optional<bool> has_pin_result;
  PinBackend::GetInstance()->IsSet(
      test_account, base::BindLambdaForTesting([&](bool has_pin) {
        has_pin_result = has_pin;
        run_loop.Quit();
      }));
  run_loop.Run();
  ASSERT_TRUE(has_pin_result.has_value());
  EXPECT_TRUE(*has_pin_result);
}

// Step 3/3: Log in again, verify prefs-based PIN does not contain state, but
// PIN is still set.
IN_PROC_BROWSER_TEST_F(PinMigrationTest, Migrate) {
  AccountId test_account =
      AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
  LoginUser(test_account);

  // Prefs PIN is not set.
  EXPECT_FALSE(QuickUnlockFactory::GetForAccountId(test_account)
                   ->pin_storage_prefs()
                   ->IsPinSet());

  // Since prefs-based PIN is not set, calling IsSet on PinBackend will only
  // return true if the PIN is set in cryptohome.
  base::RunLoop run_loop;
  absl::optional<bool> has_pin_result;
  PinBackend::GetInstance()->IsSet(
      test_account, base::BindLambdaForTesting([&](bool has_pin) {
        has_pin_result = has_pin;
        run_loop.Quit();
      }));
  run_loop.Run();
  ASSERT_TRUE(has_pin_result.has_value());
  EXPECT_TRUE(*has_pin_result);
}

}  // namespace
}  // namespace quick_unlock
}  // namespace ash
