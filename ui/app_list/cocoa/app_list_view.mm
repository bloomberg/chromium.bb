// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/app_list_view.h"

#import "third_party/GTM/AppKit/GTMNSBezierPath+RoundRect.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"

// The roundedness of the corners of the bubble.
const int kBubbleCornerRadius = 3;
// The grayscale value for the window edge stroke.
const float kWindowEdge = 0.7f;

@implementation AppListView;

- (id)initWithViewDelegate:(app_list::AppListViewDelegate*)appListViewDelegate {
  const int kViewWidth = 200;
  const int kViewHeight = 200;
  NSRect frame = NSMakeRect(0, 0, kViewWidth, kViewHeight);
  if ((self = [super initWithFrame:frame])) {
    model_.reset(new app_list::AppListModel());
    delegate_.reset(appListViewDelegate);
  }

  return self;
}

- (void)drawRect:(NSRect)rect {
  NSRect bounds = [self bounds];
  NSBezierPath* border =
      [NSBezierPath gtm_bezierPathWithRoundRect:bounds
                                   cornerRadius:kBubbleCornerRadius];

  [[NSColor grayColor] set];
  [border fill];

  // Inset so the stroke is inside the bounds.
  bounds = NSInsetRect(bounds, 0.5, 0.5);

  // TODO(tapted): Update the window border to use assets rather than vectors.
  border =
      [NSBezierPath gtm_bezierPathWithRoundRect:bounds
                                   cornerRadius:kBubbleCornerRadius];

  [[NSColor colorWithDeviceWhite:kWindowEdge alpha:1.0f] set];
  [border stroke];
}

@end
