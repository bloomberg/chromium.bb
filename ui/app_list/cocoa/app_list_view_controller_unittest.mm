// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsobject.h"
#import "testing/gtest_mac.h"
#import "ui/app_list/cocoa/app_list_view_controller.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"
#import "ui/app_list/cocoa/test/apps_grid_controller_test_helper.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"

namespace app_list {
namespace test {

class AppListViewControllerTest : public AppsGridControllerTestHelper {
 public:
  AppListViewControllerTest() {}

  virtual void SetUp() OVERRIDE {
    app_list_view_controller_.reset([[AppListViewController alloc] init]);
    delegate_.reset(new AppListTestViewDelegate);
    [app_list_view_controller_ setDelegate:delegate_.get()];
    SetUpWithGridController([app_list_view_controller_ appsGridController]);
    [[test_window() contentView] addSubview:[app_list_view_controller_ view]];
  }

  virtual void TearDown() OVERRIDE {
    [app_list_view_controller_ setDelegate:NULL];
    app_list_view_controller_.reset();
    AppsGridControllerTestHelper::TearDown();
  }

  void ReplaceTestModel(int item_count) {
    [app_list_view_controller_ setDelegate:NULL];
    delegate_.reset(new AppListTestViewDelegate);
    delegate_->ReplaceTestModel(item_count);
    [app_list_view_controller_ setDelegate:delegate_.get()];
  }

  AppListTestModel* model() { return delegate_->GetTestModel(); }

 protected:
  scoped_ptr<AppListTestViewDelegate> delegate_;
  base::scoped_nsobject<AppListViewController> app_list_view_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListViewControllerTest);
};

TEST_VIEW(AppListViewControllerTest, [app_list_view_controller_ view]);

// Test that adding and removing pages updates the pager.
TEST_F(AppListViewControllerTest, PagerSegmentCounts) {
  NSSegmentedControl* pager = [app_list_view_controller_ pagerControl];
  EXPECT_EQ(1, [pager segmentCount]);

  ReplaceTestModel(kItemsPerPage * 2);
  EXPECT_EQ(2, [pager segmentCount]);
  model()->PopulateApps(1);
  EXPECT_EQ(3, [pager segmentCount]);

  ReplaceTestModel(1);
  EXPECT_EQ(1, [pager segmentCount]);
}

// Test that clicking the pager changes pages.
TEST_F(AppListViewControllerTest, PagerChangingPage) {
  NSSegmentedControl* pager = [app_list_view_controller_ pagerControl];
  ReplaceTestModel(kItemsPerPage * 3);
  EXPECT_EQ(3, [pager segmentCount]);

  EXPECT_EQ(0, [pager selectedSegment]);
  EXPECT_EQ(0u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(1.0, [apps_grid_controller_ visiblePortionOfPage:0]);
  EXPECT_EQ(0.0, [apps_grid_controller_ visiblePortionOfPage:1]);

  // Emulate a click on the second segment to navigate to the second page.
  [pager setSelectedSegment:1];
  [[pager target] performSelector:[pager action]
                       withObject:pager];

  EXPECT_EQ(1u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(1, [pager selectedSegment]);
  EXPECT_EQ(0.0, [apps_grid_controller_ visiblePortionOfPage:0]);
  EXPECT_EQ(1.0, [apps_grid_controller_ visiblePortionOfPage:1]);

  // Replace with a single page model, and ensure we go back to the first page.
  ReplaceTestModel(1);
  EXPECT_EQ(0u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(0, [pager selectedSegment]);
  EXPECT_EQ(1, [pager segmentCount]);
  EXPECT_EQ(1.0, [apps_grid_controller_ visiblePortionOfPage:0]);
}

}  // namespace test
}  // namespace app_list
