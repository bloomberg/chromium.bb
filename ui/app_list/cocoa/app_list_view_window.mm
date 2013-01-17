// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/app_list_view_window.h"

#import "ui/base/cocoa/window_size_constants.h"

@implementation AppListViewWindow;

- (id)initAsBubble {
  if ((self = [super initWithContentRect:ui::kWindowSizeDeterminedLater
                               styleMask:NSBorderlessWindowMask
                                 backing:NSBackingStoreBuffered
                                   defer:NO])) {
    [self setReleasedWhenClosed:NO];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setOpaque:NO];
    [self setHasShadow:NO];
  }

  return self;
}

- (void)setAppListView:(NSView*)view {
  [self setContentSize:[view frame].size];
  [self setContentView:view];
}

@end
