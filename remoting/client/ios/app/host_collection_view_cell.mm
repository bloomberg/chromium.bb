// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <UIKit/UIKit.h>

#import "remoting/client/ios/app/host_collection_view_cell.h"

#import "ios/third_party/material_components_ios/src/components/ShadowElevations/src/MaterialShadowElevations.h"
#import "ios/third_party/material_components_ios/src/components/ShadowLayer/src/MaterialShadowLayer.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

static CGFloat kLinePadding = 2.f;
static const CGFloat kHostCardIconInset = 10.f;
static const CGFloat kHostCardPadding = 4.f;
static const CGFloat kHostCardIconSize = 45.f;

@interface HostCollectionViewCell ()

@property(nonatomic) UIImageView* imageView;
@property(nonatomic) UILabel* titleLabel;
@property(nonatomic) UILabel* statusLabel;
@property(nonatomic) UIView* cellView;

@end

//
// This is the implementation of the info card for a host's status shown in
// the host list. This will also be the selection for which host to connect
// to and other managements actions for a host in this list.
//
@implementation HostCollectionViewCell

@synthesize title = _title;
@synthesize status = _status;
@synthesize imageView = _imageView;
@synthesize titleLabel = _titleLabel;
@synthesize statusLabel = _statusLabel;
@synthesize cellView = _cellView;

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor clearColor];
    [self commonInit];
  }
  return self;
}

- (void)commonInit {
  _cellView = [[UIView alloc] initWithFrame:self.bounds];
  _cellView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  _cellView.backgroundColor = [UIColor whiteColor];
  _cellView.clipsToBounds = YES;
  [self addSubview:_cellView];

  MDCShadowLayer* shadowLayer = (MDCShadowLayer*)self.layer;
  shadowLayer.shadowMaskEnabled = NO;
  [shadowLayer setElevation:MDCShadowElevationCardResting];

  CGRect imageViewFrame =
      CGRectMake(kHostCardIconInset,
                 self.frame.size.height / 2.f - kHostCardIconSize / 2.f,
                 kHostCardIconSize, kHostCardIconSize);
  _imageView = [[UIImageView alloc] initWithFrame:imageViewFrame];
  _imageView.contentMode = UIViewContentModeCenter;
  _imageView.alpha = 0.87f;
  _imageView.backgroundColor = UIColor.lightGrayColor;
  _imageView.layer.cornerRadius = kHostCardIconSize / 2.f;
  _imageView.layer.masksToBounds = YES;
  [_cellView addSubview:_imageView];

  _titleLabel = [[UILabel alloc] init];
  _titleLabel.font = [MDCTypography titleFont];
  _titleLabel.alpha = [MDCTypography titleFontOpacity];
  _titleLabel.textColor = [UIColor colorWithWhite:0 alpha:0.87f];
  _titleLabel.frame = CGRectMake(
      imageViewFrame.origin.x + imageViewFrame.size.width + kHostCardIconInset,
      (self.frame.size.height / 2.f) -
          (_titleLabel.font.pointSize + kHostCardPadding / 2.f),
      self.frame.size.width, _titleLabel.font.pointSize + kLinePadding);
  [_cellView addSubview:_titleLabel];

  _statusLabel = [[UILabel alloc] init];
  _statusLabel.font = [MDCTypography captionFont];
  _statusLabel.alpha = [MDCTypography captionFontOpacity];
  _statusLabel.textColor = [UIColor colorWithWhite:0 alpha:0.60f];
  _statusLabel.frame = CGRectMake(
      imageViewFrame.origin.x + imageViewFrame.size.width + kHostCardIconInset,
      (self.frame.size.height / 2.f) + kHostCardPadding / 2.f,
      self.frame.size.width, _statusLabel.font.pointSize + kLinePadding);
  [_cellView addSubview:_statusLabel];
}

- (void)populateContentWithTitle:(NSString*)title status:(NSString*)status {
  self.title = title;
  self.titleLabel.text = title;

  self.status = status;
  self.statusLabel.text = status;

  self.imageView.image = [UIImage imageNamed:@"ic_desktop"];

  // TODO(nicholss): These colors are incorrect for the final product.
  // Need to update to the values in the mocks.
  if ([status isEqualToString:@"ONLINE"]) {
    _imageView.backgroundColor = UIColor.greenColor;
  } else {
    _imageView.backgroundColor = UIColor.lightGrayColor;
  }
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.title = nil;
  self.status = nil;
}

+ (Class)layerClass {
  return [MDCShadowLayer class];
}

@end
