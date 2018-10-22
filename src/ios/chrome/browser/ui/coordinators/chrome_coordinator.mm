// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ChromeCoordinator

@synthesize childCoordinators = _childCoordinators;
@synthesize baseViewController = _baseViewController;
@synthesize navigationController = _navigationController;
@synthesize browserState = _browserState;

- (nullable instancetype)initWithBaseViewController:
    (UIViewController*)viewController {
  return [self initWithBaseViewController:viewController browserState:nullptr];
}

- (nullable instancetype)
initWithBaseViewController:(UIViewController*)viewController
              browserState:(ios::ChromeBrowserState*)browserState {
  if (self = [super init]) {
    _baseViewController = viewController;
    _childCoordinators = [MutableCoordinatorArray array];
    _browserState = browserState;
  }
  return self;
}

- (void)dealloc {
  [self stop];
}

#pragma mark - Accessors

- (ChromeCoordinator*)activeChildCoordinator {
  // By default the active child is the one most recently added to the child
  // array, but subclasses can override this.
  return self.childCoordinators.lastObject;
}

#pragma mark - Public

- (void)start {
  // Default implementation does nothing.
}

- (void)stop {
  // Default implementation does nothing.
}

@end
