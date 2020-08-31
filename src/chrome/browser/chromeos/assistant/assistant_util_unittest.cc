// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/assistant/assistant_util.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/events/devices/device_data_manager.h"

namespace assistant {
namespace {

constexpr char kTestProfileName[] = "user@gmail.com";
constexpr char kTestGaiaId[] = "1234567890";

class ScopedSpoofGoogleBrandedDevice {
 public:
  ScopedSpoofGoogleBrandedDevice() { OverrideIsGoogleDeviceForTesting(true); }
  ~ScopedSpoofGoogleBrandedDevice() { OverrideIsGoogleDeviceForTesting(false); }
};

class FakeUserManagerWithLocalState : public chromeos::FakeChromeUserManager {
 public:
  explicit FakeUserManagerWithLocalState(
      TestingProfileManager* testing_profile_manager)
      : testing_profile_manager_(testing_profile_manager),
        test_local_state_(std::make_unique<TestingPrefServiceSimple>()) {
    RegisterPrefs(test_local_state_->registry());
  }

  PrefService* GetLocalState() const override {
    return test_local_state_.get();
  }

  TestingProfileManager* testing_profile_manager() {
    return testing_profile_manager_;
  }

 private:
  // Unowned pointer.
  TestingProfileManager* const testing_profile_manager_;

  std::unique_ptr<TestingPrefServiceSimple> test_local_state_;

  DISALLOW_COPY_AND_ASSIGN(FakeUserManagerWithLocalState);
};

class ScopedLogIn {
 public:
  ScopedLogIn(
      FakeUserManagerWithLocalState* fake_user_manager,
      signin::IdentityTestEnvironment* identity_test_env,
      const AccountId& account_id,
      user_manager::UserType user_type = user_manager::USER_TYPE_REGULAR)
      : fake_user_manager_(fake_user_manager),
        identity_test_env_(identity_test_env),
        account_id_(account_id) {
    PreventAccessToDBus();
    RunSanityChecks(user_type);
    AddUser(user_type);

    fake_user_manager_->LoginUser(account_id_);

    MakeAccountAvailableAsPrimaryAccount(user_type);
  }

  ~ScopedLogIn() { fake_user_manager_->RemoveUserFromList(account_id_); }

 private:
  // Prevent access to DBus. This switch is reset in case set from test SetUp
  // due massive usage of InitFromArgv.
  void PreventAccessToDBus() {
    base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
    if (!command_line.HasSwitch(switches::kTestType))
      command_line.AppendSwitch(switches::kTestType);
  }

  void MakeAccountAvailableAsPrimaryAccount(user_manager::UserType user_type) {
    // Guest user can never be a primary account.
    if (user_type == user_manager::USER_TYPE_GUEST)
      return;

    if (!identity_test_env_->identity_manager()->HasPrimaryAccount(
            signin::ConsentLevel::kNotRequired)) {
      identity_test_env_->MakeUnconsentedPrimaryAccountAvailable(
          account_id_.GetUserEmail());
    }
  }

  // Run sanity checks ensuring the account id is valid for the given user type.
  // If these checks go off your test is testing something that can not happen.
  void RunSanityChecks(user_manager::UserType user_type) const {
    switch (user_type) {
      case user_manager::USER_TYPE_REGULAR:
      case user_manager::USER_TYPE_CHILD:
        EXPECT_TRUE(IsGaiaAccount());
        return;
      case user_manager::USER_TYPE_SUPERVISED:
      case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
      case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
      case user_manager::USER_TYPE_KIOSK_APP:
      case user_manager::USER_TYPE_ARC_KIOSK_APP:
      case user_manager::USER_TYPE_WEB_KIOSK_APP:
        EXPECT_FALSE(IsGaiaAccount());
        return;
      case user_manager::USER_TYPE_GUEST:
        // Guest user must use the guest user account id.
        EXPECT_EQ(account_id_, fake_user_manager_->GetGuestAccountId());
        return;
      case user_manager::NUM_USER_TYPES:
        NOTREACHED();
    }
  }

