// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/release_notes/release_notes_storage.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class ReleaseNotesStorageTest : public testing::Test,
                                public testing::WithParamInterface<bool> {
 protected:
  ReleaseNotesStorageTest()
      : user_manager_(new FakeChromeUserManager()),
        scoped_user_manager_(
            std::unique_ptr<chromeos::FakeChromeUserManager>(user_manager_)) {}
  ~ReleaseNotesStorageTest() override {}

  std::unique_ptr<Profile> CreateProfile(std::string email) {
    AccountId account_id_ = AccountId::FromUserEmailGaiaId(email, "12345");
    user_manager_->AddUser(account_id_);
    TestingProfile::Builder builder;
    builder.SetProfileName(email);
    return builder.Build();
  }

  void SetupFeatureFlag(bool should_show_notification) {
    if (should_show_notification)
      scoped_feature_list_.InitAndEnableFeature(
          chromeos::features::kReleaseNotesNotification);
    else
      scoped_feature_list_.InitAndDisableFeature(
          chromeos::features::kReleaseNotesNotification);
  }

  FakeChromeUserManager* user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ReleaseNotesStorageTest);
};

INSTANTIATE_TEST_SUITE_P(All, ReleaseNotesStorageTest, testing::Bool());

TEST_P(ReleaseNotesStorageTest, ModifyLastRelease) {
  const bool should_show_notification = GetParam();
  SetupFeatureFlag(should_show_notification);

  std::unique_ptr<Profile> profile = CreateProfile("test@gmail.com");

  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());
  profile.get()->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone,
                                        -1);

  EXPECT_EQ(should_show_notification, release_notes_storage->ShouldNotify());
  release_notes_storage->MarkNotificationShown();
  EXPECT_NE(-1, profile.get()->GetPrefs()->GetInteger(
                    prefs::kReleaseNotesLastShownMilestone));
  EXPECT_EQ(false, release_notes_storage->ShouldNotify());
}

// Release notes sets current milestone to "last shown" after OOBE and should
// not be shown.
TEST_P(ReleaseNotesStorageTest, ShouldNotShowReleaseNotesOOBE) {
  const bool should_show_notification = GetParam();
  SetupFeatureFlag(should_show_notification);

  std::unique_ptr<Profile> profile = CreateProfile("test@gmail.com");

  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());

  EXPECT_EQ(false, release_notes_storage->ShouldNotify());
}

TEST_P(ReleaseNotesStorageTest, ShouldShowReleaseNotes) {
  const bool should_show_notification = GetParam();
  SetupFeatureFlag(should_show_notification);

  std::unique_ptr<Profile> profile = CreateProfile("test@gmail.com");

  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());
  profile.get()->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone,
                                        -1);

  EXPECT_EQ(should_show_notification, release_notes_storage->ShouldNotify());
}

TEST_P(ReleaseNotesStorageTest, ShouldNotShowReleaseNotes) {
  const bool should_show_notification = GetParam();
  SetupFeatureFlag(should_show_notification);

  std::unique_ptr<Profile> profile = CreateProfile("test@company.com");

  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());
  profile.get()->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone,
                                        -1);

  EXPECT_EQ(false, release_notes_storage->ShouldNotify());
}

TEST_P(ReleaseNotesStorageTest, ShouldShowReleaseNotesGoogler) {
  const bool should_show_notification = GetParam();
  SetupFeatureFlag(should_show_notification);

  std::unique_ptr<Profile> profile = CreateProfile("test@google.com");

  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());
  profile.get()->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone,
                                        -1);

  EXPECT_EQ(should_show_notification, release_notes_storage->ShouldNotify());
}

// Tests that when kReleaseNotesSuggestionChipTimesLeftToShow is greater than 0,
// ReleaseNotesStorage::ShouldShowSuggestionChip returns true, and when the
// value is 0 the method returns false.
TEST_P(ReleaseNotesStorageTest, ShowReleaseNotesSuggestionChip) {
  const bool should_show_notification = GetParam();
  SetupFeatureFlag(should_show_notification);
  std::unique_ptr<Profile> profile = CreateProfile("test@gmail.com");

  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());

  profile.get()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 1);
  EXPECT_EQ(should_show_notification,
            release_notes_storage->ShouldShowSuggestionChip());

  release_notes_storage->DecreaseTimesLeftToShowSuggestionChip();

  EXPECT_EQ(0, profile.get()->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));
  EXPECT_EQ(false, release_notes_storage->ShouldShowSuggestionChip());
}

}  // namespace chromeos
