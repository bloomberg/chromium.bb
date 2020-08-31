// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/extended_touch_target_button.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "base/feature_list.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"

@implementation ExtendedTouchTargetButton

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
#if defined(__IPHONE_13_4)
    if (@available(iOS 13.4, *)) {
      if (base::FeatureList::IsEnabled(kPointerSupport)) {
        self.pointerInteractionEnabled = YES;
      }
    }
#endif  // defined(__IPHONE_13_4)
  }
  return self;
}

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  // Point is in |bounds| coordinates, but |center| is in the |superview|
  // coordinates. Compute center in |bounds| coords.
  CGPoint center =
      CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
  CGFloat distance = sqrt((center.x - point.x) * (center.x - point.x) +
                          ((center.y - point.y) * (center.y - point.y)));
  // The UI Guidelines recommend having at least 44pt tap target.
  if (distance < 22.0f) {
    return YES;
  }
  return [super pointInside:point withEvent:event];
}

@end
