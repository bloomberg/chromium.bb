// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FULLSCREEN_FULLSCREEN_TOOLBAR_CONTROLLER_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_FULLSCREEN_FULLSCREEN_TOOLBAR_CONTROLLER_COCOA_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"

@class BrowserWindowController;

// This struct contains the calculated values of the fullscreen toolbar layout.
struct FullscreenToolbarLayout {
  // The toolbar style.
  FullscreenToolbarStyle toolbarStyle;

  // The fraction of the toolbar that is shown in the screen.
  CGFloat toolbarFraction;

  // The amount the menuber should be offset from the top of the screen.
  CGFloat menubarOffset;
};

// Provides a controller to the fullscreen toolbar for a single Cocoa based
// browser window.
@interface FullscreenToolbarControllerCocoa
    : FullscreenToolbarController<FullscreenToolbarContextDelegate> {
 @private
  BrowserWindowController* browserController_;  // weak
}

// Designated initializer.
- (id)initWithBrowserController:(BrowserWindowController*)controller;

// Computes and return the layout for the fullscreen toolbar.
- (FullscreenToolbarLayout)computeLayout;

// Updates |toolbarStyle_|.
- (void)updateToolbarStyle:(BOOL)isExitingTabFullscreen;

// Updates the toolbar style. If the style has changed, then the toolbar will
// relayout.
- (void)layoutToolbarStyleIsExitingTabFullscreen:(BOOL)isExitingTabFullscreen;

// Updates the toolbar by updating the layout.
- (void)layoutToolbar;

@end

#endif  //
CHROME_BROWSER_UI_COCOA_FULLSCREEN_FULLSCREEN_TOOLBAR_CONTROLLER_COCOA_H_
