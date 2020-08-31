// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_coordinator.h"

#include "base/mac/foundation_util.h"
#include "components/google/core/common/google_util.h"
#include "components/sync/driver/sync_service_utils.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@interface GoogleServicesSettingsCoordinator () <
    GoogleServicesSettingsCommandHandler,
    GoogleServicesSettingsViewControllerPresentationDelegate,
    ManageSyncSettingsCoordinatorDelegate>

// Google services settings mode.
@property(nonatomic, assign, readonly) GoogleServicesSettingsMode mode;
// Google services settings mediator.
@property(nonatomic, strong) GoogleServicesSettingsMediator* mediator;
// Returns the authentication service.
@property(nonatomic, assign, readonly) AuthenticationService* authService;
// Manages the authentication flow for a given identity.
@property(nonatomic, strong) AuthenticationFlow* authenticationFlow;
// View controller presented by this coordinator.
@property(nonatomic, strong, readonly)
    GoogleServicesSettingsViewController* googleServicesSettingsViewController;
// Coordinator to present the manage sync settings.
@property(nonatomic, strong)
    ManageSyncSettingsCoordinator* manageSyncSettingsCoordinator;
// YES if stop has been called.
@property(nonatomic, assign) BOOL stopDone;
// YES if the last sign-in has been interrupted. In that case, the coordinator
// is going to be stopped, and the sync setup flag should not be marked as done.
// And the sync should not be marked as disabled. The sync should be kept
// undecided.
@property(nonatomic, assign) BOOL signinInterrupted;

@end

@implementation GoogleServicesSettingsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                            mode:(GoogleServicesSettingsMode)
                                                     mode {
  if ([super initWithBaseViewController:navigationController browser:browser]) {
    _baseNavigationController = navigationController;
    _mode = mode;
  }
  return self;
}

- (void)dealloc {
  // -[GoogleServicesSettingsCoordinator stop] needs to be called explicitly.
  DCHECK(self.stopDone);
}

- (void)start {
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;

  GoogleServicesSettingsViewController* viewController =
      [[GoogleServicesSettingsViewController alloc] initWithStyle:style];
  viewController.presentationDelegate = self;
  self.viewController = viewController;
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.mediator = [[GoogleServicesSettingsMediator alloc]
      initWithUserPrefService:self.browser->GetBrowserState()->GetPrefs()
             localPrefService:GetApplicationContext()->GetLocalState()
             syncSetupService:syncSetupService
                         mode:self.mode];
  self.mediator.consumer = viewController;
  self.mediator.authService = self.authService;
  self.mediator.identityManager = IdentityManagerFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.mediator.commandHandler = self;
  self.mediator.syncService = ProfileSyncServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  viewController.modelDelegate = self.mediator;
  viewController.serviceDelegate = self.mediator;
  DCHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  if (self.stopDone) {
    return;
  }
  // Sync changes should only be commited if the user is authenticated and
  // the sign-in has not been interrupted.
  if (self.authService->IsAuthenticated() && !self.signinInterrupted) {
    SyncSetupService* syncSetupService =
        SyncSetupServiceFactory::GetForBrowserState(
            self.browser->GetBrowserState());
    if (self.mode == GoogleServicesSettingsModeSettings &&
        syncSetupService->GetSyncServiceState() ==
            SyncSetupService::kSyncSettingsNotConfirmed) {
      // If Sync is still in aborted state, this means the user didn't turn on
      // sync, and wants Sync off. To acknowledge, Sync has to be turned off.
      syncSetupService->SetSyncEnabled(false);
    }
    syncSetupService->CommitSyncChanges();
  }
  self.stopDone = YES;
}

#pragma mark - Private

- (void)authenticationFlowDidComplete {
  DCHECK(self.authenticationFlow);
  self.authenticationFlow = nil;
  [self.googleServicesSettingsViewController allowUserInteraction];
}

#pragma mark - Properties

- (AuthenticationService*)authService {
  return AuthenticationServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
}

- (GoogleServicesSettingsViewController*)googleServicesSettingsViewController {
  return base::mac::ObjCCast<GoogleServicesSettingsViewController>(
      self.viewController);
}

#pragma mark - GoogleServicesSettingsCommandHandler

