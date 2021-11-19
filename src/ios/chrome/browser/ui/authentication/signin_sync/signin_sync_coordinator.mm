// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/first_run/first_run_metrics.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/enterprise/user_policy_signout/user_policy_signout_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_mediator.h"
#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_view_controller_delegate.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SigninSyncCoordinator () <IdentityChooserCoordinatorDelegate,
                                     PolicyWatcherBrowserAgentObserving,
                                     SigninSyncViewControllerDelegate,
                                     UserPolicySignoutCoordinatorDelegate> {
  // Observer for the sign-out policy changes.
  std::unique_ptr<PolicyWatcherBrowserAgentObserverBridge>
      _policyWatcherObserverBridge;
}

// First run screen delegate.
@property(nonatomic, weak) id<FirstRunScreenDelegate> delegate;
// The view controller.
@property(nonatomic, strong) SigninSyncViewController* viewController;
// The mediator.
@property(nonatomic, strong) SigninSyncMediator* mediator;
// Coordinator handling choosing the account to sign in with.
@property(nonatomic, strong)
    IdentityChooserCoordinator* identityChooserCoordinator;
// Coordinator handling adding a user account.
@property(nonatomic, strong) SigninCoordinator* addAccountSigninCoordinator;
// Whether the user attempted to sign in (the attempt can be successful, failed
// or canceled).
@property(nonatomic, assign) first_run::SignInAttemptStatus attemptStatus;
// Whether there was existing accounts when the screen was presented.
@property(nonatomic, assign) BOOL hadIdentitiesAtStartup;
// The coordinator that manages the prompt for when the user is signed out due
// to policy.
@property(nonatomic, strong)
    UserPolicySignoutCoordinator* policySignoutPromptCoordinator;
// Account manager service to retrieve Chrome identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;
// YES if this coordinator is currently used in First Run.
@property(nonatomic, readonly) BOOL firstRun;

@end

@implementation SigninSyncCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
    _baseNavigationController = navigationController;
    _delegate = delegate;
    _policyWatcherObserverBridge =
        std::make_unique<PolicyWatcherBrowserAgentObserverBridge>(self);

    // Determine if the sign-in screen is used in First Run.
    SceneState* sceneState =
        SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
    AppState* appState = sceneState.appState;
    _firstRun = appState.initStage == InitStageFirstRun;
  }
  return self;
}

- (void)start {
  if (!signin::IsSigninAllowedByPolicy()) {
    self.attemptStatus = first_run::SignInAttemptStatus::SKIPPED_BY_POLICY;
    [self finishPresentingAndSkipRemainingScreens:NO];
    return;
  }

  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  if (authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    // Don't show sign in screen if there is already an account signed in (for
    // example going through the FRE then killing the app and restarting the
    // FRE). Don't record any metric as the user didn't take any action.
    [self.delegate willFinishPresenting];
    return;
  }

  PolicyWatcherBrowserAgent::FromBrowser(self.browser)
      ->AddObserver(_policyWatcherObserverBridge.get());

  self.viewController = [[SigninSyncViewController alloc] init];
  self.viewController.delegate = self;
  self.viewController.enterpriseSignInRestrictions =
      GetEnterpriseSignInRestrictions();

  self.accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  self.mediator = [[SigninSyncMediator alloc]
      initWithAccountManagerService:self.accountManagerService
              authenticationService:authenticationService];

  self.mediator.selectedIdentity =
      self.accountManagerService->GetDefaultIdentity();
  self.hadIdentitiesAtStartup = self.accountManagerService->HasIdentities();

  self.mediator.consumer = self.viewController;
  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ self.viewController ]
                                           animated:animated];
  self.viewController.modalInPresentation = YES;

  if (self.firstRun) {
    base::UmaHistogramEnumeration("FirstRun.Stage",
                                  first_run::kSignInScreenStart);
  }
}

- (void)stop {
  PolicyWatcherBrowserAgent::FromBrowser(self.browser)
      ->RemoveObserver(_policyWatcherObserverBridge.get());

  self.delegate = nil;
  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  [self.identityChooserCoordinator stop];
  self.identityChooserCoordinator = nil;
  // If advancedSettingsSigninCoordinator wasn't dismissed yet (which can
  // happen when closing the scene), try to call -interruptWithAction: to
  // properly cleanup the coordinator.
  SigninCoordinator* signinCoordiantor = self.addAccountSigninCoordinator;
  [self.addAccountSigninCoordinator
      interruptWithAction:SigninCoordinatorInterruptActionNoDismiss
               completion:^() {
                 [signinCoordiantor stop];
               }];
  self.addAccountSigninCoordinator = nil;
  [self.policySignoutPromptCoordinator stop];
  self.policySignoutPromptCoordinator = nil;
}

#pragma mark - SigninSyncViewControllerDelegate

- (void)showAccountPickerFromPoint:(CGPoint)point {
  self.identityChooserCoordinator = [[IdentityChooserCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.identityChooserCoordinator.delegate = self;
  self.identityChooserCoordinator.origin = point;
  [self.identityChooserCoordinator start];
  self.identityChooserCoordinator.selectedIdentity =
      self.mediator.selectedIdentity;
}

- (void)didTapPrimaryActionButton {
  if (self.mediator.selectedIdentity) {
    [self startSignIn];
  } else {
    [self triggerAddAccount];
  }
}

- (void)didTapSecondaryActionButton {
  [self finishPresentingAndSkipRemainingScreens:NO];
  if (self.firstRun) {
    base::UmaHistogramEnumeration(
        "FirstRun.Stage", first_run::kSignInScreenCompletionWithoutSignIn);
  }
}

#pragma mark - IdentityChooserCoordinatorDelegate

- (void)identityChooserCoordinatorDidClose:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  [self.identityChooserCoordinator stop];
  self.identityChooserCoordinator = nil;
}

- (void)identityChooserCoordinatorDidTapOnAddAccount:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  DCHECK(!self.addAccountSigninCoordinator);

  [self triggerAddAccount];
}

