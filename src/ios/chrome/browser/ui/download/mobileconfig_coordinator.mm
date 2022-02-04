// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/mobileconfig_coordinator.h"

#import <SafariServices/SafariServices.h>

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observation.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/download/mobileconfig_tab_helper.h"
#import "ios/chrome/browser/download/mobileconfig_tab_helper_delegate.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/web_state_list/web_state_dependency_installer_bridge.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kUmaDownloadMobileConfigFileUI[] =
    "Download.IOSDownloadMobileConfigFileUI";

@interface MobileConfigCoordinator () <DependencyInstalling,
                                       MobileConfigTabHelperDelegate,
                                       SFSafariViewControllerDelegate> {
  // Bridge which observes WebStateList and alerts this coordinator when this
  // needs to register the Mediator with a new WebState.
  std::unique_ptr<WebStateDependencyInstallerBridge> _dependencyInstallerBridge;
}

// Coordinator used to display modal alerts to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// SFSafariViewController used to download .mobileconfig file. When a
// mobileconfig is downloaded from a SFSafariViewController, it's directly send
// to the Settings app.
@property(nonatomic, strong) SFSafariViewController* safariViewController;

@end

@implementation MobileConfigCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser {
  if (self = [super initWithBaseViewController:baseViewController
                                       browser:browser]) {
    _dependencyInstallerBridge =
        std::make_unique<WebStateDependencyInstallerBridge>(
            self, browser->GetWebStateList());
  }
  return self;
}

- (void)stop {
  // Reset this observer manually. We want this to go out of scope now, to
  // ensure it detaches before |browser| and its WebStateList get destroyed.
  _dependencyInstallerBridge.reset();

  self.safariViewController = nil;
  [self.alertCoordinator stop];
}

#pragma mark - Private

// Presents SFSafariViewController in order to download .mobileconfig file.
- (void)presentSFSafariViewController:(NSURL*)fileURL {
  base::UmaHistogramEnumeration(
      kUmaDownloadMobileConfigFileUI,
      DownloadMobileConfigFileUI::kSFSafariViewIsPresented);

  self.safariViewController =
      [[SFSafariViewController alloc] initWithURL:fileURL];
  self.safariViewController.delegate = self;
  self.safariViewController.preferredBarTintColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];

  [self.baseViewController presentViewController:self.safariViewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - DependencyInstalling methods

- (void)installDependencyForWebState:(web::WebState*)webState {
  if (MobileConfigTabHelper::FromWebState(webState)) {
    MobileConfigTabHelper::FromWebState(webState)->set_delegate(self);
  }
}

- (void)uninstallDependencyForWebState:(web::WebState*)webState {
  if (MobileConfigTabHelper::FromWebState(webState)) {
    MobileConfigTabHelper::FromWebState(webState)->set_delegate(nil);
  }
}

#pragma mark - MobileConfigTabHelperDelegate

- (void)presentMobileConfigAlertFromURL:(NSURL*)fileURL {
  if (!fileURL) {
    return;
  }

  base::UmaHistogramEnumeration(
      kUmaDownloadMobileConfigFileUI,
      DownloadMobileConfigFileUI::KWarningAlertIsPresented);

  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:
                               l10n_util::GetNSString(
                                   IDS_IOS_DOWNLOAD_MOBILECONFIG_FILE_WARNING_TITLE)
                         message:
                             l10n_util::GetNSString(
                                 IDS_IOS_DOWNLOAD_MOBILECONFIG_FILE_WARNING_MESSAGE)];

  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^{
                  base::UmaHistogramEnumeration(
                      kUmaDownloadMobileConfigFileUI,
                      DownloadMobileConfigFileUI::KWarningAlertIsDismissed);
                }
                 style:UIAlertActionStyleCancel];

  __weak MobileConfigCoordinator* weakSelf = self;
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_DOWNLOAD_MOBILECONFIG_CONTINUE)
                action:^{
                  [weakSelf presentSFSafariViewController:fileURL];
                }
                 style:UIAlertActionStyleDefault];

  [self.alertCoordinator start];
}

#pragma mark - SFSafariViewControllerDelegate

- (void)safariViewControllerDidFinish:(SFSafariViewController*)controller {
  [self.baseViewController dismissViewControllerAnimated:true completion:nil];
}

@end
