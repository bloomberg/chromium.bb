// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/thumb_strip_plus_sign_button.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ThumbStripPlusSignButton ()
// The transparency gradient of the button.
@property(nonatomic, strong) CAGradientLayer* gradient;
@end

@implementation ThumbStripPlusSignButton

- (void)didMoveToSuperview {
  [super didMoveToSuperview];

  if (self.subviews.count > 0)
    return;

  UIImageView* plusSignImage = [[UIImageView alloc]
      initWithImage:[UIImage imageNamed:@"grid_cell_plus_sign"]];
  self.plusSignImage = plusSignImage;
  plusSignImage.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:plusSignImage];

  NSArray* constraints = @[
    [plusSignImage.centerXAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kPlusSignImageTrailingCenterDistance],
    [plusSignImage.centerYAnchor
        constraintEqualToAnchor:self.topAnchor
                       constant:kPlusSignImageYCenterConstant],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];

  if (self.gradient)
    [self.gradient removeFromSuperlayer];

  CAGradientLayer* gradient = [CAGradientLayer layer];
  self.gradient = gradient;
  gradient.frame = self.bounds;
  gradient.colors =
      @[ (id)[UIColor clearColor].CGColor, (id)[UIColor blackColor].CGColor ];
  gradient.startPoint = CGPointMake(0.0, 0.5);
  gradient.endPoint = CGPointMake(1.0, 0.5);
  gradient.locations = @[ @0, @0.5 ];
  [self.layer insertSublayer:gradient atIndex:0];
}

@end
