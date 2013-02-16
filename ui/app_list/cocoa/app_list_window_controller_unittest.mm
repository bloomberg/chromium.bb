// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/memory/scoped_nsobject.h"
#import "testing/gtest_mac.h"
#import "ui/app_list/app_list_view_delegate.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"
#import "ui/app_list/cocoa/app_list_window_controller.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

namespace {

class AppListWindowControllerTest : public ui::CocoaTest {
 public:
  AppListWindowControllerTest();

 protected:
  virtual void TearDown() OVERRIDE;

  scoped_nsobject<AppListWindowController> controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListWindowControllerTest);
};

AppListWindowControllerTest::AppListWindowControllerTest() {
  Init();
  scoped_ptr<app_list::AppListViewDelegate> delegate;
  scoped_nsobject<AppsGridController> content(
      [[AppsGridController alloc] initWithViewDelegate:delegate.Pass()]);
  controller_.reset(
      [[AppListWindowController alloc] initWithGridController:content]);
}

void AppListWindowControllerTest::TearDown() {
  EXPECT_TRUE(controller_.get());
  [[controller_ window] close];
  controller_.reset();
  ui::CocoaTest::TearDown();
}

}  // namespace

// Test showing, hiding and closing the app list window.
TEST_F(AppListWindowControllerTest, ShowHideCloseRelease) {
  EXPECT_TRUE([controller_ window]);
  [[controller_ window] makeKeyAndOrderFront:nil];
  EXPECT_TRUE([[controller_ window] isVisible]);
  EXPECT_TRUE([[[controller_ window] contentView] superview]);
  [[controller_ window] close];  // Hide.
  EXPECT_FALSE([[controller_ window] isVisible]);
  [[controller_ window] makeKeyAndOrderFront:nil];
}
