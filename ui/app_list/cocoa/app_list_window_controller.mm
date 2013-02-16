// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/app_list_window_controller.h"

#import "ui/app_list/cocoa/apps_grid_controller.h"

@implementation AppListWindowController;

- (id)initWithGridController:(AppsGridController*)gridController {
  scoped_nsobject<NSWindow> controlledWindow(
      [[NSWindow alloc] initWithContentRect:[[gridController view] bounds]
                                  styleMask:NSBorderlessWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  [controlledWindow setContentView:[gridController view]];
  [controlledWindow setReleasedWhenClosed:NO];
  [controlledWindow setBackgroundColor:[NSColor clearColor]];
  [controlledWindow setOpaque:NO];
  [controlledWindow setHasShadow:NO];

  if ((self = [super initWithWindow:controlledWindow])) {
    appsGridController_.reset([gridController retain]);
  }
  return self;
}

@end
