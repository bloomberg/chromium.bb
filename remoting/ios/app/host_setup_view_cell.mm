// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/host_setup_view_cell.h"

#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/app/view_utils.h"

static const CGFloat kNumberIconPadding = 16.f;
static const CGFloat kNumberIconSize = 45.f;
static const CGFloat kCellPadding = 22.f;

@interface HostSetupViewCell () {
  UIView* _numberContainerView;
  UILabel* _numberLabel;
  UILabel* _contentLabel;
}
@end

@implementation HostSetupViewCell

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    [self commonInit];
  }
  return self;
}

- (void)commonInit {
  self.isAccessibilityElement = YES;
  self.backgroundColor = RemotingTheme.setupListBackgroundColor;

  _numberContainerView = [[UIView alloc] init];
  _numberLabel = [[UILabel alloc] init];
  _contentLabel = [[UILabel alloc] init];

  _numberContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  _numberLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _contentLabel.translatesAutoresizingMaskIntoConstraints = NO;

  _contentLabel.lineBreakMode = NSLineBreakByWordWrapping;
  _contentLabel.numberOfLines = 0;

  _numberContainerView.backgroundColor = RemotingTheme.hostOnlineColor;
  _numberLabel.textColor = RemotingTheme.setupListNumberColor;
  _contentLabel.textColor = RemotingTheme.setupListTextColor;
  _numberLabel.font = MDCTypography.titleFont;
  _contentLabel.font = MDCTypography.subheadFont;
  _numberContainerView.layer.cornerRadius = kNumberIconSize / 2.f;

  [self.contentView addSubview:_numberContainerView];
  [self.contentView addSubview:_contentLabel];
  [_numberContainerView addSubview:_numberLabel];

  UILayoutGuide* safeAreaLayoutGuide =
      remoting::SafeAreaLayoutGuideForView(self.contentView);

  NSArray* constraints = @[
    [_numberContainerView.leadingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor
                       constant:kCellPadding],
    [_numberContainerView.centerYAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.centerYAnchor],
    [_numberContainerView.widthAnchor
        constraintEqualToConstant:kNumberIconSize],
    [_numberContainerView.heightAnchor
        constraintEqualToConstant:kNumberIconSize],

    [_numberLabel.centerXAnchor
        constraintEqualToAnchor:_numberContainerView.centerXAnchor],
    [_numberLabel.centerYAnchor
        constraintEqualToAnchor:_numberContainerView.centerYAnchor],

    [_contentLabel.leadingAnchor
        constraintEqualToAnchor:_numberContainerView.trailingAnchor
                       constant:kNumberIconPadding],
    [_contentLabel.trailingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.trailingAnchor
                       constant:-kCellPadding],
    [_contentLabel.centerYAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.centerYAnchor],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

- (void)setContentText:(NSString*)text number:(NSInteger)number {
  self.accessibilityLabel = text;
  _contentLabel.text = text;
  _numberLabel.text = [@(number) stringValue];
}

@end
