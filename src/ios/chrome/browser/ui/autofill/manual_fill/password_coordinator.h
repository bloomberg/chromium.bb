// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_COORDINATOR_H_

#import "ios/chrome/browser/ui/autofill/manual_fill/fallback_coordinator.h"

class WebStateList;

namespace manual_fill {

extern NSString* const PasswordDoneButtonAccessibilityIdentifier;

}  // namespace manual_fill

// Delegate for the coordinator actions.
@protocol PasswordCoordinatorDelegate<FallbackCoordinatorDelegate>

// Opens the passwords settings.
- (void)openPasswordSettings;

@end

// Creates and manages a view controller to present passwords to the user.
// Any selected password will be sent to the current field in the active web
// state.
@interface ManualFillPasswordCoordinator : FallbackCoordinator

// The delegate for this coordinator. Delegate class extends
// FallbackCoordinatorDelegate, and replaces super class delegate.
@property(nonatomic, weak) id<PasswordCoordinatorDelegate> delegate;

// Creates a coordinator that uses a |viewController|, |browserState|,
// |webStateList| and an |injectionHandler|.
- (instancetype)
initWithBaseViewController:(UIViewController*)viewController
              browserState:(ios::ChromeBrowserState*)browserState
              webStateList:(WebStateList*)webStateList
          injectionHandler:(ManualFillInjectionHandler*)injectionHandler
    NS_DESIGNATED_INITIALIZER;

// Unavailable, use
// -initWithBaseViewController:browserState:webStateList:injectionHandler:.
- (instancetype)
initWithBaseViewController:(UIViewController*)viewController
              browserState:(ios::ChromeBrowserState*)browserState
          injectionHandler:(ManualFillInjectionHandler*)injectionHandler
    NS_UNAVAILABLE;

// Presents the password view controller as a popover from the passed button.
- (void)presentFromButton:(UIButton*)button;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_COORDINATOR_H_
