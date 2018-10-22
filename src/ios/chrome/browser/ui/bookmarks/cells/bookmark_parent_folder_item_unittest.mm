// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_parent_folder_item.h"

#include "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using BookmarkParentFolderItemTest = PlatformTest;

TEST_F(BookmarkParentFolderItemTest, LabelGetsTitleUIRefreshDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kUIRefreshPhase1);

  BookmarkParentFolderItem* item =
      [[BookmarkParentFolderItem alloc] initWithType:0];
  LegacyBookmarkParentFolderCell* cell =
      [[LegacyBookmarkParentFolderCell alloc] initWithFrame:CGRectZero];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];

  item.title = @"Foo";
  [item configureCell:cell withStyler:styler];
  EXPECT_NSEQ(@"Foo", cell.parentFolderNameLabel.text);
}

TEST_F(BookmarkParentFolderItemTest, LabelGetsTitleUIRefreshEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kUIRefreshPhase1);

  BookmarkParentFolderItem* item =
      [[BookmarkParentFolderItem alloc] initWithType:0];
  BookmarkParentFolderCell* cell =
      [[BookmarkParentFolderCell alloc] initWithFrame:CGRectZero];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];

  item.title = @"Foo";
  [item configureCell:cell withStyler:styler];
  EXPECT_NSEQ(@"Foo", cell.parentFolderNameLabel.text);
}

}  // namespace
