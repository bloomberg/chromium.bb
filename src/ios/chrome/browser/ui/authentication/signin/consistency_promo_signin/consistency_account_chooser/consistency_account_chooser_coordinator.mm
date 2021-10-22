// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_mediator.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_table_view_controller_action_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_view_controller.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ConsistencyAccountChooserCoordinator () <
    ConsistencyAccountChooserTableViewControllerActionDelegate>

@property(nonatomic, strong)
    ConsistencyAccountChooserViewController* accountChooserViewController;

@property(nonatomic, strong) ConsistencyAccountChooserMediator* mediator;

@end

@implementation ConsistencyAccountChooserCoordinator

- (void)startWithSelectedIdentity:(ChromeIdentity*)selectedIdentity {
  [super start];
  self.mediator = [[ConsistencyAccountChooserMediator alloc]
      initWithSelectedIdentity:selectedIdentity
         accountManagerService:ChromeAccountManagerServiceFactory::
                                   GetForBrowserState(
                                       self.browser->GetBrowserState())];

  self.accountChooserViewController =
      [[ConsistencyAccountChooserViewController alloc] init];
  self.accountChooserViewController.modelDelegate = self.mediator;
  self.mediator.consumer = self.accountChooserViewController.consumer;
  self.accountChooserViewController.actionDelegate = self;
  self.accountChooserViewController.layoutDelegate = self.layoutDelegate;
  [self.accountChooserViewController view];
}

- (void)stop {
  [super stop];
  [self.mediator disconnect];
}

#pragma mark - Properties

- (UIViewController*)viewController {
  return self.accountChooserViewController;
}

- (ChromeIdentity*)selectedIdentity {
  return self.mediator.selectedIdentity;
}

#pragma mark - ConsistencyAccountChooserTableViewControllerActionDelegate

- (void)consistencyAccountChooserTableViewController:
            (ConsistencyAccountChooserTableViewController*)viewController
                         didSelectIdentityWithGaiaID:(NSString*)gaiaID {
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  ChromeIdentity* identity = accountManagerService->GetIdentityWithGaiaID(
      base::SysNSStringToUTF8(gaiaID));
  DCHECK(identity);
  self.mediator.selectedIdentity = identity;
  [self.delegate
      consistencyAccountChooserCoordinatorChromeIdentitySelected:self];
}

- (void)consistencyAccountChooserTableViewControllerDidTapOnAddAccount:
    (ConsistencyAccountChooserTableViewController*)viewController {
  [self.delegate consistencyAccountChooserCoordinatorOpenAddAccount:self];
}

- (void)showManagementHelpPage {
  OpenNewTabCommand* command = [OpenNewTabCommand
      commandWithURLFromChrome:GURL(kManagementLearnMoreURL)];
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [handler closeSettingsUIAndOpenURL:command];
}

@end
