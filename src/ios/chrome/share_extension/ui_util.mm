// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "ios/chrome/share_extension/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ui_util {

const CGFloat kAnimationDuration = 0.3;

CGFloat AlignValueToPixel(CGFloat value) {
  CGFloat scale = [[UIScreen mainScreen] scale];
  return floor(value * scale) / scale;
}

void ConstrainAllSidesOfViewToView(UIView* container, UIView* filler) {
  [NSLayoutConstraint activateConstraints:@[
    [filler.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
    [filler.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
    [filler.topAnchor constraintEqualToAnchor:container.topAnchor],
    [filler.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
  ]];
}

}  // namespace ui_util
