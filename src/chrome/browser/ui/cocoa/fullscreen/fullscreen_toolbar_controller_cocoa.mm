// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller_cocoa.h"

#import "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_menubar_tracker.h"
#import "chrome/browser/ui/cocoa/fullscreen/immersive_fullscreen_controller.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

@implementation FullscreenToolbarControllerCocoa

- (id)initWithBrowserController:(BrowserWindowController*)controller {
  if ((self = [super initWithDelegate:self]))
    browserController_ = controller;

  return self;
}

- (void)enterFullscreenMode {
  [self updateToolbarStyle:NO];
  [FullscreenToolbarController recordToolbarStyle:[self toolbarStyle]];

  [super enterFullscreenMode];
}

- (void)exitFullscreenMode {
  [super exitFullscreenMode];

  // No more calls back up to the BWC.
  browserController_ = nil;
}

- (FullscreenToolbarLayout)computeLayout {
  FullscreenToolbarLayout layout;
  layout.toolbarStyle = [self toolbarStyle];
  layout.toolbarFraction = [self toolbarFraction];

  // Calculate how much the toolbar is offset downwards to avoid the menu.
  if ([browserController_ isInAppKitFullscreen]) {
    layout.menubarOffset = [[self menubarTracker] menubarFraction];
  } else {
    layout.menubarOffset =
        [[self immersiveFullscreenController] shouldShowMenubar] ? 1 : 0;
  }
  layout.menubarOffset *= -[browserController_ menubarHeight];

  return layout;
}

- (void)updateToolbarStyle:(BOOL)isExitingTabFullscreen {
  if ([browserController_ isFullscreenForTabContentOrExtension] &&
      !isExitingTabFullscreen) {
    [self setToolbarStyle:FullscreenToolbarStyle::TOOLBAR_NONE];
  } else {
    PrefService* prefs = [browserController_ profile]->GetPrefs();
    [self setToolbarStyle:prefs->GetBoolean(prefs::kShowFullscreenToolbar)
                              ? FullscreenToolbarStyle::TOOLBAR_PRESENT
                              : FullscreenToolbarStyle::TOOLBAR_HIDDEN];
  }
}

- (void)layoutToolbarStyleIsExitingTabFullscreen:(BOOL)isExitingTabFullscreen {
  FullscreenToolbarStyle oldStyle = [self toolbarStyle];
  [self updateToolbarStyle:isExitingTabFullscreen];

  FullscreenToolbarStyle curStyle = [self toolbarStyle];
  if (oldStyle != curStyle) {
    [self layoutToolbar];
    [FullscreenToolbarController recordToolbarStyle:curStyle];
  }
}

- (void)layoutToolbar {
  [browserController_ layoutSubviews];
  [super layoutToolbar];
}

- (BOOL)isInAnyFullscreenMode {
  return [browserController_ isInAnyFullscreenMode];
}

- (BOOL)isFullscreenTransitionInProgress {
  return [browserController_ isFullscreenTransitionInProgress];
}

- (NSWindow*)window {
  return [browserController_ window];
}

- (BOOL)isInImmersiveFullscreen {
  return [browserController_ isInImmersiveFullscreen];
}

@end
