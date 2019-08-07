// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_coordinator.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/google/core/common/google_util.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_observer_bridge.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_table_view_controller.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_browser_opener.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ManageSyncSettingsCoordinator () <
    ChromeIdentityBrowserOpener,
    ManageSyncSettingsCommandHandler,
    ManageSyncSettingsTableViewControllerPresentationDelegate,
    SyncObserverModelBridge> {
  // Sync observer.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
}

// View controller.
@property(nonatomic, strong)
    ManageSyncSettingsTableViewController* viewController;
// Mediator.
@property(nonatomic, strong) ManageSyncSettingsMediator* mediator;
// Web and app activity view controller.
@property(nonatomic, weak)
    UINavigationController* webAndAppSettingDetailsController;
// Sync service.
@property(nonatomic, assign, readonly) syncer::SyncService* syncService;

@end

@implementation ManageSyncSettingsCoordinator

- (void)start {
  DCHECK(self.dispatcher);
  DCHECK(self.navigationController);
  self.mediator = [[ManageSyncSettingsMediator alloc]
      initWithSyncService:self.syncService
          userPrefService:self.browserState->GetPrefs()];
  self.mediator.syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(self.browserState);
  self.mediator.commandHandler = self;
  self.viewController = [[ManageSyncSettingsTableViewController alloc]
      initWithTableViewStyle:UITableViewStyleGrouped
                 appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  self.viewController.serviceDelegate = self.mediator;
  self.viewController.presentationDelegate = self;
  self.viewController.modelDelegate = self.mediator;
  self.mediator.consumer = self.viewController;
  [self.navigationController pushViewController:self.viewController
                                       animated:YES];
  _syncObserver.reset(new SyncObserverBridge(self, self.syncService));
}

#pragma mark - Properties

- (syncer::SyncService*)syncService {
  return ProfileSyncServiceFactory::GetForBrowserState(self.browserState);
}

#pragma mark - Private

// Called by the close button of the Web and app activity view controller.
- (void)closeGoogleActivitySettings:(id)sender {
  DCHECK(self.webAndAppSettingDetailsController);
  [self.navigationController dismissViewControllerAnimated:YES completion:nil];
  self.webAndAppSettingDetailsController = nil;
}

// Closes the Manage sync settings view controller.
- (void)closeManageSyncSettings {
  if (self.viewController.navigationController) {
    if (self.webAndAppSettingDetailsController) {
      [self.navigationController dismissViewControllerAnimated:NO
                                                    completion:nil];
      self.webAndAppSettingDetailsController = nil;
    }
    [self.navigationController popToViewController:self.viewController
                                          animated:NO];
    [self.navigationController popViewControllerAnimated:YES];
  }
}

#pragma mark - ManageSyncSettingsTableViewControllerPresentationDelegate

- (void)manageSyncSettingsTableViewControllerWasPopped:
    (ManageSyncSettingsTableViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate manageSyncSettingsCoordinatorWasPopped:self];
}

#pragma mark - ChromeIdentityBrowserOpener

- (void)openURL:(NSURL*)url
              view:(UIView*)view
    viewController:(UIViewController*)viewController {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:net::GURLWithNSURL(url)];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

#pragma mark - ManageSyncSettingsCommandHandler

- (void)openPassphraseDialog {
  DCHECK(self.mediator.shouldEncryptionItemBeEnabled);
  UIViewController<SettingsRootViewControlling>* controllerToPush;
  // If there was a sync error, prompt the user to enter the passphrase.
  // Otherwise, show the full encryption options.
  if (self.syncService->GetUserSettings()->IsPassphraseRequired()) {
    controllerToPush = [[SyncEncryptionPassphraseTableViewController alloc]
        initWithBrowserState:self.browserState];
  } else {
    controllerToPush = [[SyncEncryptionTableViewController alloc]
        initWithBrowserState:self.browserState];
  }
  controllerToPush.dispatcher = self.dispatcher;
  [self.navigationController pushViewController:controllerToPush animated:YES];
}

- (void)openWebAppActivityDialog {
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(self.browserState);
  base::RecordAction(base::UserMetricsAction(
      "Signin_AccountSettings_GoogleActivityControlsClicked"));
  DCHECK(!self.webAndAppSettingDetailsController);
  self.webAndAppSettingDetailsController =
      ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->CreateWebAndAppSettingDetailsController(
              authService->GetAuthenticatedIdentity(), self);
  UIImage* closeIcon = [ChromeIcon closeIcon];
  SEL action = @selector(closeGoogleActivitySettings:);
  [self.webAndAppSettingDetailsController.topViewController navigationItem]
      .leftBarButtonItem = [ChromeIcon templateBarButtonItemWithImage:closeIcon
                                                               target:self
                                                               action:action];
  [self.navigationController
      presentViewController:self.webAndAppSettingDetailsController
                   animated:YES
                 completion:nil];
}

- (void)openDataFromChromeSyncWebPage {
  GURL url = google_util::AppendGoogleLocaleParam(
      GURL(kSyncGoogleDashboardURL),
      GetApplicationContext()->GetApplicationLocale());
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:url];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (self.syncService->GetDisableReasons() !=
      syncer::SyncService::DISABLE_REASON_NONE) {
    [self closeManageSyncSettings];
  }
}

@end
