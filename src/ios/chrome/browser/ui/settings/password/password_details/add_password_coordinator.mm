// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/add_password_coordinator.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AddPasswordCoordinator () <
    AddPasswordHandler,
    AddPasswordMediatorDelegate,
    UIAdaptivePresentationControllerDelegate> {
  // Manager responsible for getting existing password profiles.
  IOSChromePasswordCheckManager* _manager;
}

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordDetailsTableViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) AddPasswordMediator* mediator;

// Module containing the reauthentication mechanism for editing existing
// passwords.
@property(nonatomic, weak) ReauthenticationModule* reauthenticationModule;

// Modal alert for interactions with password.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// Dispatcher.
@property(nonatomic, weak) id<ApplicationCommands, BrowserCommands> dispatcher;

@end

@implementation AddPasswordCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              reauthModule:(ReauthenticationModule*)reauthModule
                      passwordCheckManager:
                          (IOSChromePasswordCheckManager*)manager {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    DCHECK(viewController);
    DCHECK(manager);
    DCHECK(reauthModule);
    _manager = manager;
    _reauthenticationModule = reauthModule;
    _dispatcher = static_cast<id<BrowserCommands, ApplicationCommands>>(
        browser->GetCommandDispatcher());
  }
  return self;
}

- (void)start {
  self.viewController = [[PasswordDetailsTableViewController alloc]
      initWithCredentialType:CredentialTypeNew];

  self.mediator = [[AddPasswordMediator alloc] initWithDelegate:self
                                           passwordCheckManager:_manager];
  self.mediator.consumer = self.viewController;
  self.viewController.delegate = self.mediator;
  self.viewController.addPasswordHandler = self;
  self.viewController.reauthModule = self.reauthenticationModule;

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.viewController];
  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  navigationController.presentationController.delegate = self;

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.viewController.navigationController dismissViewControllerAnimated:YES
                                                               completion:nil];
  self.mediator = nil;
  self.viewController = nil;
}

#pragma mark - AddPasswordMediatorDelegate

- (void)dismissPasswordDetailsTableViewController {
  [self.delegate passwordDetailsTableViewControllerDidFinish:self];
}

- (void)showReplacePasswordAlert:(NSString*)username
                         hostUrl:(NSString*)hostUrl {
  // TODO(crbug.com/1226006): Use i18n strings for title, message and the
  // buttons.
  NSString* title = @"Replace Password?";
  NSString* message = [NSString
      stringWithFormat:@"You already saved a password for \"%@\" at %@. "
                       @"Do you want to replace it?",
                       username, hostUrl];
  self.alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self.viewController
                                                   browser:self.browser
                                                     title:title
                                                   message:message];

  __weak __typeof(self) weakSelf = self;

  [self.alertCoordinator addItemWithTitle:@"Cancel"
                                   action:nil
                                    style:UIAlertActionStyleCancel];

  [self.alertCoordinator
      addItemWithTitle:@"Replace"
                action:^{
                  if (!weakSelf) {
                    return;
                  }
                  [weakSelf.viewController
                          validateUserAndReplaceExistingCredential];
                }
                 style:UIAlertActionStyleDestructive];

  [self.alertCoordinator start];
}

- (void)setUpdatedPasswordForm:
    (const password_manager::PasswordForm&)passwordForm {
  [self.delegate setMostRecentlyUpdatedPasswordDetails:passwordForm];
}

#pragma mark - AddPasswordHandler

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

@end
