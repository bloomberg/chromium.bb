// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_coordinator.h"

#include "base/check_op.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/google/core/common/google_util.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/sync/driver/sync_user_settings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/sync/sync_observer_bridge.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/sync_error_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@interface ManageSyncSettingsCoordinator () <
    ManageSyncSettingsCommandHandler,
    SyncErrorSettingsCommandHandler,
    ManageSyncSettingsTableViewControllerPresentationDelegate,
    SyncObserverModelBridge,
    SyncSettingsViewState> {
  // Sync observer.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
}

// View controller.
@property(nonatomic, strong)
    ManageSyncSettingsTableViewController* viewController;
// Mediator.
@property(nonatomic, strong) ManageSyncSettingsMediator* mediator;
// Sync service.
@property(nonatomic, assign, readonly) syncer::SyncService* syncService;
// Authentication service.
@property(nonatomic, assign, readonly) AuthenticationService* authService;
// Dismiss callback for Web and app setting details view.
@property(nonatomic, copy) ios::DismissASMViewControllerBlock
    dismissWebAndAppSettingDetailsControllerBlock;
// Manages the authentication flow for a given identity.
@property(nonatomic, strong) AuthenticationFlow* authenticationFlow;
// YES if the last sign-in has been interrupted. In that case, the sync UI will
// be dismissed and the sync setup flag should not be marked as done. The sync
// should be kept undecided, not marked as disabled.
@property(nonatomic, assign) BOOL signinInterrupted;
// Displays the sign-out options for a syncing user.
@property(nonatomic, strong) SignoutActionSheetCoordinator* signOutCoordinator;

@end

@implementation ManageSyncSettingsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  if (self = [super initWithBaseViewController:navigationController
                                       browser:browser]) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  DCHECK(self.baseNavigationController);
  self.mediator = [[ManageSyncSettingsMediator alloc]
      initWithSyncService:self.syncService
          userPrefService:self.browser->GetBrowserState()->GetPrefs()];
  self.mediator.syncSetupService = SyncSetupServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.mediator.commandHandler = self;
  self.mediator.syncErrorHandler = self;
  self.viewController = [[ManageSyncSettingsTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.viewController.title = self.delegate.manageSyncSettingsCoordinatorTitle;
  self.viewController.serviceDelegate = self.mediator;
  self.viewController.presentationDelegate = self;
  self.viewController.modelDelegate = self.mediator;
  self.mediator.consumer = self.viewController;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
  _syncObserver.reset(new SyncObserverBridge(self, self.syncService));
}

- (void)stop {
  // If kMobileIdentityConsistency is disabled,
  // GoogleServicesSettingsCoordinator is in charge to enable sync or not when
  // being closed. This coordinator displays a sub view.
  // With kMobileIdentityConsistency enabled:
  // This coordinator displays the main view and it is in charge to enable sync
  // or not when being closed.
  // Sync changes should only be commited if the user is authenticated and
  // the sign-in has not been interrupted.
  if (base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency) &&
      (self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin) ||
       !self.signinInterrupted)) {
    SyncSetupService* syncSetupService =
        SyncSetupServiceFactory::GetForBrowserState(
            self.browser->GetBrowserState());
    if (syncSetupService->GetSyncServiceState() ==
        SyncSetupService::kSyncSettingsNotConfirmed) {
      // If Sync is still in aborted state, this means the user didn't turn on
      // sync, and wants Sync off. To acknowledge, Sync has to be turned off.
      syncSetupService->SetSyncEnabled(false);
    }
    syncSetupService->CommitSyncChanges();
  }
}

#pragma mark - Properties

- (syncer::SyncService*)syncService {
  return SyncServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
}

- (AuthenticationService*)authService {
  return AuthenticationServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
}

#pragma mark - SyncSettingsViewState

- (BOOL)isSettingsViewShown {
  return [self.viewController
      isEqual:self.baseNavigationController.topViewController];
}

- (UINavigationItem*)navigationItem {
  return self.viewController.navigationItem;
}

#pragma mark - Private

// Closes the Manage sync settings view controller.
- (void)closeManageSyncSettings {
  if (self.viewController.navigationController) {
    if (self.dismissWebAndAppSettingDetailsControllerBlock) {
      self.dismissWebAndAppSettingDetailsControllerBlock(NO);
      self.dismissWebAndAppSettingDetailsControllerBlock = nil;
    }
    [self.baseNavigationController popToViewController:self.viewController
                                              animated:NO];
    [self.baseNavigationController popViewControllerAnimated:YES];
  }
}

- (void)signinFinishedWithSuccess:(BOOL)success {
  DCHECK(self.authenticationFlow);
  self.authenticationFlow = nil;
  [self.viewController allowUserInteraction];

  ChromeIdentity* primaryAccount =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          ->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  // TODO(crbug.com/1101346): SigninCoordinatorResult should be received instead
  // of guessing if the sign-in has been interrupted.
  self.signinInterrupted = !success && primaryAccount;
}

#pragma mark - ManageSyncSettingsTableViewControllerPresentationDelegate

- (void)manageSyncSettingsTableViewControllerWasRemoved:
    (ManageSyncSettingsTableViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate manageSyncSettingsCoordinatorWasRemoved:self];
}

#pragma mark - ManageSyncSettingsCommandHandler

