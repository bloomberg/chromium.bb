// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_COCOA_APP_LIST_PAGER_VIEW_H_
#define UI_APP_LIST_COCOA_APP_LIST_PAGER_VIEW_H_

#import <Cocoa/Cocoa.h>

#import "ui/base/cocoa/tracking_area.h"

@class AppListPagerView;

// Delegate to obtain the visible portion of a page and respond to clicks.
@protocol AppListPagerDelegate<NSObject>

// Returns the portion of a page that is visible, in the range (-1.0, 1.0].
// Positive indicates the left side is visible, negative indicates the right.
- (CGFloat)visiblePortionOfPage:(int)page;

// Invoked when the pager is clicked.
- (void)onPagerClicked:(AppListPagerView*)sender;

@end

// AppListPagerView draws a button strip with buttons representing pages, and a
// highlight that mirrors the visible portion of the page.
@interface AppListPagerView : NSSegmentedControl {
 @private
  // Used to auto-select a segment on hover.
  ui::ScopedCrTrackingArea trackingArea_;

  // The segment currently highlighted with a mouse hover, or -1 for none.
  NSInteger hoveredSegment_;
}

// Returns -1 if |locationInWindow| is not over a segment. Otherwise returns the
// segment index and highlights it.
- (NSInteger)findAndHighlightSegmentAtLocation:(NSPoint)locationInWindow;

@end

#endif  // UI_APP_LIST_COCOA_APP_LIST_PAGER_VIEW_H_
