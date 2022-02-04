// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_coordinator.h"

#import "base/metrics/histogram_functions.h"
#include "components/sync/driver/sync_service.h"
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
#import "ios/chrome/browser/signin/constants.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/consent_auditor_factory.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/enterprise/user_policy_signout/user_policy_signout_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_mediator.h"
#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_mediator_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_view_controller_delegate.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#include "ios/chrome/browser/ui/first_run/fre_field_trial.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SigninSyncCoordinator () <IdentityChooserCoordinatorDelegate,
                                     PolicyWatcherBrowserAgentObserving,
                                     SigninSyncMediatorDelegate,
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
// YES if this coordinator is currently used in First Run. When set to NO, it
// is assumed that the coordinator is used outside of First Run (i.e., used
// from Settings).
@property(nonatomic, readonly) BOOL firstRun;
// The consent string ids that were pushed that are related to the text for
// sync.
@property(nonatomic, assign, readonly) NSMutableArray* consentStringIDs;
// Coordinator for showing advanced settings on top of the screen.
@property(nonatomic, strong)
    SigninCoordinator* advancedSettingsSigninCoordinator;
// Browser sign-in state to revert to in case sync is canceled.
@property(nonatomic, assign) IdentitySigninState signinStateOnStart;
// Sign-in identity when the coordiantor starts. This is used as the identity to
// revert to in case sync is canceled.
@property(nonatomic, strong) ChromeIdentity* signinIdentityOnStart;

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
    // Make sure that the coordinator is only used for the FRE which is the
    // only context that is supported at the moment. The coordinator may be
    // used outside of the FRE but this case isn't supported yet.
    DCHECK(_firstRun);
  }
  return self;
}

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();

  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  self.signinIdentityOnStart =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  if (!signin::IsSigninAllowedByPolicy() ||
      IsSyncDisabledByPolicy(browserState)) {
    // Skip the screen if sync is disabled by policy.
    self.attemptStatus = first_run::SignInAttemptStatus::SKIPPED_BY_POLICY;
    [self finishPresentingAndSkipRemainingScreens:NO];
    return;
  }

  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(browserState);
  if (syncSetupService->IsFirstSetupComplete()) {
    // Skip the screen if sync is already enabled. Sync might be already
    // enabled if the coordinator is used outside of the FRE OR when tests are
    // performed with irregular states. We expect sync to be disabled when the
    // FRE is displayed in a regular situation (i.e., first launch after
    // install).
    [self.delegate willFinishPresenting];
    return;
  }

  self.signinStateOnStart =
      signin::GetPrimaryIdentitySigninState(self.browser->GetBrowserState());

  PolicyWatcherBrowserAgent::FromBrowser(self.browser)
      ->AddObserver(_policyWatcherObserverBridge.get());

  self.viewController = [[SigninSyncViewController alloc] init];
  self.viewController.delegate = self;
  self.viewController.enterpriseSignInRestrictions =
      GetEnterpriseSignInRestrictions(browserState);
  self.viewController.identitySwitcherPosition =
      fre_field_trial::GetSigninSyncScreenUIIdentitySwitcherPosition();
  self.viewController.stringsSet =
      fre_field_trial::GetSigninSyncScreenUIStringSet();

  self.accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState);

  self.mediator = [[SigninSyncMediator alloc]
      initWithAuthenticationService:AuthenticationServiceFactory::
                                        GetForBrowserState(browserState)
                    identityManager:IdentityManagerFactory::GetForBrowserState(
                                        browserState)
              accountManagerService:self.accountManagerService
                     consentAuditor:ConsentAuditorFactory::GetForBrowserState(
                                        browserState)
                   syncSetupService:syncSetupService
              unifiedConsentService:UnifiedConsentServiceFactory::
                                        GetForBrowserState(browserState)
                        syncService:SyncServiceFactory::GetForBrowserState(
                                        self.browser->GetBrowserState())];
  self.mediator.delegate = self;
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

  // If |_addAccountSigninCoordinator| or |_advancedSettingsSigninCoordinator|
  // weren't stopped yet (which can happen when closing the scene), try to
  // call -interruptWithAction: to properly tear down the coordinators.
  SigninCoordinator* signinCoordinator = self.addAccountSigninCoordinator;
  [self.addAccountSigninCoordinator
      interruptWithAction:SigninCoordinatorInterruptActionNoDismiss
               completion:^() {
                 [signinCoordinator stop];
               }];
  self.addAccountSigninCoordinator = nil;
  signinCoordinator = self.advancedSettingsSigninCoordinator;
  [self.advancedSettingsSigninCoordinator
      interruptWithAction:SigninCoordinatorInterruptActionNoDismiss
               completion:^() {
                 [signinCoordinator stop];
               }];
  self.advancedSettingsSigninCoordinator = nil;

  [self.policySignoutPromptCoordinator stop];
  self.policySignoutPromptCoordinator = nil;
}

