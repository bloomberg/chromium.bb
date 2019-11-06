// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_ALL_PASSWORD_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_ALL_PASSWORD_COORDINATOR_H_

#import "ios/chrome/browser/ui/autofill/manual_fill/fallback_coordinator.h"

@protocol PasswordListNavigator;

namespace manual_fill {

extern NSString* const kPasswordDoneButtonAccessibilityIdentifier;

}  // namespace manual_fill

// Creates and manages a view controller to present all the passwords to the
// user. The view controller contains a search bar. The presentation is done
// with a table view presentation controller. Any selected password will be sent
// to the current field in the active web state.
@interface ManualFillAllPasswordCoordinator : FallbackCoordinator

// Creates a coordinator with the passed args.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                  browserState:(ios::ChromeBrowserState*)browserState
              injectionHandler:(ManualFillInjectionHandler*)injectionHandler
                     navigator:(id<PasswordListNavigator>)navigator
    NS_DESIGNATED_INITIALIZER;

// Unavailable, use
// -initWithBaseViewController:browserState:webStateList:injectionHandler:.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                  browserState:(ios::ChromeBrowserState*)browserState
              injectionHandler:(ManualFillInjectionHandler*)injectionHandler
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_ALL_PASSWORD_COORDINATOR_H_
