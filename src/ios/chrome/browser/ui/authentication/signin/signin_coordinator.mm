// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"

#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/trusted_vault_reauthentication/trusted_vault_reauthentication_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/first_run_signin_logger.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/upgrade_signin_logger.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@implementation SigninCoordinator

+ (instancetype)
    userSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                        browser:(Browser*)browser
                                       identity:(ChromeIdentity*)identity
                                    accessPoint:(AccessPoint)accessPoint
                                    promoAction:(PromoAction)promoAction {
  UserSigninLogger* logger =
      [[UserSigninLogger alloc] initWithAccessPoint:accessPoint
                                        promoAction:promoAction];
  return [[UserSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                        identity:identity
                    signinIntent:UserSigninIntentSignin
                          logger:logger];
}

+ (instancetype)firstRunCoordinatorWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                                        browser:
                                                            (Browser*)browser {
  UserSigninLogger* logger = [[FirstRunSigninLogger alloc]
      initWithAccessPoint:AccessPoint::ACCESS_POINT_START_PAGE
              promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO];
  return [[UserSigninCoordinator alloc]
      initWithBaseNavigationController:navigationController
                               browser:browser
                          signinIntent:UserSigninIntentFirstRun
                                logger:logger];
}

+ (instancetype)
    upgradeSigninPromoCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                browser:(Browser*)browser {
  UserSigninLogger* logger = [[UpgradeSigninLogger alloc]
      initWithAccessPoint:AccessPoint::ACCESS_POINT_SIGNIN_PROMO
              promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO];
  return [[UserSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                        identity:nil
                    signinIntent:UserSigninIntentUpgrade
                          logger:logger];
}

+ (instancetype)
    advancedSettingsSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                    browser:(Browser*)browser {
  return [[AdvancedSettingsSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser];
}

+ (instancetype)addAccountCoordinatorWithBaseViewController:
                    (UIViewController*)viewController
                                                    browser:(Browser*)browser
                                                accessPoint:
                                                    (AccessPoint)accessPoint {
  return [[AddAccountSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                     accessPoint:accessPoint
                     promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO
                    signinIntent:AddAccountSigninIntentAddSecondaryAccount];
}

+ (instancetype)
    reAuthenticationCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                              browser:(Browser*)browser
                                          accessPoint:(AccessPoint)accessPoint
                                          promoAction:(PromoAction)promoAction {
  return [[AddAccountSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                     accessPoint:accessPoint
                     promoAction:promoAction
                    signinIntent:AddAccountSigninIntentReauthPrimaryAccount];
}

+ (instancetype)
    trustedVaultReAuthenticationCoordiantorWithBaseViewController:
        (UIViewController*)viewController
                                                          browser:
                                                              (Browser*)browser
                                                 retrievalTrigger:
                                                     (syncer::
                                                          KeyRetrievalTriggerForUMA)
                                                         retrievalTrigger {
  return [[TrustedVaultReauthenticationCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                retrievalTrigger:retrievalTrigger];
}

- (void)dealloc {
  // -[SigninCoordinator runCompletionCallbackWithSigninResult:identity:
  // showAdvancedSettingsSignin:] has to be called by the subclass before
  // the coordinator is deallocated.
  DCHECK(!self.signinCompletion);
}

- (void)interruptWithAction:(SigninCoordinatorInterruptAction)action
                 completion:(ProceduralBlock)completion {
  // This method needs to be implemented in the subclass.
  NOTREACHED();
}

#pragma mark - SigninCoordinator

- (void)start {
  // |signinCompletion| needs to be set by the owner to know when the sign-in
  // is finished.
  DCHECK(self.signinCompletion);
}

- (void)stop {
  // -[SigninCoordinator runCompletionCallbackWithSigninResult:identity:
  // showAdvancedSettingsSignin:] has to be called by the subclass before
  // -[SigninCoordinator stop] is called.
  DCHECK(!self.signinCompletion);
}

#pragma mark - Properties

- (BOOL)isSettingsViewPresented {
  // TODO(crbug.com/971989): Remove this method.
  return NO;
}

#pragma mark - Private

- (void)runCompletionCallbackWithSigninResult:
            (SigninCoordinatorResult)signinResult
                                     identity:(ChromeIdentity*)identity
                   showAdvancedSettingsSignin:(BOOL)showAdvancedSettingsSignin {
  SigninCompletionAction signinCompletionAction =
      showAdvancedSettingsSignin
          ? SigninCompletionActionShowAdvancedSettingsSignin
          : SigninCompletionActionNone;
  SigninCompletionInfo* signinCompletionInfo =
      [[SigninCompletionInfo alloc] initWithIdentity:identity
                              signinCompletionAction:signinCompletionAction];
  // If |self.signinCompletion| is nil, this method has been probably called
  // twice.
  DCHECK(self.signinCompletion);
  SigninCoordinatorCompletionCallback signinCompletion = self.signinCompletion;
  // The owner should call the stop method, during the callback.
  // |self.signinCompletion| needs to be set to nil before calling it.
  self.signinCompletion = nil;
  signinCompletion(signinResult, signinCompletionInfo);
}

@end
