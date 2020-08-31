// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADVANCED_SETTINGS_SIGNIN_ADVANCED_SETTINGS_SIGNIN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADVANCED_SETTINGS_SIGNIN_ADVANCED_SETTINGS_SIGNIN_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

@class AdvancedSettingsSigninMediator;
@class AdvancedSettingsSigninNavigationController;
class AuthenticationService;
class PrefService;
class SyncSetupService;
namespace syncer {
class SyncService;
}

// Mediator for the advanced settings sign-in flow.
@interface AdvancedSettingsSigninMediator : NSObject

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithSyncSetupService:(SyncSetupService*)syncSetupService
                   authenticationService:
                       (AuthenticationService*)authenticationService
                             syncService:(syncer::SyncService*)syncService
                             prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

// Saves the user sync preferences.
- (void)saveUserPreferenceForSigninResult:(SigninCoordinatorResult)signinResult;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADVANCED_SETTINGS_SIGNIN_ADVANCED_SETTINGS_SIGNIN_MEDIATOR_H_
