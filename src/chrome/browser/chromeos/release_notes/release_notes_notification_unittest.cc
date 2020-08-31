// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/release_notes/release_notes_notification.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/testing_pref_service.h"

namespace chromeos {

class ReleaseNotesNotificationTest : public BrowserWithTestWindowTest {
 public:
  ReleaseNotesNotificationTest() {}
  ~ReleaseNotesNotificationTest() override = default;

  // BrowserWithTestWindowTest:
  TestingProfile* CreateProfile() override {
    return profile_manager()->CreateTestingProfile("googler@google.com");
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    tester_ = std::make_unique<NotificationDisplayServiceTester>(nullptr);
    tester_->SetNotificationAddedClosure(
        base::BindRepeating(&ReleaseNotesNotificationTest::OnNotificationAdded,
                            base::Unretained(this)));
    release_notes_notification_ =
        std::make_unique<ReleaseNotesNotification>(profile());
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kReleaseNotesNotification);
  }

  void TearDown() override {
    release_notes_notification_.reset();
    tester_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  void OnNotificationAdded() { notification_count_++; }

 protected:
  bool HasReleaseNotesNotification() {
    return tester_->GetNotification("show_release_notes_notification")
        .has_value();
  }

  int notification_count_ = 0;
  std::unique_ptr<ReleaseNotesNotification> release_notes_notification_;

 private:
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ReleaseNotesNotificationTest);
};

TEST_F(ReleaseNotesNotificationTest, DoNotShowReleaseNotesNotification) {
  release_notes_notification_->MaybeShowReleaseNotes();
  EXPECT_EQ(false, HasReleaseNotesNotification());
  EXPECT_EQ(0, notification_count_);
}

TEST_F(ReleaseNotesNotificationTest, ShowReleaseNotesNotification) {
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile());
  profile()->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone, -1);
  release_notes_notification_->MaybeShowReleaseNotes();
  EXPECT_EQ(true, HasReleaseNotesNotification());
  EXPECT_EQ(1, notification_count_);
}

}  // namespace chromeos
