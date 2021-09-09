// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/activity_overlay_view.h"

#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ActivityOverlayView

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _indicator = GetLargeUIActivityIndicatorView();
    _indicator.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return self;
}

- (void)didMoveToSuperview {
  if (self.subviews.count == 0) {
    // This is the first time the view is used, finish setting everything up.
    self.backgroundColor = [UIColor colorNamed:kScrimBackgroundColor];
    [self addSubview:_indicator];
    AddSameCenterConstraints(self, _indicator);
    [_indicator startAnimating];
  }
  [super didMoveToSuperview];
}

@end
