// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/host_setup_header_view.h"

#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "remoting/ios/app/remoting_theme.h"

#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"

static const CGFloat kSetupTitleInset = 22.f;
static const CGFloat kBottomPadding = 6.f;

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
  titleLabel.text = l10n_util::GetNSString(IDS_HOST_SETUP_TITLE);
  titleLabel.font = MDCTypography.titleFont;
  titleLabel.numberOfLines = 1;
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
