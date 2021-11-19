// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_promo_coordinator.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_view_controller.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_string_util.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DefaultBrowserPromoCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>

// The fullscreen confirmation modal promo view controller this coordiantor
// manages.
@property(nonatomic, strong)
    DefaultBrowserPromoViewController* defaultBrowerPromoViewController;
// Popover used to show learn more info, not nil when presented.
@property(nonatomic, strong)
    PopoverLabelViewController* learnMoreViewController;

@end

@implementation DefaultBrowserPromoCoordinator

#pragma mark - Public Methods.

- (void)start {
  [super start];
  base::RecordAction(
      base::UserMetricsAction("IOS.DefaultBrowserFullscreenPromo.Impression"));
  self.defaultBrowerPromoViewController =
      [[DefaultBrowserPromoViewController alloc] init];
  self.defaultBrowerPromoViewController.actionHandler = self;
  self.defaultBrowerPromoViewController.modalPresentationStyle =
      UIModalPresentationFormSheet;
  self.defaultBrowerPromoViewController.presentationController.delegate = self;
  [self.baseViewController
      presentViewController:self.defaultBrowerPromoViewController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  // Ensure that presentationControllerDidDismiss: is not called in response to
  // a stop.
  self.defaultBrowerPromoViewController.presentationController.delegate = nil;
  [self.defaultBrowerPromoViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.defaultBrowerPromoViewController = nil;
  [super stop];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(
      base::UserMetricsAction("IOS.DefaultBrowserFullscreenPromo.Dismissed"));
  // This ensures that a modal swipe dismiss will also be logged.
  LogUserInteractionWithFullscreenPromo();
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  if (IsInRemindMeLaterGroup()) {
    if (self.defaultBrowerPromoViewController.tertiaryActionString) {
      [self logDefaultBrowserFullscreenPromoRemindMeHistogramForAction:
                ACTION_BUTTON];
    } else {
      [self logDefaultBrowserFullscreenRemindMeSecondPromoHistogramForAction:
                ACTION_BUTTON];
    }
  } else {
    [self logDefaultBrowserFullscreenPromoHistogramForAction:ACTION_BUTTON];
  }
  base::RecordAction(base::UserMetricsAction(
      "IOS.DefaultBrowserFullscreenPromo.PrimaryActionTapped"));
  LogUserInteractionWithFullscreenPromo();
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];

  [self.handler hidePromo];
}

- (void)confirmationAlertSecondaryAction {
  LogUserInteractionWithFullscreenPromo();
  if (IsInRemindMeLaterGroup()) {
    if (self.defaultBrowerPromoViewController.tertiaryActionString) {
      // When the "Remind Me Later" button is visible, it is the secondary
      // button, while the "No Thanks" button is the tertiary button.
      [self logDefaultBrowserFullscreenPromoRemindMeHistogramForAction:
                REMIND_ME_LATER];
      base::RecordAction(base::UserMetricsAction(
          "IOS.DefaultBrowserFullscreenPromo.RemindMeTapped"));
      LogRemindMeLaterPromoActionInteraction();
    } else {
      [self logDefaultBrowserFullscreenRemindMeSecondPromoHistogramForAction:
                CANCEL];
      base::RecordAction(base::UserMetricsAction(
          "IOS.DefaultBrowserFullscreenPromo.Dismissed"));
    }
  } else {
    [self logDefaultBrowserFullscreenPromoHistogramForAction:CANCEL];
    base::RecordAction(
        base::UserMetricsAction("IOS.DefaultBrowserFullscreenPromo.Dismissed"));
  }
  [self.handler hidePromo];
}

- (void)confirmationAlertTertiaryAction {
  DCHECK(IsInRemindMeLaterGroup());
  [self logDefaultBrowserFullscreenPromoRemindMeHistogramForAction:CANCEL];
  base::RecordAction(
      base::UserMetricsAction("IOS.DefaultBrowserFullscreenPromo.Dismissed"));
  LogUserInteractionWithFullscreenPromo();
  [self.handler hidePromo];
}

- (void)confirmationAlertLearnMoreAction {
  base::RecordAction(base::UserMetricsAction(
      "IOS.DefaultBrowserFullscreen.PromoMoreInfoTapped"));
  NSString* message = GetDefaultBrowserLearnMoreText();
  self.learnMoreViewController =
      [[PopoverLabelViewController alloc] initWithMessage:message];

  self.learnMoreViewController.popoverPresentationController.barButtonItem =
      self.defaultBrowerPromoViewController.helpButton;
  self.learnMoreViewController.popoverPresentationController
      .permittedArrowDirections = UIPopoverArrowDirectionUp;

  [self.defaultBrowerPromoViewController
      presentViewController:self.learnMoreViewController
                   animated:YES
                 completion:nil];
}

- (void)logDefaultBrowserFullscreenPromoHistogramForAction:
    (IOSDefaultBrowserFullscreenPromoAction)action {
  base::UmaHistogramEnumeration("IOS.DefaultBrowserFullscreenPromo", action);
}

- (void)logDefaultBrowserFullscreenPromoRemindMeHistogramForAction:
    (IOSDefaultBrowserFullscreenPromoAction)action {
  base::UmaHistogramEnumeration("IOS.DefaultBrowserFullscreenPromoRemindMe",
                                action);
}

- (void)logDefaultBrowserFullscreenRemindMeSecondPromoHistogramForAction:
    (IOSDefaultBrowserFullscreenPromoAction)action {
  base::UmaHistogramEnumeration(
      "IOS.DefaultBrowserFullscreenPromoRemindMeSecondPromo", action);
}

@end
