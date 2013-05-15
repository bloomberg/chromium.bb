// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/status_item_view.h"

#include "base/memory/scoped_nsobject.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

class StatusItemViewTest : public ui::CocoaTest {
 public:
  StatusItemViewTest()
      : status_item_([[NSStatusBar systemStatusBar] statusItemWithLength:
                         NSVariableStatusItemLength]),
        view_([[MCStatusItemView alloc] initWithStatusItem:status_item_]) {
  }

  virtual void SetUp() OVERRIDE {
    // CocoaTest keeps a list of windows that were open at the time of its
    // construction, and it will attempt to close all new ones. Using
    // NSStatusBar creates a non-closeable window. Call Init() again after
    // creating the NSStatusItem to exclude the status bar window from the
    // list of windows to attempt to close.
    ui::CocoaTest::Init();
    ui::CocoaTest::SetUp();
    [[test_window() contentView] addSubview:view_];
  }

 protected:
  NSStatusItem* status_item_;  // Weak, owned by |view_|.
  scoped_nsobject<MCStatusItemView> view_;
};

// These tests are like TEST_VIEW() but set some of the properties.

TEST_F(StatusItemViewTest, Callback) {
  __block BOOL got_callback = NO;
  [view_ setCallback:^{
      got_callback = YES;
  }];
  [view_ mouseDown:nil];
  EXPECT_TRUE(got_callback);
}

TEST_F(StatusItemViewTest, UnreadCount) {
  [view_ setUnreadCount:2];
  [view_ display];
  [view_ setUnreadCount:10];
  [view_ display];
  [view_ setUnreadCount:0];
  [view_ display];
  [view_ setUnreadCount:1000];
  [view_ display];
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
