// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_MEDIATOR_H_

#import <Foundation/Foundation.h>
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

@class AuthenticationFlow;
class AuthenticationService;
@class ChromeIdentity;
class SyncSetupService;

namespace consent_auditor {
class ConsentAuditor;
}

namespace signin {
class IdentityManager;
}

namespace unified_consent {
class UnifiedConsentService;
}

// Delegate that handles interactions with unified consent screen.
@protocol UserSigninMediatorDelegate

// Returns the state of the |settingsLinkWasTapped| parameter in
// UnifiedConsentCoordinator.
- (BOOL)userSigninMediatorGetSettingsLinkWasTapped;

// Gets the consent confirmation ID from UnifiedConsentCoordinator.
- (int)userSigninMediatorGetConsentConfirmationId;

// Get the consent string IDs from UnifiedConsentCoordinator.
- (const std::vector<int>&)userSigninMediatorGetConsentStringIds;

// Updates sign-in state for the UserSigninCoordinator following sign-in
// finishing its workflow.
- (void)userSigninMediatorSigninFinishedWithResult:
    (SigninCoordinatorResult)signinResult;

// Called when the user fails.
- (void)userSigninMediatorSigninFailed;

@end

// Mediator that handles the sign-in operation.
@interface UserSigninMediator : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
                   consentAuditor:
                       (consent_auditor::ConsentAuditor*)consentAuditor
            unifiedConsentService:
                (unified_consent::UnifiedConsentService*)unifiedConsentService
                 syncSetupService:(SyncSetupService*)syncSetupService
    NS_DESIGNATED_INITIALIZER;

// The delegate.
@property(nonatomic, weak) id<UserSigninMediatorDelegate> delegate;

// Whether the authentication operation is in progress.
@property(nonatomic, assign, readonly) BOOL isAuthenticationInProgress;

// Enters the authentication state following identity selection. If there is an
// error transitions to the identity selection state, otherwise enters the final
// authentication completed state.
- (void)authenticateWithIdentity:(ChromeIdentity*)identity
              authenticationFlow:(AuthenticationFlow*)authenticationFlow;

// Reverts the sign-in operation.
- (void)cancelSignin;

// Cancels and dismisses the authentication flow if sign-in is in progress.
- (void)cancelAndDismissAuthenticationFlow;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_MEDIATOR_H_
