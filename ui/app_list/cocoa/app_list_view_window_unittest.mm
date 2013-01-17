// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/memory/scoped_nsobject.h"
#import "testing/gtest_mac.h"
#import "ui/app_list/cocoa/app_list_view.h"
#import "ui/app_list/cocoa/app_list_view_window.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

namespace {

class AppListViewWindowTest : public ui::CocoaTest {
 public:
  AppListViewWindowTest();

  void Show();
  void Release();

 protected:
  scoped_nsobject<AppListViewWindow> window_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListViewWindowTest);
};

AppListViewWindowTest::AppListViewWindowTest() {
  Init();
  window_.reset([[AppListViewWindow alloc] initAsBubble]);
}

void AppListViewWindowTest::Show() {
  [window_ makeKeyAndOrderFront:nil];
}

void AppListViewWindowTest::Release() {
  EXPECT_TRUE(window_.get());
  EXPECT_FALSE([window_ isReleasedWhenClosed]);
  [window_ close];
  window_.reset();
}

}  // namespace

// Test showing, hiding and closing the AppListViewWindow with no content.
TEST_F(AppListViewWindowTest, ShowHideCloseRelease) {
  Show();
  EXPECT_TRUE([window_ isVisible]);
  [window_ close];  // Hide.
  EXPECT_FALSE([window_ isVisible]);
  Show();
  Release();
}

// Test allocating the AppListView in an AppListViewWindow, and showing.
TEST_F(AppListViewWindowTest, AddViewAndShow) {
  scoped_nsobject<AppListView> contentView(
      [[AppListView alloc] initWithViewDelegate:NULL]);
  [window_ setAppListView:contentView];
  Show();
  EXPECT_TRUE([contentView superview]);
  Release();
}
