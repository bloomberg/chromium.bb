// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/bvc_container_view_controller.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BVCContainerViewController

#pragma mark - public property implementation

- (UIViewController*)currentBVC {
  return [self.childViewControllers firstObject];
}

- (void)setCurrentBVC:(UIViewController*)bvc {
  DCHECK(bvc);
  if (self.currentBVC == bvc) {
    return;
  }

  // Remove the current bvc, if any.
  if (self.currentBVC) {
    [self.currentBVC willMoveToParentViewController:nil];
    [self.currentBVC.view removeFromSuperview];
    [self.currentBVC removeFromParentViewController];
  }

  DCHECK_EQ(nil, self.currentBVC);
  DCHECK_EQ(0U, self.view.subviews.count);

  // Add the new active view controller.
  [self addChildViewController:bvc];
  bvc.view.frame = self.view.bounds;
  [self.view addSubview:bvc.view];
  [bvc didMoveToParentViewController:self];

  // Let the system know that the child has changed so appearance updates can
  // be made.
  [self setNeedsStatusBarAppearanceUpdate];

  DCHECK(self.currentBVC == bvc);
}

#pragma mark - UIViewController methods

- (void)presentViewController:(UIViewController*)viewControllerToPresent
                     animated:(BOOL)flag
                   completion:(void (^)())completion {
  // Force presentation to go through the current BVC, which does some
  // associated bookkeeping.
  DCHECK(self.currentBVC);
  [self.currentBVC presentViewController:viewControllerToPresent
                                animated:flag
                              completion:completion];
}

- (void)dismissViewControllerAnimated:(BOOL)flag
                           completion:(void (^)())completion {
  // Force dismissal to go through the current BVC, which does some associated
  // bookkeeping.
  DCHECK(self.currentBVC);
  [self.currentBVC dismissViewControllerAnimated:flag completion:completion];
}

- (UIViewController*)childViewControllerForStatusBarHidden {
  return self.currentBVC;
}

- (UIViewController*)childViewControllerForStatusBarStyle {
  return self.currentBVC;
}

- (BOOL)shouldAutorotate {
  return self.currentBVC ? [self.currentBVC shouldAutorotate]
                         : [super shouldAutorotate];
}

@end
