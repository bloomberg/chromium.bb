// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activities/bookmark_activity.h"

#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "ios/chrome/browser/policy/policy_features.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_ios_unittest.h"
#include "ios/chrome/browser/ui/commands/browser_commands.h"
#include "ios/chrome/grit/ios_strings.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for covering the BookmarkActivity class.
class BookmarkActivityTest : public BookmarkIOSUnitTest {
 protected:
  BookmarkActivityTest() {}

  void SetUp() override {
    BookmarkIOSUnitTest::SetUp();

    // Turn off flag by default.
    scoped_features_.InitAndDisableFeature(kEditBookmarksIOS);

    mocked_handler_ = OCMProtocolMock(@protocol(BrowserCommands));

    RegisterPrefs();
  }

  // Registers the edit bookmarks pref.
  void RegisterPrefs() {
    testing_pref_service_.registry()->RegisterBooleanPref(
        bookmarks::prefs::kEditBookmarksEnabled, true);
  }

  // Sets the edit bookmarks pref to |canEdit|.
  void SetCanEditBookmarkPref(bool canEdit) {
    testing_pref_service_.SetBoolean(bookmarks::prefs::kEditBookmarksEnabled,
                                     canEdit);
  }

  // Creates a BookmarkActivity instance with the given |URL|.
  BookmarkActivity* CreateActivity(const GURL& URL) {
    return [[BookmarkActivity alloc] initWithURL:URL
                                   bookmarkModel:bookmark_model_
                                         handler:mocked_handler_
                                     prefService:&testing_pref_service_];
  }

  base::test::ScopedFeatureList scoped_features_;
  TestingPrefServiceSimple testing_pref_service_;
  id mocked_handler_;
};

// Tests that the activity can always be performed when the kEditBookmarksIOS
// feature flag is disabled.
TEST_F(BookmarkActivityTest, FlagOff_ActivityAlwaysAvailable) {
  BookmarkActivity* activity = CreateActivity(GURL());

  // Flag Off, Editable bookmark pref true.
  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);

  SetCanEditBookmarkPref(false);

  // Flag off, Editable bookmark pref false.
  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);
}

// Tests that, when the kEditBookmarksIOS is enabled, the activity can only be
// performed if the preferences indicate that bookmarks can be edited.
TEST_F(BookmarkActivityTest, FlagOn_ActivityHiddenByPref) {
  BookmarkActivity* activity = CreateActivity(GURL());
  scoped_features_.Reset();
  scoped_features_.InitAndEnableFeature(kEditBookmarksIOS);

  // Flag On, Editable bookmark pref true.
  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);

  SetCanEditBookmarkPref(false);

  // Flag On, Editable bookmark pref false.
  EXPECT_FALSE([activity canPerformWithActivityItems:@[]]);
}

// Tests that passing a nil bookmarkModel won't crash the activity.
TEST_F(BookmarkActivityTest, NilBookmarkModel_NoCrash) {
  BookmarkActivity* activity =
      [[BookmarkActivity alloc] initWithURL:GURL("https://example.com/")
                              bookmarkModel:nil
                                    handler:mocked_handler_
                                prefService:&testing_pref_service_];

  EXPECT_FALSE([activity canPerformWithActivityItems:@[]]);
}

// Tests that the title of the activity is add when URL is not bookmarked.
TEST_F(BookmarkActivityTest, ActivityTitle_AddBookmark) {
  GURL testUrl("https://example.com/");
  BookmarkActivity* activity = CreateActivity(testUrl);

  NSString* addBookmarkString =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_ADD_TO_BOOKMARKS);
  EXPECT_TRUE([addBookmarkString isEqualToString:activity.activityTitle]);
}

// Tests that the title of the activity is edit when URL is already bookmarked.
TEST_F(BookmarkActivityTest, ActivityTitle_EditBookmark) {
  // Add a bookmark.
  const bookmarks::BookmarkNode* bookmark =
      AddBookmark(bookmark_model_->mobile_node(), @"activity_test");
  ASSERT_TRUE(bookmark_model_->IsBookmarked(bookmark->url()));

  BookmarkActivity* activity = CreateActivity(bookmark->url());

  NSString* editBookmarkString =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_EDIT_BOOKMARK);
  EXPECT_TRUE([editBookmarkString isEqualToString:activity.activityTitle]);
}
