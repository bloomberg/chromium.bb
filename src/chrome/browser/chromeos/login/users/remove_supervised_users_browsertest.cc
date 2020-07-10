// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/common/chrome_features.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_base.h"

namespace chromeos {

namespace {

struct {
  const char* email;
  const char* gaia_id;
} const kTestUsers[] = {
    {"test-user1@gmail.com", "1111111111"},
    {"test-user2@gmail.com", "2222222222"},
    // Test Supervised User.
    // The domain is defined in user_manager::kSupervisedUserDomain.
    // That const isn't directly referenced here to keep this code readable by
    // avoiding std::string concatenations.
    {"test-superviseduser@locally-managed.localhost", "3333333333"},
};

}  // namespace

class RemoveSupervisedUsersBrowserTest : public LoginManagerTest {
 public:
  RemoveSupervisedUsersBrowserTest() : LoginManagerTest(false, false) {}

  ~RemoveSupervisedUsersBrowserTest() override = default;

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    InitializeTestUsers();
  }

  void TearDownOnMainThread() override {
    LoginManagerTest::TearDownOnMainThread();
    scoped_user_manager_.reset();
  }

 protected:
  std::vector<AccountId> test_users_;

 private:
  void InitializeTestUsers() {
    for (size_t i = 0; i < base::size(kTestUsers); ++i) {
      test_users_.emplace_back(AccountId::FromUserEmailGaiaId(
          kTestUsers[i].email, kTestUsers[i].gaia_id));
      RegisterUser(test_users_[i]);
    }

    // Setup a scoped real ChromeUserManager, since this is an end-to-end
    // test.  This will read the users registered to the local state just
    // before this in RegisterUser.
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        chromeos::ChromeUserManagerImpl::CreateChromeUserManager());
  }

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  ScopedTestingCrosSettings scoped_testing_cros_settings_;

  DISALLOW_COPY_AND_ASSIGN(RemoveSupervisedUsersBrowserTest);
};

class RemoveSupervisedUsersBrowserEnabledTest
    : public RemoveSupervisedUsersBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kRemoveSupervisedUsersOnStartup},
        {user_manager::kHideSupervisedUsers});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class RemoveSupervisedUsersBrowserDisabledTest
    : public RemoveSupervisedUsersBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeatures(
        {}, {user_manager::kHideSupervisedUsers,
             features::kRemoveSupervisedUsersOnStartup});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(RemoveSupervisedUsersBrowserEnabledTest,
                       SupervisedUserRemoved) {
  // When Supervised users are removed, only 2 test users should be returned.
  EXPECT_EQ(2U, user_manager::UserManager::Get()->GetUsers().size());

  // None of the non-supervised users should have been removed.
  for (user_manager::User* user :
       user_manager::UserManager::Get()->GetUsers()) {
    EXPECT_EQ(false, user->IsSupervised());
  }
}

IN_PROC_BROWSER_TEST_F(RemoveSupervisedUsersBrowserDisabledTest,
                       SupervisedUserNotRemoved) {
  // When Supervised users are not removed, all 3 test users should be returned.
  EXPECT_EQ(3U, user_manager::UserManager::Get()->GetUsers().size());
}

}  // namespace chromeos
