// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services_navigation_coordinator.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_coordinator.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface GoogleServicesNavigationCoordinator ()

// Google services settings coordinator.
@property(nonatomic, strong)
    GoogleServicesSettingsCoordinator* googleServicesSettingsCoordinator;
// Main view controller.
@property(nonatomic, strong) UINavigationController* navigationController;

@end

@implementation GoogleServicesNavigationCoordinator

@synthesize googleServicesSettingsCoordinator =
    _googleServicesSettingsCoordinator;
@synthesize navigationController = _navigationController;
@synthesize delegate = _delegate;

- (void)start {
  self.navigationController = [[UINavigationController alloc] init];
  self.googleServicesSettingsCoordinator =
      [[GoogleServicesSettingsCoordinator alloc]
          initWithBaseViewController:self.navigationController
                        browserState:self.browserState];
  self.googleServicesSettingsCoordinator.navigationController =
      self.navigationController;
  [self.googleServicesSettingsCoordinator start];
  self.googleServicesSettingsCoordinator.viewController.navigationItem
      .leftBarButtonItem = [self closeButton];
  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];
}

// This method should be moved to the view controller.
- (UIBarButtonItem*)closeButton {
  UIBarButtonItem* closeButton =
      [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon closeIcon]
                                          target:self
                                          action:@selector(closeSettings)];
  closeButton.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_CLOSE);
  return closeButton;
}

- (void)closeSettings {
  DCHECK_EQ(self.navigationController,
            self.baseViewController.presentedViewController);
  void (^completion)(void) = ^{
    [self.delegate googleServicesNavigationCoordinatorDidClose:self];
  };
  [self.baseViewController dismissViewControllerAnimated:YES
                                              completion:completion];
}

@end
