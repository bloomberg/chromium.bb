// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_personalization_coordinator.h"

#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_personalization_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ConsentBumpPersonalizationCoordinator ()

// The view controller, redefined as ConsentBumpPersonalizationViewController.
@property(nonatomic, strong)
    ConsentBumpPersonalizationViewController* personalizationViewController;

@end

@implementation ConsentBumpPersonalizationCoordinator

@synthesize personalizationViewController = _personalizationViewController;

#pragma mark - Properties

- (UIViewController*)viewController {
  return self.personalizationViewController;
}

- (ConsentBumpOptionType)selectedOption {
  return self.personalizationViewController.selectedOption;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.personalizationViewController =
      [[ConsentBumpPersonalizationViewController alloc] init];
}

- (void)stop {
  self.personalizationViewController = nil;
}

@end
