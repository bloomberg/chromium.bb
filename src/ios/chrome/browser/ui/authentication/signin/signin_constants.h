// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_CONSTANTS_H_

#import <Foundation/Foundation.h>

@class ChromeIdentity;

// Sign-in result returned Sign-in result.
typedef NS_ENUM(NSUInteger, SigninCoordinatorResult) {
  // Sign-in has been canceled by the user or by another reason.
  SigninCoordinatorResultCanceledByUser,
  // Sign-in has been done, but the user didn’t accept nor refuse to sync.
  SigninCoordinatorResultInterrupted,
  // Sign-in has been done, the user has been explicitly accepted or refused
  // sync.
  SigninCoordinatorResultSuccess,
};

// User's signed-in state as defined by AuthenticationService.
typedef NS_ENUM(NSUInteger, IdentitySigninState) {
  IdentitySigninStateSignedOut,
  IdentitySigninStateSignedInWithSyncDisabled,
  IdentitySigninStateSignedInWithSyncEnabled,
};

// Action to do when the sign-in dialog needs to be interrupted.
typedef NS_ENUM(NSUInteger, SigninCoordinatorInterruptAction) {
  // Stops the sign-in coordinator without dismissing the view.
  SigninCoordinatorInterruptActionNoDismiss,
  // Stops the sign-in coordinator and dismisses the view without animation.
  SigninCoordinatorInterruptActionDismissWithoutAnimation,
  // Stops the sign-in coordinator and dismisses the view with animation.
  SigninCoordinatorInterruptActionDismissWithAnimation,
};

// Name of notification sent when the user has attempted a sign-in.
extern NSString* const kUserSigninAttemptedNotification;
// Name of accessibility identifier for the skip sign-in button.
extern NSString* const kSkipSigninAccessibilityIdentifier;
// Name of accessibility identifier for the add account button in the sign-in
// flow.
extern NSString* const kAddAccountAccessibilityIdentifier;
// Name of accessibility identifier for the confirmation "Yes I'm In" sign-in
// button.
extern NSString* const kConfirmationAccessibilityIdentifier;
// Name of accessibility identifier for the more button in the sign-in flow.
extern NSString* const kMoreAccessibilityIdentifier;
// Name of accessibility identifier for the web sign-in consistency sheet.
extern NSString* const kWebSigninAccessibilityIdentifier;
// Name of accessiblity identifier for "Continue As..." button that signs in
// the primary account user for the web sign-in consistency sheet.
extern NSString* const kWebSigninContinueAsButtonAccessibilityIdentifier;
// Name of accessibility identifier for "Skip" button in the web sign-in
// consistency sheet.
extern NSString* const kWebSigninSkipButtonAccessibilityIdentifier;

// Action that is required to do to complete the sign-in, or instead of sign-in.
// This action is in charge of the SigninCoordinator's owner.
typedef NS_ENUM(NSUInteger, SigninCompletionAction) {
  // No action needed.
  SigninCompletionActionNone,
  // The user tapped the manager, learn more, link and sign-in was cancelled.
  SigninCompletionActionShowManagedLearnMore,
};

// Intent for TrustedVaultReauthenticationCoordinator to display either
// the reauthentication or degraded recoverability dialog.
typedef NS_ENUM(NSUInteger, SigninTrustedVaultDialogIntent) {
  // Show reauthentication dialog for fetch keys.
  SigninTrustedVaultDialogIntentFetchKeys,
  // Show reauthentication degraded recoverability dialog (to enroll additional
  // recovery factors).
  SigninTrustedVaultDialogIntentDegradedRecoverability,
};

// Max dismissal count for web sign-in consistency dialog (the dismissal value
// is reset as soon as the user shows sign-in intent).
extern const int kDefaultWebSignInDismissalCount;

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_CONSTANTS_H_
