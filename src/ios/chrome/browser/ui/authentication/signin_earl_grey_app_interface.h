// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

@class FakeChromeIdentity;
@protocol GREYMatcher;

namespace signin {
enum class ConsentLevel;
}

// SigninEarlGreyAppInterface contains the app-side implementation for
// helpers that primarily work via direct model access. These helpers are
// compiled into the app binary and can be called from either app or test code.
@interface SigninEarlGreyAppInterface : NSObject

// Returns a fake identity.
+ (FakeChromeIdentity*)fakeIdentity1;

// Returns a second fake identity.
+ (FakeChromeIdentity*)fakeIdentity2;

// Returns a fake managed identity.
+ (FakeChromeIdentity*)fakeManagedIdentity;

// Adds |fakeIdentity| to the fake identity service.
+ (void)addFakeIdentity:(FakeChromeIdentity*)fakeIdentity;

// Maps |capabilities| to the |fakeIdentity|.
// Must be called after |addFakeIdentity|.
+ (void)setCapabilities:(NSDictionary*)capabilities
            forIdentity:(FakeChromeIdentity*)fakeIdentity;

// Removes |fakeIdentity| from the fake chrome identity service asynchronously
// to simulate identity removal from the device.
+ (void)forgetFakeIdentity:(FakeChromeIdentity*)fakeIdentity;

// Returns the gaia ID of the signed-in account.
// If there is no signed-in account returns an empty string.
+ (NSString*)primaryAccountGaiaID;

// Returns the email of the primary account base on |consentLevel|.
// If there is no signed-in account returns an empty string.
+ (NSString*)primaryAccountEmailWithConsent:(signin::ConsentLevel)consentLevel;

// Checks that no identity is signed in.
+ (BOOL)isSignedOut;

// Checks whether there is a primary identity.
+ (BOOL)hasPrimaryIdentity;

// Signs out the current user.
+ (void)signOut;

// Triggers the reauth dialog. This is done by sending ShowSigninCommand to
// SceneController, without any UI interaction to open the dialog.
+ (void)triggerReauthDialogWithFakeIdentity:(FakeChromeIdentity*)identity;

// Triggers the web sign-in consistency dialog. This is done by calling
// directly the current SceneController.
+ (void)triggerConsistencyPromoSigninDialog;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_EARL_GREY_APP_INTERFACE_H_
