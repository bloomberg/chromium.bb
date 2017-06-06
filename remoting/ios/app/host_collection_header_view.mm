// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/host_collection_header_view.h"

#import <UIKit/UIKit.h>

#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

// Applied on the left and right of the label.
static const float kTitleMargin = 12.f;

@interface HostCollectionHeaderView () {
 @private
  UILabel* _titleLabel;
}
@end

@implementation HostCollectionHeaderView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.font = [MDCTypography body2Font];
    _titleLabel.textColor = [UIColor whiteColor];
    _titleLabel.backgroundColor = [UIColor clearColor];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_titleLabel];

    [NSLayoutConstraint activateConstraints:@[
      [[_titleLabel leadingAnchor] constraintEqualToAnchor:[self leadingAnchor]
                                                  constant:kTitleMargin],
      [[_titleLabel centerYAnchor]
          constraintEqualToAnchor:[self centerYAnchor]],
      [[_titleLabel trailingAnchor]
          constraintEqualToAnchor:[self trailingAnchor]
                         constant:-kTitleMargin],
    ]];
  }
  return self;
}

- (NSString*)text {
  return _titleLabel.text;
}

- (void)setText:(NSString*)text {
  _titleLabel.text = text;
}

@end
