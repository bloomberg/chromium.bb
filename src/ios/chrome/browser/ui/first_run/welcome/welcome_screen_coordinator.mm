// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/welcome/welcome_screen_coordinator.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#include "ios/chrome/browser/ui/first_run/welcome/tos_commands.h"
#include "ios/chrome/browser/ui/first_run/welcome/tos_coordinator.h"
#include "ios/chrome/browser/ui/first_run/welcome/welcome_screen_mediator.h"
#include "ios/chrome/browser/ui/first_run/welcome/welcome_screen_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WelcomeScreenCoordinator () <WelcomeScreenViewControllerDelegate>

@property(nonatomic, weak) id<FirstRunScreenDelegate> delegate;

// Welcome screen view controller.
@property(nonatomic, strong) WelcomeScreenViewController* viewController;

// Welcome screen mediator.
@property(nonatomic, strong) WelcomeScreenMediator* mediator;

// Whether the user tapped on the TOS link.
@property(nonatomic, assign) BOOL TOSLinkWasTapped;

// Coordinator used to manage the TOS page.
@property(nonatomic, strong) TOSCoordinator* TOSCoordinator;

@end

@implementation WelcomeScreenCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _delegate = delegate;
  }
  return self;
}

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(TOSCommands)];
  id<TOSCommands> TOSHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), TOSCommands);

  self.viewController =
      [[WelcomeScreenViewController alloc] initWithTOSHandler:TOSHandler];
  self.viewController.delegate = self;
  self.mediator = [[WelcomeScreenMediator alloc] init];

  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ self.viewController ]
                                           animated:animated];
  if (@available(iOS 13, *)) {
    self.viewController.modalInPresentation = YES;
  }
}

- (void)stop {
  self.delegate = nil;
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark - WelcomeScreenViewControllerDelegate

- (BOOL)isCheckboxSelectedByDefault {
  return [self.mediator isCheckboxSelectedByDefault];
}

- (void)didTapPrimaryActionButton {
  // TODO(crbug.com/1189815): Remember that the welcome screen has been shown in
  // NSUserDefaults.
  [self.mediator
      setMetricsReportingEnabled:self.viewController.checkBoxSelected];
  if (self.TOSLinkWasTapped) {
    base::RecordAction(base::UserMetricsAction("MobileFreTOSLinkTapped"));
  }

  [self.delegate willFinishPresenting];
}

#pragma mark - TOSCommands

- (void)showTOSPage {
  self.TOSLinkWasTapped = YES;
  self.TOSCoordinator =
      [[TOSCoordinator alloc] initWithBaseViewController:self.viewController
                                                 browser:self.browser];
  [self.TOSCoordinator start];
}

- (void)hideTOSPage {
  [self.TOSCoordinator stop];
  self.TOSCoordinator = nil;
}

@end
