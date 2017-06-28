// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/host_setup_header_view.h"

#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "remoting/ios/app/remoting_theme.h"

static const CGFloat kSetupTitleInset = 22.f;
static const CGFloat kBottomPadding = 6.f;

// TODO(yuweih): i18n
static NSString* const kSetupTitle =
    @"Welcome to Chrome Remote Desktop.\n"
    @"Let's get you setup.";

@implementation HostSetupHeaderView

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    [self commonInit];
  }
  return self;
}

- (void)commonInit {
  self.backgroundColor = RemotingTheme.setupListBackgroundColor;

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = kSetupTitle;
  titleLabel.font = MDCTypography.titleFont;
  titleLabel.numberOfLines = 2;
  titleLabel.adjustsFontSizeToFitWidth = YES;
  titleLabel.textColor = RemotingTheme.setupListTextColor;
  [self addSubview:titleLabel];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [titleLabel.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                             constant:kSetupTitleInset],
    [titleLabel.trailingAnchor constraintEqualToAnchor:self.trailingAnchor
                                              constant:-kSetupTitleInset],
    [titleLabel.bottomAnchor constraintEqualToAnchor:self.bottomAnchor
                                            constant:-kBottomPadding],
  ]];
}

@end
