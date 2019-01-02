// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory_coordinator.h"

#include "base/mac/foundation_util.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_controller.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FormInputAccessoryCoordinator ()

// The View Controller for the input accessory view.
@property FormInputAccessoryViewController* formInputAccessoryViewController;

// The Mediator for the input accessory view controller.
@property FormInputAccessoryMediator* formInputAccessoryMediator;

@end

@implementation FormInputAccessoryCoordinator

@synthesize formInputAccessoryViewController =
    _formInputAccessoryViewController;
@synthesize formInputAccessoryMediator = _formInputAccessoryMediator;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                              webStateList:(WebStateList*)webStateList {
  self = [super initWithBaseViewController:viewController
                              browserState:browserState];
  if (self) {
    DCHECK(webStateList);
    _formInputAccessoryViewController =
        [[FormInputAccessoryViewController alloc] init];
    _formInputAccessoryMediator = [[FormInputAccessoryMediator alloc]
        initWithConsumer:self.formInputAccessoryViewController
            webStateList:webStateList];
  }
  return self;
}

@end
