// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsobject.h"
#import "testing/gtest_mac.h"
#import "ui/app_list/cocoa/app_list_view_controller.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"
#import "ui/app_list/cocoa/test/apps_grid_controller_test_helper.h"
#include "ui/app_list/signin_delegate.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"

namespace app_list {
namespace test {

class AppListViewControllerTest : public AppsGridControllerTestHelper,
                                  public SigninDelegate {
 public:
  AppListViewControllerTest() {}

  virtual void SetUp() OVERRIDE {
    app_list_view_controller_.reset([[AppListViewController alloc] init]);
    SetUpWithGridController([app_list_view_controller_ appsGridController]);
    [[test_window() contentView] addSubview:[app_list_view_controller_ view]];
  }

  virtual void TearDown() OVERRIDE {
    [app_list_view_controller_
        setDelegate:scoped_ptr<app_list::AppListViewDelegate>()];
    app_list_view_controller_.reset();
    AppsGridControllerTestHelper::TearDown();
  }

  virtual void ResetModel(scoped_ptr<AppListModel> new_model) OVERRIDE {
    scoped_ptr<AppListTestViewDelegate> delegate(new AppListTestViewDelegate);
    delegate->set_test_signin_delegate(this);
    [app_list_view_controller_
          setDelegate:delegate.PassAs<AppListViewDelegate>()
        withTestModel:new_model.Pass()];
  }

  // SigninDelegate overrides:
  virtual bool NeedSignin() OVERRIDE { return false; }
  virtual void ShowSignin() OVERRIDE {}
  virtual void OpenLearnMore() OVERRIDE {}
  virtual void OpenSettings() OVERRIDE {}

  virtual base::string16 GetSigninHeading() OVERRIDE {
    return base::string16();
  }
  virtual base::string16 GetSigninText() OVERRIDE { return base::string16(); }
  virtual base::string16 GetSigninButtonText() OVERRIDE {
    return base::string16();
  }
  virtual base::string16 GetLearnMoreLinkText() OVERRIDE {
    return base::string16();
  }
  virtual base::string16 GetSettingsLinkText() OVERRIDE {
    return base::string16();
  }

 protected:
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

// Test the view when the user is already signed in.
TEST_F(AppListViewControllerTest, SignedIn) {
  // There should be just 1, visible subview when signed in.
  EXPECT_EQ(1u, [[[app_list_view_controller_ view] subviews] count]);
  EXPECT_FALSE([[app_list_view_controller_ backgroundView] isHidden]);
}

// Test the view when signin is required.
TEST_F(AppListViewControllerTest, NeedsSignin) {
  // Begin the test with a signed out app list.
  scoped_ptr<AppListModel> new_model(new AppListModel);
  new_model->SetSignedIn(false);
  ResetModel(new_model.Pass());
  EXPECT_EQ(2u, [[[app_list_view_controller_ view] subviews] count]);
  EXPECT_TRUE([[app_list_view_controller_ backgroundView] isHidden]);

  // Simulate signing in, should enter the SignedIn state.
  model()->SetSignedIn(true);
  EXPECT_EQ(1u, [[[app_list_view_controller_ view] subviews] count]);
  EXPECT_FALSE([[app_list_view_controller_ backgroundView] isHidden]);
}

}  // namespace test
}  // namespace app_list
