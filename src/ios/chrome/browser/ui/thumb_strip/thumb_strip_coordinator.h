// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@class ThumbStripCoordinator;
@class ViewRevealingVerticalPanHandler;

// Coordinator for the thumb strip, which is a 1-row horizontal display of tab
// miniatures above the toolbar.
@interface ThumbStripCoordinator : ChromeCoordinator

// The thumb strip's pan gesture handler.
@property(nonatomic, strong) ViewRevealingVerticalPanHandler* panHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_COORDINATOR_H_