- (void)identityChooserCoordinator:(IdentityChooserCoordinator*)coordinator
                 didSelectIdentity:(ChromeIdentity*)identity {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  self.mediator.selectedIdentity = identity;
}

#pragma mark - PolicyWatcherBrowserAgentObserving

- (void)policyWatcherBrowserAgentNotifySignInDisabled:
    (PolicyWatcherBrowserAgent*)policyWatcher {
  if (self.addAccountSigninCoordinator) {
    __weak __typeof(self) weakSelf = self;
    [self.addAccountSigninCoordinator
        interruptWithAction:SigninCoordinatorInterruptActionDismissWithAnimation
                 completion:^{
                   [weakSelf showSignedOutModal];
                 }];
  } else {
    [self showSignedOutModal];
  }
}

#pragma mark - UserPolicySignoutCoordinatorDelegate

- (void)hidePolicySignoutPromptForLearnMore:(BOOL)learnMore {
  [self dismissSignedOutModalAndSkipScreens:learnMore];
}

- (void)userPolicySignoutDidDismiss {
  [self dismissSignedOutModalAndSkipScreens:NO];
}

#pragma mark - Private

// Dismisses the Signed Out modal if it is still present and |skipScreens|.
- (void)dismissSignedOutModalAndSkipScreens:(BOOL)skipScreens {
  [self.policySignoutPromptCoordinator stop];
  self.policySignoutPromptCoordinator = nil;
  [self finishPresentingAndSkipRemainingScreens:skipScreens];
}

// Shows the modal letting the user know that they have been signed out.
- (void)showSignedOutModal {
  self.attemptStatus = first_run::SignInAttemptStatus::SKIPPED_BY_POLICY;
  self.policySignoutPromptCoordinator = [[UserPolicySignoutCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.policySignoutPromptCoordinator.delegate = self;
  [self.policySignoutPromptCoordinator start];
}

// Completes the presentation of the screen, recording the metrics and notifying
// the delegate to skip the rest of the FRE if |skipRemainingScreens| is YES, or
// to continue the FRE.
- (void)finishPresentingAndSkipRemainingScreens:(BOOL)skipRemainingScreens {
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  if (self.firstRun) {
    RecordFirstRunSignInMetrics(identityManager, self.attemptStatus,
                                self.hadIdentitiesAtStartup);
  }

  if (skipRemainingScreens) {
    [self.delegate skipAll];
  } else {
    [self.delegate willFinishPresenting];
  }
}

// Starts the coordinator to present the Add Account module.
- (void)triggerAddAccount {
  self.attemptStatus = first_run::SignInAttemptStatus::ATTEMPTED;

  self.addAccountSigninCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.viewController
                                          browser:self.browser
                                      accessPoint:signin_metrics::AccessPoint::
                                                      ACCESS_POINT_START_PAGE];

  __weak __typeof(self) weakSelf = self;
  self.addAccountSigninCoordinator.signinCompletion =
      ^(SigninCoordinatorResult signinResult,
        SigninCompletionInfo* signinCompletionInfo) {
        [weakSelf addAccountSigninCompleteWithResult:signinResult
                                      completionInfo:signinCompletionInfo];
      };
  [self.addAccountSigninCoordinator start];
}

// Callback handling the completion of the AddAccount action.
- (void)addAccountSigninCompleteWithResult:(SigninCoordinatorResult)signinResult
                            completionInfo:
                                (SigninCompletionInfo*)signinCompletionInfo {
  [self.addAccountSigninCoordinator stop];
  self.addAccountSigninCoordinator = nil;
  if (signinResult == SigninCoordinatorResultSuccess &&
      self.accountManagerService->IsValidIdentity(
          signinCompletionInfo.identity)) {
    self.mediator.selectedIdentity = signinCompletionInfo.identity;
    self.mediator.addedAccount = YES;
  }
}

// Starts the sign in process.
- (void)startSignIn {
  DCHECK(self.mediator.selectedIdentity);

  self.attemptStatus = first_run::SignInAttemptStatus::ATTEMPTED;

  DCHECK(self.mediator.selectedIdentity);
  AuthenticationFlow* authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowser:self.browser
                                         identity:self.mediator.selectedIdentity
                                  shouldClearData:SHOULD_CLEAR_DATA_USER_CHOICE
                                 postSignInAction:POST_SIGNIN_ACTION_NONE
                         presentingViewController:self.viewController];
  __weak __typeof(self) weakSelf = self;
  [self.mediator
      startSignInWithAuthenticationFlow:authenticationFlow
                             completion:^() {
                               [weakSelf
                                   finishPresentingAndSkipRemainingScreens:NO];
                               if (self.firstRun) {
                                 base::UmaHistogramEnumeration(
                                     "FirstRun.Stage",
                                     first_run::
                                         kSignInScreenCompletionWithSignIn);
                               }
                             }];
}

@end
