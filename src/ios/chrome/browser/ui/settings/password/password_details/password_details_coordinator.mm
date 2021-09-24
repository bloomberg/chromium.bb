// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordDetailsCoordinator () <PasswordDetailsHandler> {
  password_manager::PasswordForm _password;

  // Manager responsible for password check feature.
  IOSChromePasswordCheckManager* _manager;
}

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordDetailsTableViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordDetailsMediator* mediator;

// Module containing the reauthentication mechanism for viewing and copying
// passwords.
@property(nonatomic, weak) ReauthenticationModule* reauthenticationModule;

// Denotes the type of the credential passed to this coordinator. Could be
// blocked, federated, new or regular.
@property(nonatomic, assign) CredentialType credentialType;

// Modal alert for interactions with password.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// The action sheet coordinator, if one is currently being shown.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

// Dispatcher.
@property(nonatomic, weak) id<ApplicationCommands, BrowserCommands> dispatcher;

@end

@implementation PasswordDetailsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                            password:
                                (const password_manager::PasswordForm&)password
                        reauthModule:(ReauthenticationModule*)reauthModule
                passwordCheckManager:(IOSChromePasswordCheckManager*)manager {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    DCHECK(navigationController);
    DCHECK(manager);

    _baseNavigationController = navigationController;
    _password = password;
    _manager = manager;
    _reauthenticationModule = reauthModule;
    _credentialType = password.blocked_by_user ? CredentialTypeBlocked
                                               : CredentialTypeRegular;
    if (_credentialType == CredentialTypeRegular &&
        !_password.federation_origin.opaque()) {
      _credentialType = CredentialTypeFederation;
    }
    _dispatcher = static_cast<id<BrowserCommands, ApplicationCommands>>(
        browser->GetCommandDispatcher());
  }
  return self;
}

- (void)start {
  self.viewController = [[PasswordDetailsTableViewController alloc]
      initWithCredentialType:_credentialType];

  self.mediator = [[PasswordDetailsMediator alloc] initWithPassword:_password
                                               passwordCheckManager:_manager];
  self.mediator.consumer = self.viewController;
  self.viewController.handler = self;
  self.viewController.delegate = self.mediator;
  self.viewController.commandsHandler = self.dispatcher;
  self.viewController.reauthModule = self.reauthenticationModule;

  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewController = nil;
}

#pragma mark - PasswordDetailsHandler

- (void)passwordDetailsTableViewControllerDidDisappear {
  [self.delegate passwordDetailsCoordinatorDidRemove:self];
}

- (void)showPasscodeDialog {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE);
  NSString* message =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_CONTENT);
  self.alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self.viewController
                                                   browser:self.browser
                                                     title:title
                                                   message:message];

  __weak __typeof(self) weakSelf = self;
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:GURL(kPasscodeArticleURL)];

  [self.alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                                   action:nil
                                    style:UIAlertActionStyleCancel];

  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_LEARN_HOW)
                action:^{
                  [weakSelf.dispatcher closeSettingsUIAndOpenURL:command];
                }
                 style:UIAlertActionStyleDefault];

  [self.alertCoordinator start];
}

- (void)showPasswordDeleteDialogWithOrigin:(NSString*)origin
                       compromisedPassword:(BOOL)compromisedPassword {
  NSString* message;

  if (origin.length > 0) {
    int stringID = compromisedPassword
                       ? IDS_IOS_DELETE_COMPROMISED_PASSWORD_DESCRIPTION
                       : IDS_IOS_DELETE_PASSWORD_DESCRIPTION;
    message =
        l10n_util::GetNSStringF(stringID, base::SysNSStringToUTF16(origin));
  }
  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                           title:nil
                         message:message
                   barButtonItem:self.viewController.deleteButton];

  __weak __typeof(self) weakSelf = self;

  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_CONFIRM_PASSWORD_DELETION)
                action:^{
                  [weakSelf passwordDeletionConfirmedForCompromised:
                                compromisedPassword];
                }
                 style:UIAlertActionStyleDestructive];

  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_CANCEL_PASSWORD_DELETION)
                action:nil
                 style:UIAlertActionStyleCancel];

  [self.actionSheetCoordinator start];
}

- (void)showPasswordEditDialogWithOrigin:(NSString*)origin {
  NSString* message = l10n_util::GetNSStringF(IDS_IOS_EDIT_PASSWORD_DESCRIPTION,
                                              base::SysNSStringToUTF16(origin));
  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                           title:nil
                         message:message
                   barButtonItem:self.viewController.navigationItem
                                     .rightBarButtonItem];

  __weak __typeof(self) weakSelf = self;

  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_CONFIRM_PASSWORD_EDIT)
                action:^{
                  [weakSelf.viewController passwordEditingConfirmed];
                }
                 style:UIAlertActionStyleDefault];

  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_CANCEL_PASSWORD_EDIT)
                action:nil
                 style:UIAlertActionStyleCancel];

  [self.actionSheetCoordinator start];
}

#pragma mark - Private

// Notifies delegate about password deletion and records metric if needed.
- (void)passwordDeletionConfirmedForCompromised:(BOOL)compromised {
  [self.delegate passwordDetailsCoordinator:self
                             deletePassword:self.mediator.password];
  if (compromised) {
    base::UmaHistogramEnumeration(
        "PasswordManager.BulkCheck.UserAction",
        password_manager::metrics_util::PasswordCheckInteraction::
            kRemovePassword);
  }
}

@end
