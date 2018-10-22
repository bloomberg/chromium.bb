// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_transition_handler.h"

#import "ios/chrome/browser/ui/tab_grid/transitions/grid_to_visible_tab_animator.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_state_providing.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/reduced_motion_animator.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/tab_to_grid_animator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabGridTransitionHandler

@synthesize provider = _provider;

#pragma mark - UIViewControllerTransitioningDelegate

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForPresentedController:(UIViewController*)presented
                     presentingController:(UIViewController*)presenting
                         sourceController:(UIViewController*)source {
  id<UIViewControllerAnimatedTransitioning> animator;
  if (UIAccessibilityIsReduceMotionEnabled() ||
      !self.provider.selectedCellVisible) {
    ReducedMotionAnimator* simpleAnimator =
        [[ReducedMotionAnimator alloc] init];
    simpleAnimator.presenting = YES;
    animator = simpleAnimator;
  } else {
    animator =
        [[GridToVisibleTabAnimator alloc] initWithStateProvider:self.provider];
  }
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForDismissedController:(UIViewController*)dismissed {
  id<UIViewControllerAnimatedTransitioning> animator;
  if (UIAccessibilityIsReduceMotionEnabled()) {
    ReducedMotionAnimator* simpleAnimator =
        [[ReducedMotionAnimator alloc] init];
    simpleAnimator.presenting = NO;
    animator = simpleAnimator;
  } else {
    animator = [[TabToGridAnimator alloc] initWithStateProvider:self.provider];
  }
  return animator;
}

@end
