// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/status_item_view.h"

#include "base/mac/scoped_nsobject.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

class StatusItemViewTest : public ui::CocoaTest {
 public:
  StatusItemViewTest()
      : view_([[MCStatusItemView alloc] init]) {
  }

  virtual void SetUp() OVERRIDE {
    ui::CocoaTest::SetUp();
    [[test_window() contentView] addSubview:view_];
  }

  virtual void TearDown() OVERRIDE {
    [view_ removeItem];
    ui::CocoaTest::TearDown();
  }

 protected:
  base::scoped_nsobject<MCStatusItemView> view_;
};

// These tests are like TEST_VIEW() but set some of the properties.

TEST_F(StatusItemViewTest, Callback) {
  __block BOOL got_callback = NO;
  [view_ setCallback:^{
      got_callback = YES;
  }];
  [view_ mouseDown:nil];
  EXPECT_TRUE(got_callback);
  [view_ mouseUp:nil];

  got_callback = NO;
  [view_ rightMouseDown:nil];
  EXPECT_TRUE(got_callback);
  [view_ rightMouseUp:nil];

  got_callback = NO;
  [view_ otherMouseDown:nil];
  EXPECT_TRUE(got_callback);
  [view_ otherMouseUp:nil];
}

TEST_F(StatusItemViewTest, UnreadCount) {
  CGFloat initial_width = NSWidth([view_ frame]);

  CGFloat width = initial_width;
  [view_ setUnreadCount:2];
  [view_ display];
  EXPECT_GT(NSWidth([view_ frame]), width);
  width = NSWidth([view_ frame]);

  [view_ setUnreadCount:10];
  [view_ display];
  EXPECT_GT(NSWidth([view_ frame]), width);
  width = NSWidth([view_ frame]);

  CGFloat max_width = width;

  [view_ setUnreadCount:0];
  [view_ display];
  EXPECT_LT(NSWidth([view_ frame]), width);
  width = NSWidth([view_ frame]);
  EXPECT_CGFLOAT_EQ(width, initial_width);

  [view_ setUnreadCount:1000];
  [view_ display];
  EXPECT_GT(NSWidth([view_ frame]), width);
  width = NSWidth([view_ frame]);
  EXPECT_CGFLOAT_EQ(width, max_width);
}

TEST_F(StatusItemViewTest, Highlight) {
  [view_ setHighlight:YES];
  [view_ display];
  [view_ setHighlight:NO];
  [view_ display];
}

TEST_F(StatusItemViewTest, HighlightAndMouseEvent) {
  [view_ mouseDown:nil];
  [view_ display];
  [view_ setHighlight:YES];
  [view_ display];
  [view_ mouseUp:nil];
  [view_ display];
  [view_ setHighlight:NO];
  [view_ display];
}
