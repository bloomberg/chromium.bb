// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

namespace ios {
class ChromeBrowserState;
}  // namespace ios
@protocol ConsentBumpCoordinatorDelegate;

// Coordinator handling the consent bump.
@interface ConsentBumpCoordinator : ChromeCoordinator

// ViewController associated with this coordinator.
@property(nonatomic, strong, readonly) UIViewController* viewController;

// Delegate for this coordinator.
@property(nonatomic, weak) id<ConsentBumpCoordinatorDelegate> delegate;

// Returns YES if the consent bump should be presented to the user.
+ (BOOL)shouldShowConsentBumpWithBrowserState:
    (ios::ChromeBrowserState*)browserState;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_COORDINATOR_H_
