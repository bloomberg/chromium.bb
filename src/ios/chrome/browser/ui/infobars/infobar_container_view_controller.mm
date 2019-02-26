// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_container_view_controller.h"

#include "base/ios/block_types.h"
#include "base/logging.h"
#import "ios/chrome/browser/ui/infobars/infobar_positioner.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Duration for the alpha change animation.
const CGFloat kAlphaChangeAnimationDuration = 0.35;
}  // namespace

@interface InfobarContainerViewController ()

// Whether the controller's view is currently available.
// YES from viewDidAppear to viewDidDisappear.
@property(nonatomic, assign, getter=isVisible) BOOL visible;

@end

@implementation InfobarContainerViewController

// Whenever the container or contained views are re-drawn update the layout to
// match their new size or position.
- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updateLayoutAnimated:YES];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  self.visible = YES;
}

- (void)viewDidDisappear:(BOOL)animated {
  self.visible = NO;
  [super viewDidDisappear:animated];
}

#pragma mark - InfobarConsumer

- (void)addInfoBarView:(UIView*)infoBarView position:(NSInteger)position {
  DCHECK_LE(static_cast<NSUInteger>(position), [[self.view subviews] count]);
  [self.view insertSubview:infoBarView atIndex:position];
  infoBarView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [infoBarView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [infoBarView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [infoBarView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [infoBarView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor]
  ]];
}

- (void)setUserInteractionEnabled:(BOOL)enabled {
  [self.view setUserInteractionEnabled:enabled];
}

- (void)updateLayoutAnimated:(BOOL)animated {
  // Update the infobarContainer height.
  CGRect containerFrame = self.view.frame;
  CGFloat height = [self topmostVisibleInfoBarHeight];
  containerFrame.origin.y =
      CGRectGetMaxY([self.positioner parentView].frame) - height;
  containerFrame.size.height = height;

  auto completion = ^(BOOL finished) {
    if (!self.visible)
      return;
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                    self.view);
  };

  ProceduralBlock frameUpdates = ^{
    [self.view setFrame:containerFrame];
  };
  if (animated) {
    [UIView animateWithDuration:0.1
                     animations:frameUpdates
                     completion:completion];
  } else {
    frameUpdates();
    completion(YES);
  }
}

#pragma mark - Private Methods

// Animates |self.view| alpha to |alpha|.
- (void)animateInfoBarContainerToAlpha:(CGFloat)alpha {
  CGFloat oldAlpha = self.view.alpha;
  if (oldAlpha > 0 && alpha == 0) {
    [self.view setUserInteractionEnabled:NO];
  }

  [UIView transitionWithView:self.view
      duration:kAlphaChangeAnimationDuration
      options:UIViewAnimationOptionCurveEaseInOut
      animations:^{
        [self.view setAlpha:alpha];
      }
      completion:^(BOOL) {
        if (oldAlpha == 0 && alpha > 0) {
          [self.view setUserInteractionEnabled:YES];
        };
      }];
}

// Height of the frontmost infobar contained in |self.view| that is not hidden.
- (CGFloat)topmostVisibleInfoBarHeight {
  for (UIView* view in [self.view.subviews reverseObjectEnumerator]) {
    return [view sizeThatFits:self.view.frame.size].height;
  }
  return 0;
}

@end
