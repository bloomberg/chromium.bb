// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

#import "ios/chrome/browser/infobars/infobar_container_state_delegate.h"

namespace infobars {
class InfoBarManager;
}

@protocol InfobarPositioner;

// Coordinator that owns and manages an InfoBarContainer.
@interface InfobarCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
    NS_UNAVAILABLE;

// The InfoBarContainer View.
- (UIView*)view;

// Displays an Infobar it was previously hidden.
- (void)restoreInfobars;

// Hides an Infobar if being shown.
- (void)suspendInfobars;

// Sets the InfobarContainer manager to |infobarManager|.
- (void)setInfobarManager:(infobars::InfoBarManager*)infobarManager;

// Updates the InfobarContainer according to the positioner information.
- (void)updateInfobarContainer;

// The delegate used to position the InfoBarContainer in the view.
@property(nonatomic, weak) id<InfobarPositioner> positioner;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_COORDINATOR_H_