- (void)openWebAppActivityDialog {
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  base::RecordAction(base::UserMetricsAction(
      "Signin_AccountSettings_GoogleActivityControlsClicked"));
  self.dismissWebAndAppSettingDetailsControllerBlock =
      ios::GetChromeBrowserProvider()
          .GetChromeIdentityService()
          ->PresentWebAndAppSettingDetailsController(
              authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin),
              self.viewController, YES);
}

- (void)openDataFromChromeSyncWebPage {
  if ([self.delegate
          respondsToSelector:@selector
          (manageSyncSettingsCoordinatorNeedToOpenChromeSyncWebPage:)]) {
    [self.delegate
        manageSyncSettingsCoordinatorNeedToOpenChromeSyncWebPage:self];
  }
  GURL url = google_util::AppendGoogleLocaleParam(
      GURL(kSyncGoogleDashboardURL),
      GetApplicationContext()->GetApplicationLocale());
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:url];
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [handler closeSettingsUIAndOpenURL:command];
}

- (void)showTurnOffSyncOptionsFromTargetRect:(CGRect)targetRect {
  self.signOutCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                            rect:targetRect
                            view:self.viewController.view];
  __weak ManageSyncSettingsCoordinator* weakSelf = self;
  self.signOutCoordinator.completion = ^(BOOL success) {
    if (success) {
      [weakSelf closeManageSyncSettings];
    }
  };
  [self.signOutCoordinator start];
}

#pragma mark - SyncErrorSettingsCommandHandler

- (void)openPassphraseDialog {
  DCHECK(self.mediator.shouldEncryptionItemBeEnabled);
  UIViewController<SettingsRootViewControlling>* controllerToPush;
  // If there was a sync error, prompt the user to enter the passphrase.
  // Otherwise, show the full encryption options.
  if (self.syncService->GetUserSettings()->IsPassphraseRequired()) {
    controllerToPush = [[SyncEncryptionPassphraseTableViewController alloc]
        initWithBrowser:self.browser];
  } else {
    controllerToPush = [[SyncEncryptionTableViewController alloc]
        initWithBrowser:self.browser];
  }
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  controllerToPush.dispatcher = static_cast<
      id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>>(
      self.browser->GetCommandDispatcher());
  [self.baseNavigationController pushViewController:controllerToPush
                                           animated:YES];
}

- (void)openTrustedVaultReauthForFetchKeys {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  [applicationCommands
      showTrustedVaultReauthForFetchKeysFromViewController:self.viewController
                                                   trigger:
                                                       syncer::
                                                           TrustedVaultUserActionTriggerForUMA::
                                                               kSettings];
}

- (void)openTrustedVaultReauthForDegradedRecoverability {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  [applicationCommands
      showTrustedVaultReauthForDegradedRecoverabilityFromViewController:
          self.viewController
                                                                trigger:
                                                                    syncer::TrustedVaultUserActionTriggerForUMA::
                                                                        kSettings];
}

- (void)restartAuthenticationFlow {
  ChromeIdentity* authenticatedIdentity =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          ->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  [self.viewController preventUserInteraction];
  DCHECK(!self.authenticationFlow);
  self.authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowser:self.browser
                                         identity:authenticatedIdentity
                                  shouldClearData:SHOULD_CLEAR_DATA_USER_CHOICE
                                 postSignInAction:POST_SIGNIN_ACTION_START_SYNC
                         presentingViewController:self.viewController];
  self.authenticationFlow.dispatcher = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowsingDataCommands);
  __weak ManageSyncSettingsCoordinator* weakSelf = self;
  [self.authenticationFlow startSignInWithCompletion:^(BOOL success) {
    [weakSelf signinFinishedWithSuccess:success];
  }];
}

- (void)openReauthDialogAsSyncIsInAuthError {
  ChromeIdentity* identity =
      self.authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (self.authService->HasCachedMDMErrorForIdentity(identity)) {
    self.authService->ShowMDMErrorDialogForIdentity(identity);
    return;
  }
  // Sync enters in a permanent auth error state when fetching an access token
  // fails with invalid credentials. This corresponds to Gaia responding with an
  // "invalid grant" error. The current implementation of the iOS SSOAuth
  // library user by Chrome removes the identity from the device when receiving
  // an "invalid grant" response, which leads to the account being also signed
  // out of Chrome. So the sync permanent auth error is a transient state on
  // iOS. The decision was to avoid handling this error in the UI, which means
  // that the reauth dialog is not actually presented on iOS.
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  syncer::SyncService::DisableReasonSet disableReasons =
      self.syncService->GetDisableReasons();
  bool isMICeEnabled =
      base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency);
  syncer::SyncService::DisableReasonSet userChoiceDisableReason =
      syncer::SyncService::DisableReasonSet(
          syncer::SyncService::DISABLE_REASON_USER_CHOICE);
  // MICe: manage sync settings needs to stay opened if sync is disabled with
  // DISABLE_REASON_USER_CHOICE. Manage sync settings is the only way for a
  // user to turn on the sync engine (and remove DISABLE_REASON_USER_CHOICE).
  // The sync engine turned back on automatically by enabling any datatype.
  // A pre-MICe signed in user who migrated to MICe, might have sync disabled.
  bool closeSyncSettingsWithMice =
      isMICeEnabled &&
      (!disableReasons.Empty() && disableReasons != userChoiceDisableReason);
  // Pre-MICe: manage sync settings needs to be closed if the sync is disabled.
  bool closeSyncSettingsPreMICE = !isMICeEnabled && !disableReasons.Empty();
  if (closeSyncSettingsWithMice || closeSyncSettingsPreMICE) {
    [self closeManageSyncSettings];
  }
}

@end
