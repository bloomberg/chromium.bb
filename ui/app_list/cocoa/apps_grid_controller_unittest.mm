// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_nsobject.h"
#include "base/message_loop.h"
#import "testing/gtest_mac.h"
#include "ui/app_list/app_list_item_model.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#import "ui/base/test/cocoa_test_event_utils.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

namespace {

class AppsGridControllerTest : public ui::CocoaTest {
 public:
  AppsGridControllerTest() {}

  virtual void SetUp() OVERRIDE {
    ui::CocoaTest::SetUp();
    scoped_ptr<app_list::AppListViewDelegate> delegate(
        new app_list::test::AppListTestViewDelegate);
    apps_grid_controller_.reset([[AppsGridController alloc]
        initWithViewDelegate:delegate.Pass()]);

    scoped_ptr<app_list::AppListModel> model(
        new app_list::test::AppListTestModel);
    [apps_grid_controller_ setModel:model.Pass()];

    [[test_window() contentView] addSubview:[apps_grid_controller_ view]];
  }

 protected:
  // Send a click to the test window in the centre of |view|.
  void SimulateClick(NSView* view) {
    std::pair<NSEvent*, NSEvent*> events(
        cocoa_test_event_utils::MouseClickInView(view, 1));
    [NSApp postEvent:events.first atStart:NO];
    [NSApp postEvent:events.second atStart:NO];
  }

  app_list::test::AppListTestViewDelegate* delegate() {
    return static_cast<app_list::test::AppListTestViewDelegate*>(
        [apps_grid_controller_ delegate]);
  }

  app_list::test::AppListTestModel* model() {
    return static_cast<app_list::test::AppListTestModel*>(
        [apps_grid_controller_ model]);
  }

  scoped_nsobject<AppsGridController> apps_grid_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppsGridControllerTest);
};

}  // namespace

TEST_VIEW(AppsGridControllerTest, [apps_grid_controller_ view]);

// Test showing with an empty model.
TEST_F(AppsGridControllerTest, EmptyModelAndShow) {
  EXPECT_TRUE([[apps_grid_controller_ view] superview]);
  EXPECT_TRUE([[apps_grid_controller_ collectionView] superview]);
  EXPECT_EQ(0u, [[[apps_grid_controller_ collectionView] content] count]);
}

// Test with a single item.
// This test is disabled in builders until the delay to wait for the animations
// can be removed, or some other solution is found.
TEST_F(AppsGridControllerTest, DISABLED_SingleEntryModel) {
  const size_t kTotalItems = 1;
  MessageLoopForUI message_loop;

  EXPECT_EQ(0u, [[[apps_grid_controller_ collectionView] content] count]);

  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext] setDuration:0.0];
  model()->PopulateApps(kTotalItems);
  [NSAnimationContext endGrouping];

  EXPECT_EQ(kTotalItems,
            [[[apps_grid_controller_ collectionView] content] count]);

  message_loop.PostDelayedTask(FROM_HERE,
                               MessageLoop::QuitClosure(),
                               base::TimeDelta::FromMilliseconds(100));
  message_loop.Run();

  NSArray* subviews = [[apps_grid_controller_ collectionView] subviews];
  EXPECT_EQ(kTotalItems, [subviews count]);
  NSView* subview = [subviews objectAtIndex:0];

  // Launch the item.
  SimulateClick(subview);
  message_loop.PostTask(FROM_HERE, MessageLoop::QuitClosure());
  message_loop.Run();
  EXPECT_EQ(1, delegate()->activate_count());
  EXPECT_EQ(std::string("Item 0"), delegate()->last_activated()->title());
}

// Test setModel.
TEST_F(AppsGridControllerTest, ReplaceModel) {
  const size_t kOrigItems = 1;
  const size_t kNewItems = 2;

  model()->PopulateApps(kOrigItems);
  EXPECT_EQ(kOrigItems,
            [[[apps_grid_controller_ collectionView] content] count]);

  scoped_ptr<app_list::test::AppListTestModel> new_model(
      new app_list::test::AppListTestModel);
  new_model->PopulateApps(kNewItems);
  [apps_grid_controller_ setModel:new_model.PassAs<app_list::AppListModel>()];
  EXPECT_EQ(kNewItems,
            [[[apps_grid_controller_ collectionView] content] count]);
}