  void AddUser(user_manager::UserType user_type) {
    switch (user_type) {
      case user_manager::USER_TYPE_REGULAR:
        fake_user_manager_->AddUser(account_id_);
        return;
      case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
        fake_user_manager_->AddActiveDirectoryUser(account_id_);
        return;
      case user_manager::USER_TYPE_SUPERVISED:
        fake_user_manager_->AddSupervisedUser(account_id_);
        return;
      case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
        fake_user_manager_->AddPublicAccountUser(account_id_);
        return;
      case user_manager::USER_TYPE_KIOSK_APP:
        fake_user_manager_->AddKioskAppUser(account_id_);
        return;
      case user_manager::USER_TYPE_ARC_KIOSK_APP:
        fake_user_manager_->AddArcKioskAppUser(account_id_);
        return;
      case user_manager::USER_TYPE_WEB_KIOSK_APP:
        fake_user_manager_->AddWebKioskAppUser(account_id_);
        return;
      case user_manager::USER_TYPE_CHILD:
        fake_user_manager_->AddChildUser(account_id_);
        return;
      case user_manager::USER_TYPE_GUEST:
        fake_user_manager_->AddGuestUser();
        return;
      case user_manager::NUM_USER_TYPES:
        NOTREACHED();
    }
  }

  bool IsGaiaAccount() const {
    return account_id_.GetAccountType() == AccountType::GOOGLE;
  }

  FakeUserManagerWithLocalState* fake_user_manager_;
  signin::IdentityTestEnvironment* identity_test_env_;
  const AccountId account_id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLogIn);
};

}  // namespace

class ChromeAssistantUtilTest : public testing::Test {
 public:
  ChromeAssistantUtilTest() = default;
  ~ChromeAssistantUtilTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    profile_ = profile_manager_->CreateTestingProfile(
        kTestProfileName, /*prefs=*/{}, base::UTF8ToUTF16(kTestProfileName),
        /*avatar_id=*/0, /*supervised_user_id=*/{},
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<FakeUserManagerWithLocalState>(
            profile_manager_.get()));

