// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_nsobject.h"
#import "testing/gtest_mac.h"
#import "ui/app_list/cocoa/app_list_view_window.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

// Unit test harness for Cocoa-specific interactions with the app_list.
class AppListViewWindowTest : public ui::CocoaTest {
 public:
  AppListViewWindowTest() {
    Init();
    window_.reset([[AppListViewWindow alloc] initAsBubble]);
  }

 protected:
  scoped_nsobject<AppListViewWindow> window_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListViewWindowTest);
};

// Test showing, hiding and closing the AppListViewWindow with no content.
TEST_F(AppListViewWindowTest, ShowHideCloseRelease) {
  [window_ makeKeyAndOrderFront:nil];
  [window_ close];
  [window_ setReleasedWhenClosed:YES];
  [window_ close];
  window_.reset();
}
