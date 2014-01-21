// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/app_list_window_controller.h"

#include "ui/app_list/app_list_view_delegate.h"
#import "ui/app_list/cocoa/app_list_view_controller.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"
#import "ui/app_list/cocoa/apps_search_box_controller.h"
#include "ui/base/cocoa/window_size_constants.h"

@interface AppListWindow : NSWindow;
@end

@implementation AppListWindow

// If we initialize a window with NSBorderlessWindowMask, it will not accept key
// events (among other things) unless canBecomeKeyWindow is overridden.
- (BOOL)canBecomeKeyWindow {
  return YES;
}

- (BOOL)canBecomeMainWindow {
  return YES;
}

// On Mavericks with the "Displays have separate Spaces" option, OSX has stopped
// switching out of the fullscreen space when activating a window in the non-
// active application, other than by clicking its Dock icon. Since the app
// launcher Dock icon is not Chrome, this can leave a user in fullscreen with
// the app launcher window obscured. Overriding this private method allows the
// app launcher to appear on top of other applications in fullscreen. Then,
// since clicking that window will make Chrome active, subsequent window
// activations will successfully switch the user out of the fullscreen space.
- (BOOL)_allowedInOtherAppsFullScreenSpaceWithCollectionBehavior:
    (NSUInteger)collectionBehavior {
  return YES;
}

@end

@implementation AppListWindowController;

- (id)init {
  base::scoped_nsobject<NSWindow> controlledWindow(
      [[AppListWindow alloc] initWithContentRect:ui::kWindowSizeDeterminedLater
                                       styleMask:NSBorderlessWindowMask
                                         backing:NSBackingStoreBuffered
                                           defer:NO]);
  [controlledWindow setReleasedWhenClosed:NO];
  [controlledWindow setBackgroundColor:[NSColor clearColor]];
  [controlledWindow setOpaque:NO];
  [controlledWindow setHasShadow:YES];
  [controlledWindow setLevel:NSDockWindowLevel];
  [controlledWindow
      setCollectionBehavior:NSWindowCollectionBehaviorMoveToActiveSpace];

  if ((self = [super initWithWindow:controlledWindow])) {
    appListViewController_.reset([[AppListViewController alloc] init]);
    [[self window] setFrame:[[appListViewController_ view] bounds]
                    display:NO];
    [[self window] setContentView:[appListViewController_ view]];
    [[self window] setDelegate:self];
  }
  return self;
}

- (AppListViewController*)appListViewController {
  return appListViewController_;
}

- (void)windowDidResignMain:(NSNotification*)notification {
  if ([appListViewController_ delegate])
    [appListViewController_ delegate]->Dismiss();
}

- (void)windowWillClose:(NSNotification*)notification {
  [[appListViewController_ searchBoxController] clearSearch];
}

@end