    ui::DeviceDataManager::CreateInstance();
  }

  void TearDown() override {
    ui::DeviceDataManager::DeleteInstance();
    identity_test_env_adaptor_.reset();
    user_manager_enabler_.reset();
    profile_manager_->DeleteTestingProfile(kTestProfileName);
    profile_ = nullptr;
    profile_manager_.reset();
  }

  TestingProfile* profile() { return profile_; }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  FakeUserManagerWithLocalState* GetFakeUserManager() const {
    return static_cast<FakeUserManagerWithLocalState*>(
        user_manager::UserManager::Get());
  }

  AccountId GetActiveDirectoryUserAccountId(const TestingProfile* profile) {
    return AccountId::AdFromUserEmailObjGuid(profile->GetProfileUserName(),
                                             "<obj_guid>");
  }

  AccountId GetNonGaiaUserAccountId(const TestingProfile* profile) {
    return AccountId::FromUserEmail(profile->GetProfileUserName());
  }

  AccountId GetGaiaUserAccountId(const TestingProfile* profile) {
    return AccountId::FromUserEmailGaiaId(profile->GetProfileUserName(),
                                          kTestGaiaId);
  }

  AccountId GetGaiaUserAccountId(const std::string& user_name,
                                 const std::string& gaia_id) {
    return AccountId::FromUserEmailGaiaId(user_name, gaia_id);
  }

  AccountId GetGuestAccountId() {
    return GetFakeUserManager()->GetGuestAccountId();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  // Owned by |profile_manager_|
  TestingProfile* profile_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ChromeAssistantUtilTest);
};

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_PrimaryUser) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetGaiaUserAccountId(profile()));

  EXPECT_EQ(chromeos::assistant::AssistantAllowedState::ALLOWED,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_SecondaryUser) {
  ScopedLogIn secondary_user_login(
      GetFakeUserManager(), identity_test_env(),
      GetGaiaUserAccountId("user2@gmail.com", "0123456789"));
  ScopedLogIn primary_user_login(GetFakeUserManager(), identity_test_env(),
                                 GetGaiaUserAccountId(profile()));

  EXPECT_EQ(
      chromeos::assistant::AssistantAllowedState::DISALLOWED_BY_NONPRIMARY_USER,
      IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_SupervisedUser) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetNonGaiaUserAccountId(profile()),
                    user_manager::USER_TYPE_SUPERVISED);

  profile()->SetSupervisedUserId("foo");

  EXPECT_EQ(
      chromeos::assistant::AssistantAllowedState::DISALLOWED_BY_SUPERVISED_USER,
      IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_ChildUser) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetGaiaUserAccountId(profile()),
                    user_manager::USER_TYPE_CHILD);

  EXPECT_EQ(chromeos::assistant::AssistantAllowedState::ALLOWED,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_GuestUser) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetGuestAccountId(), user_manager::USER_TYPE_GUEST);

  EXPECT_EQ(
      chromeos::assistant::AssistantAllowedState::DISALLOWED_BY_NONPRIMARY_USER,
      IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_Locale) {
  profile()->GetTestingPrefService()->SetString(
      language::prefs::kApplicationLocale, "he");
  UErrorCode error_code = U_ZERO_ERROR;
  const icu::Locale& old_locale = icu::Locale::getDefault();
  icu::Locale::setDefault(icu::Locale("he"), error_code);
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetGaiaUserAccountId(profile()));

  EXPECT_EQ(chromeos::assistant::AssistantAllowedState::DISALLOWED_BY_LOCALE,
            IsAssistantAllowedForProfile(profile()));
  icu::Locale::setDefault(old_locale, error_code);
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_DemoMode) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOnline);
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetNonGaiaUserAccountId(profile()),
                    user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  EXPECT_EQ(chromeos::assistant::AssistantAllowedState::DISALLOWED_BY_DEMO_MODE,
            IsAssistantAllowedForProfile(profile()));

  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kNone);
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_PublicSession) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetNonGaiaUserAccountId(profile()),
                    user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  EXPECT_EQ(
      chromeos::assistant::AssistantAllowedState::DISALLOWED_BY_PUBLIC_SESSION,
      IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_NonGmail) {
  ScopedLogIn login(
      GetFakeUserManager(), identity_test_env(),
      GetGaiaUserAccountId("user2@someotherdomain.com", "0123456789"));

  EXPECT_EQ(
      chromeos::assistant::AssistantAllowedState::DISALLOWED_BY_ACCOUNT_TYPE,
      IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_GoogleMail) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetGaiaUserAccountId("user2@googlemail.com", "0123456789"));

  EXPECT_EQ(chromeos::assistant::AssistantAllowedState::ALLOWED,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest,
       IsAssistantAllowed_AllowsNonGmailOnGoogleBrandedDevices) {
  ScopedLogIn login(
      GetFakeUserManager(), identity_test_env(),
      GetGaiaUserAccountId("user2@someotherdomain.com", "0123456789"));

  ScopedSpoofGoogleBrandedDevice make_google_branded_device;
  EXPECT_EQ(chromeos::assistant::AssistantAllowedState::ALLOWED,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest,
       IsAssistantAllowedForProfile_ActiveDirectoryUser) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetActiveDirectoryUserAccountId(profile()),
                    user_manager::USER_TYPE_ACTIVE_DIRECTORY);

  EXPECT_EQ(
      chromeos::assistant::AssistantAllowedState::DISALLOWED_BY_ACCOUNT_TYPE,
      IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForKiosk_KioskApp) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetNonGaiaUserAccountId(profile()),
                    user_manager::USER_TYPE_KIOSK_APP);

  EXPECT_EQ(
      chromeos::assistant::AssistantAllowedState::DISALLOWED_BY_KIOSK_MODE,
      IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForKiosk_ArcKioskApp) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetNonGaiaUserAccountId(profile()),
                    user_manager::USER_TYPE_ARC_KIOSK_APP);

  EXPECT_EQ(
      chromeos::assistant::AssistantAllowedState::DISALLOWED_BY_KIOSK_MODE,
      IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForKiosk_WebKioskApp) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetNonGaiaUserAccountId(profile()),
                    user_manager::USER_TYPE_WEB_KIOSK_APP);

  EXPECT_EQ(
      chromeos::assistant::AssistantAllowedState::DISALLOWED_BY_KIOSK_MODE,
      IsAssistantAllowedForProfile(profile()));
}

}  // namespace assistant
