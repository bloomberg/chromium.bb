// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_nsobject.h"
#import "testing/gtest_mac.h"
#import "ui/app_list/cocoa/app_list_view.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

namespace {

class AppListViewTest : public ui::CocoaTest {
 public:
  AppListViewTest() {}

  virtual void SetUp() OVERRIDE {
    ui::CocoaTest::SetUp();
    view_.reset([[AppListView alloc] initWithViewDelegate:NULL]);
    [[test_window() contentView] addSubview:view_];
  }

 protected:
  scoped_nsobject<AppListView> view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListViewTest);
};

}  // namespace

TEST_VIEW(AppListViewTest, view_);

// Test allocating the view with no delegate or model, and showing.
TEST_F(AppListViewTest, AddViewAndShow) {
  EXPECT_TRUE([view_ superview]);
  [view_ display];
}
