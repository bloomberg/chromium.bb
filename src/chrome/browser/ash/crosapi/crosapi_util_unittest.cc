// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/crosapi_util.h"

#include <string>

#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using user_manager::User;

namespace crosapi {

class CrosapiUtilTest : public testing::Test {
 public:
  CrosapiUtilTest() = default;
  ~CrosapiUtilTest() override = default;

  void SetUp() override {
    fake_user_manager_ = new ash::FakeChromeUserManager;
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_));
    browser_util::RegisterLocalStatePrefs(pref_service_.registry());
  }

  void AddRegularUser(const std::string& email) {
    AccountId account_id = AccountId::FromUserEmail(email);
    const User* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, &testing_profile_);
  }

  // The order of these members is relevant for both construction and
  // destruction timing.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
  ash::FakeChromeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(CrosapiUtilTest, GetInterfaceVersions) {
  base::flat_map<base::Token, uint32_t> versions =
      browser_util::GetInterfaceVersions();

  // Check that a known interface with version > 0 is present and has non-zero
  // version.
  EXPECT_GT(versions[mojom::KeystoreService::Uuid_], 0);

  // Check that the empty token is not present.
  base::Token token;
  auto it = versions.find(token);
  EXPECT_EQ(it, versions.end());
}

TEST_F(CrosapiUtilTest, IsSigninProfileOrBelongsToAffiliatedUserSigninProfile) {
  TestingProfile::Builder builder;
  builder.SetPath(base::FilePath(FILE_PATH_LITERAL(chrome::kInitialProfile)));
  std::unique_ptr<Profile> signin_profile = builder.Build();

  EXPECT_TRUE(browser_util::IsSigninProfileOrBelongsToAffiliatedUser(
      signin_profile.get()));
}

TEST_F(CrosapiUtilTest, IsSigninProfileOrBelongsToAffiliatedUserOffTheRecord) {
  Profile* otr_profile = testing_profile_.GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);

  EXPECT_FALSE(
      browser_util::IsSigninProfileOrBelongsToAffiliatedUser(otr_profile));
}

TEST_F(CrosapiUtilTest,
       IsSigninProfileOrBelongsToAffiliatedUserAffiliatedUser) {
  AccountId account_id = AccountId::FromUserEmail("user@test.com");
  const User* user = fake_user_manager_->AddUserWithAffiliation(
      account_id, /*is_affiliated=*/true);
  fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                   /*browser_restart=*/false,
                                   /*is_child=*/false);
  chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
      user, &testing_profile_);

  EXPECT_TRUE(browser_util::IsSigninProfileOrBelongsToAffiliatedUser(
      &testing_profile_));
}

TEST_F(CrosapiUtilTest,
       IsSigninProfileOrBelongsToAffiliatedUserNotAffiliatedUser) {
  AddRegularUser("user@test.com");

  EXPECT_FALSE(browser_util::IsSigninProfileOrBelongsToAffiliatedUser(
      &testing_profile_));
}

TEST_F(CrosapiUtilTest,
       IsSigninProfileOrBelongsToAffiliatedUserLockScreenProfile) {
  TestingProfile::Builder builder;
  builder.SetPath(
      base::FilePath(FILE_PATH_LITERAL(chrome::kLockScreenProfile)));
  std::unique_ptr<Profile> lock_screen_profile = builder.Build();

  EXPECT_FALSE(browser_util::IsSigninProfileOrBelongsToAffiliatedUser(
      lock_screen_profile.get()));
}

TEST_F(CrosapiUtilTest, EmptyDeviceSettings) {
  auto settings = browser_util::GetDeviceSettings();
  EXPECT_EQ(settings->attestation_for_content_protection_enabled,
            crosapi::mojom::DeviceSettings::OptionalBool::kUnset);
  EXPECT_EQ(settings->device_system_wide_tracing_enabled,
            crosapi::mojom::DeviceSettings::OptionalBool::kUnset);
}

TEST_F(CrosapiUtilTest, DeviceSettingsWithData) {
  testing_profile_.ScopedCrosSettingsTestHelper()
      ->ReplaceDeviceSettingsProviderWithStub();
  testing_profile_.ScopedCrosSettingsTestHelper()->SetTrustedStatus(
      ash::CrosSettingsProvider::TRUSTED);
  base::RunLoop().RunUntilIdle();
  testing_profile_.ScopedCrosSettingsTestHelper()
      ->GetStubbedProvider()
      ->SetBoolean(ash::kAttestationForContentProtectionEnabled, true);

  base::Value allowlist(base::Value::Type::LIST);
  base::Value ids(base::Value::Type::DICTIONARY);
  ids.SetIntKey(ash::kUsbDetachableAllowlistKeyVid, 2);
  ids.SetIntKey(ash::kUsbDetachableAllowlistKeyPid, 3);
  allowlist.Append(std::move(ids));

  testing_profile_.ScopedCrosSettingsTestHelper()->GetStubbedProvider()->Set(
      ash::kUsbDetachableAllowlist, std::move(allowlist));

  auto settings = browser_util::GetDeviceSettings();
  testing_profile_.ScopedCrosSettingsTestHelper()
      ->RestoreRealDeviceSettingsProvider();

  EXPECT_EQ(settings->attestation_for_content_protection_enabled,
            crosapi::mojom::DeviceSettings::OptionalBool::kTrue);
  EXPECT_EQ(settings->usb_detachable_allow_list->usb_device_ids.size(), 1);
  EXPECT_EQ(
      settings->usb_detachable_allow_list->usb_device_ids[0]->has_vendor_id,
      true);
  EXPECT_EQ(settings->usb_detachable_allow_list->usb_device_ids[0]->vendor_id,
            2);
  EXPECT_EQ(
      settings->usb_detachable_allow_list->usb_device_ids[0]->has_product_id,
      true);
  EXPECT_EQ(settings->usb_detachable_allow_list->usb_device_ids[0]->product_id,
            3);
}

}  // namespace crosapi
