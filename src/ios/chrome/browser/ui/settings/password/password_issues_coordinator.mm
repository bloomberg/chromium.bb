// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues_coordinator.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_issue_with_form.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_presenter.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_table_view_controller.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordIssuesCoordinator () <PasswordDetailsCoordinatorDelegate,
                                         PasswordIssuesPresenter> {
  // Password check manager to power mediator.
  IOSChromePasswordCheckManager* _manager;
}

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordIssuesTableViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordIssuesMediator* mediator;

// Coordinator for password details.
@property(nonatomic, strong) PasswordDetailsCoordinator* passwordDetails;

@end

@implementation PasswordIssuesCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                            passwordCheckManager:
                                (IOSChromePasswordCheckManager*)manager {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _manager = manager;
    _dispatcher = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                     ApplicationCommands);
  }
  return self;
}

- (void)start {
  [super start];
  // To start, a password check manager should be ready.
  DCHECK(_manager);

  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;

  self.viewController =
      [[PasswordIssuesTableViewController alloc] initWithStyle:style];

  self.mediator =
      [[PasswordIssuesMediator alloc] initWithPasswordCheckManager:_manager];
  // If reauthentication module was not provided, coordinator will create its
  // own.
  if (!self.reauthModule) {
    self.reauthModule = [[ReauthenticationModule alloc]
        initWithSuccessfulReauthTimeAccessor:self.mediator];
  }

  self.mediator.consumer = self.viewController;
  self.viewController.presenter = self;

  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  self.mediator = nil;
  self.viewController = nil;

  [self.passwordDetails stop];
  self.passwordDetails.delegate = nil;
  self.passwordDetails = nil;
}

#pragma mark - PasswordIssuesPresenter

- (void)dismissPasswordIssuesTableViewController {
  [self.delegate passwordIssuesCoordinatorDidRemove:self];
}

- (void)presentPasswordIssueDetails:(id<PasswordIssue>)password {
  password_manager::PasswordForm form =
      base::mac::ObjCCastStrict<PasswordIssueWithForm>(password).form;

  DCHECK(!self.passwordDetails);
  self.passwordDetails = [[PasswordDetailsCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser
                              password:form
                          reauthModule:self.reauthModule
                  passwordCheckManager:_manager];
  self.passwordDetails.delegate = self;
  [self.passwordDetails start];
}

#pragma mark - PasswordDetailsCoordinatorDelegate

- (void)passwordDetailsCoordinatorDidRemove:
    (PasswordDetailsCoordinator*)coordinator {
  DCHECK_EQ(self.passwordDetails, coordinator);
  [self.passwordDetails stop];
  self.passwordDetails.delegate = nil;
  self.passwordDetails = nil;
}

- (void)passwordDetailsCoordinator:(PasswordDetailsCoordinator*)coordinator
                    deletePassword:
                        (const password_manager::PasswordForm&)password {
  if (![self.delegate willHandlePasswordDeletion:password]) {
    [self.mediator deletePassword:password];
    [self.baseNavigationController popViewControllerAnimated:YES];
  }
}

@end
