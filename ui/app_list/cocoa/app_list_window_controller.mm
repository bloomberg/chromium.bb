// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/app_list_window_controller.h"

#include "ui/app_list/app_list_view_delegate.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"

@interface AppListWindow : NSWindow;
@end

@implementation AppListWindow

// If we initialize a window with NSBorderlessWindowMask, it will not accept key
// events (among other things) unless canBecomeKeyWindow is overridden.
- (BOOL)canBecomeKeyWindow {
  return YES;
}

@end

@implementation AppListWindowController;

- (id)initWithGridController:(AppsGridController*)gridController {
  scoped_nsobject<NSWindow> controlledWindow(
      [[AppListWindow alloc] initWithContentRect:[[gridController view] bounds]
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
    [[self window] setDelegate:self];
    [[self window] makeFirstResponder:[appsGridController_ collectionView]];
  }
  return self;
}

- (AppsGridController*)appsGridController {
  return appsGridController_;
}

- (void)doCommandBySelector:(SEL)command {
  if (command == @selector(cancel:)) {
    if ([appsGridController_ delegate])
      [appsGridController_ delegate]->Dismiss();
  } else if (command == @selector(insertNewline:) ||
             command == @selector(insertLineBreak:)) {
    [appsGridController_ activateSelection];
  }
}

@end
