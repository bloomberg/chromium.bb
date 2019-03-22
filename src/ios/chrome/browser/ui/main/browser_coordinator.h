// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_BROWSER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_MAIN_BROWSER_COORDINATOR_H_

#include "base/ios/block_types.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@protocol ApplicationCommands;
@class BrowserViewController;
@class TabModel;

// Coordinator for BrowserViewController.
@interface BrowserCoordinator : ChromeCoordinator

// The main view controller.
@property(nonatomic, strong, readonly) BrowserViewController* viewController;

// Command handler for ApplicationCommands.
@property(nonatomic, weak) id<ApplicationCommands> applicationCommandHandler;

// The model.
@property(nonatomic, weak) TabModel* tabModel;

// Clears any presented state on BVC.
- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_BROWSER_COORDINATOR_H_
