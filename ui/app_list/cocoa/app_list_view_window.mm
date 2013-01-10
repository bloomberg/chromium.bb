// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/app_list_view_window.h"

@implementation AppListViewWindow;

- (id)initAsBubble {
  const int kWindowWidth = 200;
  const int kWindowHeight = 200;
  NSRect frame = NSMakeRect(0, 0, kWindowWidth, kWindowHeight);
  if ((self = [super initWithContentRect:frame
                               styleMask:NSBorderlessWindowMask
                                 backing:NSBackingStoreBuffered
                                   defer:NO])) {
    [self setReleasedWhenClosed:NO];
    [self setBackgroundColor:[NSColor grayColor]];
  }

  return self;
}

@end
