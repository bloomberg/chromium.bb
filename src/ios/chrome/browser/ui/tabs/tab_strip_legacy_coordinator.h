// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_LEGACY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_LEGACY_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#import "ios/chrome/browser/ui/tabs/requirements/tab_strip_highlighting.h"

@protocol PopupMenuLongPressDelegate;
@protocol TabStripPresentation;

// A legacy coordinator that presents the public interface for the tablet tab
// strip feature.
@interface TabStripLegacyCoordinator : ChromeCoordinator<TabStripHighlighting>

// Delegate for the long press gesture recognizer triggering popup menu.
@property(nonatomic, weak) id<PopupMenuLongPressDelegate> longPressDelegate;

// Provides methods for presenting the tab strip and checking the visibility
// of the tab strip in the containing object.
@property(nonatomic, assign) id<TabStripPresentation> presentationProvider;

// The duration to wait before starting tab strip animations. Used to
// synchronize animations.
@property(nonatomic, assign) NSTimeInterval animationWaitDuration;

// Hides or shows the TabStrip.
- (void)hideTabStrip:(BOOL)hidden;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_LEGACY_COORDINATOR_H_