- (void)restartAuthenticationFlow {
  ChromeIdentity* authenticatedIdentity =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          ->GetAuthenticatedIdentity();
  [self.googleServicesSettingsViewController preventUserInteraction];
  DCHECK(!self.authenticationFlow);
  self.authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowser:self.browser
                                         identity:authenticatedIdentity
                                  shouldClearData:SHOULD_CLEAR_DATA_USER_CHOICE
                                 postSignInAction:POST_SIGNIN_ACTION_START_SYNC
                         presentingViewController:self.viewController];
  self.authenticationFlow.dispatcher = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowsingDataCommands);
  __weak GoogleServicesSettingsCoordinator* weakSelf = self;
  [self.authenticationFlow startSignInWithCompletion:^(BOOL success) {
    // TODO(crbug.com/889919): Needs to add histogram for |success|.
    [weakSelf authenticationFlowDidComplete];
  }];
}

- (void)openReauthDialogAsSyncIsInAuthError {
  ChromeIdentity* identity = self.authService->GetAuthenticatedIdentity();
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

- (void)openPassphraseDialog {
  SyncEncryptionPassphraseTableViewController* controller =
      [[SyncEncryptionPassphraseTableViewController alloc]
          initWithBrowserState:self.browser->GetBrowserState()];
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  controller.dispatcher = static_cast<
      id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>>(
      self.browser->GetCommandDispatcher());
  [self.baseNavigationController pushViewController:controller animated:YES];
}

- (void)showSignIn {
  __weak __typeof(self) weakSelf = self;
  DCHECK(self.dispatcher);
  signin_metrics::RecordSigninUserActionForAccessPoint(
      AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS,
      PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AUTHENTICATION_OPERATION_SIGNIN
               identity:nil
            accessPoint:AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS
            promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO
               callback:^(BOOL success) {
                 [weakSelf signinFinishedWithSuccess:success];
               }];
  [self.dispatcher showSignin:command
           baseViewController:self.googleServicesSettingsViewController];
}

- (void)signinFinishedWithSuccess:(BOOL)success {
  // TODO(crbug.com/971989): SigninCoordinatorResult should be received instead
  // of guessing if the sign-in has been interrupted.
  ChromeIdentity* primaryAccount =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          ->GetAuthenticatedIdentity();
  self.signinInterrupted = !success && primaryAccount;
}

- (void)openAccountSettings {
  AccountsTableViewController* controller =
      [[AccountsTableViewController alloc] initWithBrowser:self.browser
                                 closeSettingsOnAddAccount:NO];
  controller.dispatcher = static_cast<
      id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>>(
      self.browser->GetCommandDispatcher());
  [self.baseNavigationController pushViewController:controller animated:YES];
}

- (void)openManageSyncSettings {
  DCHECK(!self.manageSyncSettingsCoordinator);
  self.manageSyncSettingsCoordinator = [[ManageSyncSettingsCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  self.manageSyncSettingsCoordinator.delegate = self;
  [self.manageSyncSettingsCoordinator start];
}

- (void)openManageGoogleAccount {
  ios::GetChromeBrowserProvider()
      ->GetChromeIdentityService()
      ->PresentAccountDetailsController(
          self.authService->GetAuthenticatedIdentity(),
          self.googleServicesSettingsViewController, /*animated=*/YES);
}

- (void)openManageGoogleAccountWebPage {
  GURL url = google_util::AppendGoogleLocaleParam(
      GURL(kManageYourGoogleAccountURL),
      GetApplicationContext()->GetApplicationLocale());
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:url];
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [handler closeSettingsUIAndOpenURL:command];
}

- (void)openTrustedVaultReauth {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  [applicationCommands
      showTrustedVaultReauthenticationFromViewController:
          self.googleServicesSettingsViewController
                                        retrievalTrigger:
                                            syncer::KeyRetrievalTriggerForUMA::
                                                kSettings];
}

#pragma mark - GoogleServicesSettingsViewControllerPresentationDelegate

- (void)googleServicesSettingsViewControllerDidRemove:
    (GoogleServicesSettingsViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate googleServicesSettingsCoordinatorDidRemove:self];
}

#pragma mark - ManageSyncSettingsCoordinatorDelegate

- (void)manageSyncSettingsCoordinatorWasRemoved:
    (ManageSyncSettingsCoordinator*)coordinator {
  DCHECK_EQ(self.manageSyncSettingsCoordinator, coordinator);
  [self.manageSyncSettingsCoordinator stop];
  self.manageSyncSettingsCoordinator = nil;
}

@end
