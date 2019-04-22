// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_UI_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_UI_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"

@class ChromeIdentity;

// Methods used for the EarlGrey tests, related to UI.
@interface SigninEarlGreyUI : NSObject

// Adds the identity (if not already added), and perform a sign-in.
+ (void)signinWithIdentity:(ChromeIdentity*)identity;

// Selects an identity when the identity chooser dialog is presented. The dialog
// is confirmed, but it doesn't validated the user consent page.
+ (void)selectIdentityWithEmail:(NSString*)userEmail;

// Confirms the sign in confirmation page, scrolls first to make the OK button
// visible on short devices (e.g. iPhone 5s).
+ (void)confirmSigninConfirmationDialog;

// Checks that the sign-in promo view (with a close button) is visible using the
// right mode.
+ (void)checkSigninPromoVisibleWithMode:(SigninPromoViewMode)mode;

// Checks that the sign-in promo view is visible using the right mode. If
// |closeButton| is set to YES, the close button in the sign-in promo has to be
// visible.
+ (void)checkSigninPromoVisibleWithMode:(SigninPromoViewMode)mode
                            closeButton:(BOOL)closeButton;

// Checks that the sign-in promo view is not visible.
+ (void)checkSigninPromoNotVisible;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_UI_H_