#pragma mark - SigninSyncViewControllerDelegate

- (void)signinSyncViewController:
            (SigninSyncViewController*)signinSyncViewController
      showAccountPickerFromPoint:(CGPoint)point {
  self.identityChooserCoordinator = [[IdentityChooserCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.identityChooserCoordinator.delegate = self;
  self.identityChooserCoordinator.origin = point;
  [self.identityChooserCoordinator start];
  self.identityChooserCoordinator.selectedIdentity =
      self.mediator.selectedIdentity;
}

- (void)signinSyncViewControllerDidTapOnSettings:
    (SigninSyncViewController*)signinSyncViewController {
  DCHECK(self.mediator.selectedIdentity);

  AuthenticationFlow* authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowser:self.browser
                                         identity:self.mediator.selectedIdentity
                                  shouldClearData:SHOULD_CLEAR_DATA_USER_CHOICE
                                 postSignInAction:POST_SIGNIN_ACTION_NONE
                         presentingViewController:self.viewController];
  authenticationFlow.dispatcher = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowsingDataCommands);

  [self.mediator
      prepareAdvancedSettingsWithAuthenticationFlow:authenticationFlow];
}

- (void)signinSyncViewController:
            (SigninSyncViewController*)signinSyncViewController
              addConsentStringID:(const int)stringID {
  [self.consentStringIDs addObject:[NSNumber numberWithInt:stringID]];
}

- (void)didTapPrimaryActionButton {
  if (self.mediator.selectedIdentity) {
    [self startSync];
  } else {
    [self triggerAddAccount];
  }
}

- (void)didTapSecondaryActionButton {
  // Cancel sync and sign out the user if needed.
  [self.mediator cancelSyncAndRestoreSigninState:self.signinStateOnStart
                           signinIdentityOnStart:self.signinIdentityOnStart];
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

#pragma mark - SigninSyncMediatorDelegate

- (void)signinSyncMediatorDidSuccessfulyFinishSignin:
    (SigninSyncMediator*)mediator {
  if (self.firstRun) {
    base::UmaHistogramEnumeration("FirstRun.Stage",
                                  first_run::kSyncScreenCompletionWithSync);
  }
  [self.delegate willFinishPresenting];
}

- (void)signinSyncMediatorDidSuccessfulyFinishSigninForAdvancedSettings:
    (SigninSyncMediator*)mediator {
  [self showAdvancedSettings];
}

- (void)signinSyncMediatorDidSuccessfulyFinishSignout:
    (SigninSyncMediator*)mediator {
  [self finishPresentingAndSkipRemainingScreens:NO];
  if (self.firstRun) {
    base::UmaHistogramEnumeration(
        "FirstRun.Stage", first_run::kSignInScreenCompletionWithoutSignIn);
  }
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

// Shows the advanced sync settings on top of the screen.
- (void)showAdvancedSettings {
  DCHECK(!self.advancedSettingsSigninCoordinator);

  self.advancedSettingsSigninCoordinator = [SigninCoordinator
      advancedSettingsSigninCoordinatorWithBaseViewController:
          self.viewController
                                                      browser:self.browser
                                                  signinState:
                                                      self.signinStateOnStart];
  __weak __typeof(self) weakSelf = self;
  self.advancedSettingsSigninCoordinator.signinCompletion =
      ^(SigninCoordinatorResult advancedSigninResult,
        SigninCompletionInfo* signinCompletionInfo) {
        [weakSelf onAdvancedSettingsFinished];
      };
  [self.advancedSettingsSigninCoordinator start];
}

// Starts syncing.
- (void)startSync {
  DCHECK(self.mediator.selectedIdentity);

  AuthenticationFlow* authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowser:self.browser
                                         identity:self.mediator.selectedIdentity
                                  shouldClearData:SHOULD_CLEAR_DATA_USER_CHOICE
                                 postSignInAction:POST_SIGNIN_ACTION_COMMIT_SYNC
                         presentingViewController:self.viewController];
  authenticationFlow.dispatcher = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowsingDataCommands);

  [self.mediator
      startSyncWithConfirmationID:[self.viewController activateSyncButtonID]
                       consentIDs:self.consentStringIDs
               authenticationFlow:authenticationFlow];
}

// Called when advanced settings are dismissed/finished.
- (void)onAdvancedSettingsFinished {
  DCHECK(self.advancedSettingsSigninCoordinator);

  [self.advancedSettingsSigninCoordinator stop];
  self.advancedSettingsSigninCoordinator = nil;
}

@end
