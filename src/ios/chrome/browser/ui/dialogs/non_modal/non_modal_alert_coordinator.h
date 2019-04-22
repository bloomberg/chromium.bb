// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DIALOGS_NON_MODAL_NON_MODAL_ALERT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_DIALOGS_NON_MODAL_NON_MODAL_ALERT_COORDINATOR_H_

#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"

// A coordinator that manages displaying alerts non-modally over the visible
// viewport of the content area.
@interface NonModalAlertCoordinator : AlertCoordinator

// Non-modal alerts must be instantiated with a BrowserState, so the initializer
// without the BrowserState parameter is disabled.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                     title:(NSString*)title
                                   message:(NSString*)message NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_DIALOGS_NON_MODAL_NON_MODAL_ALERT_COORDINATOR_H_
