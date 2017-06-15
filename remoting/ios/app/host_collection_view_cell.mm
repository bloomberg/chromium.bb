// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <UIKit/UIKit.h>

#import "remoting/ios/app/host_collection_view_cell.h"

#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/domain/host_info.h"

static const CGFloat kLinePadding = 2.f;
static const CGFloat kHostCardIconInset = 10.f;
static const CGFloat kHostCardPadding = 4.f;
static const CGFloat kHostCardIconSize = 45.f;

@interface HostCollectionViewCell () {
  UIImageView* _imageView;
  UILabel* _statusLabel;
  UILabel* _titleLabel;
  UIView* _labelView;
}
@end

//
// This is the implementation of the info card for a host's status shown in
// the host list. This will also be the selection for which host to connect
// to and other managements actions for a host in this list.
//
@implementation HostCollectionViewCell

@synthesize hostInfo = _hostInfo;

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor clearColor];
    [self commonInit];
  }
  return self;
}

- (void)commonInit {
  _imageView = [[UIImageView alloc] init];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  _imageView.contentMode = UIViewContentModeCenter;
  _imageView.alpha = 0.87f;
  _imageView.backgroundColor = RemotingTheme.offlineHostColor;
  _imageView.layer.cornerRadius = kHostCardIconSize / 2.f;
  _imageView.layer.masksToBounds = YES;
  [self.contentView addSubview:_imageView];

  // Holds both of the labels.
  _labelView = [[UIView alloc] init];
  _labelView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:_labelView];

  _titleLabel = [[UILabel alloc] init];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _titleLabel.font = [MDCTypography titleFont];
  _titleLabel.alpha = [MDCTypography titleFontOpacity];
  _titleLabel.textColor = [UIColor colorWithWhite:0 alpha:0.87f];
  [_labelView addSubview:_titleLabel];

  _statusLabel = [[UILabel alloc] init];
  _statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _statusLabel.font = [MDCTypography captionFont];
  _statusLabel.alpha = [MDCTypography captionFontOpacity];
  _statusLabel.textColor = [UIColor colorWithWhite:0 alpha:0.60f];
  [_labelView addSubview:_statusLabel];

  // Constraints
  NSArray* constraints = @[
    // +------------+---------------+
    // | +--------+ |               |
    // | |        | | [Host Name]   |
    // | |  Icon  | | - - - - - - - | <- Center Y
    // | |        | | [Host Status] |
    // | +---^----+ |               |
    // +-----|------+-------^-------+
    //       |              |
    //  Image View     Label View
    [[_imageView leadingAnchor]
        constraintEqualToAnchor:[self.contentView leadingAnchor]
                       constant:kHostCardIconInset],
    [[_imageView centerYAnchor]
        constraintEqualToAnchor:[self.contentView centerYAnchor]],
    [[_imageView widthAnchor] constraintEqualToConstant:kHostCardIconSize],
    [[_imageView heightAnchor] constraintEqualToConstant:kHostCardIconSize],

    [[_labelView leadingAnchor]
        constraintEqualToAnchor:[_imageView trailingAnchor]
                       constant:kHostCardIconInset],
    [[_labelView trailingAnchor]
        constraintEqualToAnchor:[self.contentView trailingAnchor]
                       constant:-kHostCardPadding / 2.f],
    [[_labelView topAnchor]
        constraintEqualToAnchor:[self.contentView topAnchor]],
    [[_labelView bottomAnchor]
        constraintEqualToAnchor:[self.contentView bottomAnchor]],

    // Put titleLable and statusLable symmetrically around centerY.
    [[_titleLabel leadingAnchor]
        constraintEqualToAnchor:[_labelView leadingAnchor]],
    [[_titleLabel trailingAnchor]
        constraintEqualToAnchor:[_labelView trailingAnchor]],
    [[_titleLabel bottomAnchor]
        constraintEqualToAnchor:[_labelView centerYAnchor]],

    [[_statusLabel leadingAnchor]
        constraintEqualToAnchor:[_labelView leadingAnchor]],
    [[_statusLabel trailingAnchor]
        constraintEqualToAnchor:[_labelView trailingAnchor]],
    [[_statusLabel topAnchor] constraintEqualToAnchor:[_labelView centerYAnchor]
                                             constant:kLinePadding],
  ];

  [NSLayoutConstraint activateConstraints:constraints];
}

#pragma mark - HostCollectionViewCell Public

- (void)populateContentWithHostInfo:(HostInfo*)hostInfo {
  _hostInfo = hostInfo;

  _titleLabel.text = _hostInfo.hostName;

  _imageView.image = RemotingTheme.desktopIcon;

  if ([_hostInfo.status isEqualToString:@"ONLINE"]) {
    _imageView.backgroundColor = RemotingTheme.onlineHostColor;
    _statusLabel.text = @"Online";
  } else {
    _imageView.backgroundColor = RemotingTheme.offlineHostColor;
    _statusLabel.text =
        [NSString stringWithFormat:@"Last online: %@", hostInfo.updatedTime];
  }
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  _hostInfo = nil;
  _statusLabel.text = nil;
  _titleLabel.text = nil;
}

@end
