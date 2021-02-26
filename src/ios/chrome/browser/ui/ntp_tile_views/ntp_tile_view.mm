// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp_tile_views/ntp_tile_view.h"

#include "base/feature_list.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/dynamic_type_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/common/ui/util/dynamic_type_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const NSInteger kLabelNumLines = 2;
const CGFloat kSpaceIconTitle = 10;
const CGFloat kIconSize = 56;
const CGFloat kPreferredMaxWidth = 73;

}  // namespace

@implementation NTPTileView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.textColor = UIColor.cr_secondaryLabelColor;
    _titleLabel.font = [self titleLabelFont];
    _titleLabel.textAlignment = NSTextAlignmentCenter;
    _titleLabel.preferredMaxLayoutWidth = kPreferredMaxWidth;
    _titleLabel.numberOfLines = kLabelNumLines;

    _imageContainerView = [[UIView alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _imageContainerView.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:_titleLabel];

    // The squircle background view.
    UIImageView* backgroundView =
        [[UIImageView alloc] initWithFrame:self.bounds];
    backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    UIImage* backgroundImage = [[UIImage imageNamed:@"ntp_most_visited_tile"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    backgroundView.image = backgroundImage;
    backgroundView.tintColor = [UIColor colorNamed:kGrey100Color];
    [self addSubview:backgroundView];
    [self addSubview:_imageContainerView];

    [NSLayoutConstraint activateConstraints:@[
      [backgroundView.widthAnchor constraintEqualToConstant:kIconSize],
      [backgroundView.heightAnchor
          constraintEqualToAnchor:backgroundView.widthAnchor],
      [backgroundView.centerXAnchor
          constraintEqualToAnchor:_titleLabel.centerXAnchor],
    ]];
    AddSameCenterConstraints(_imageContainerView, backgroundView);
    UIView* containerView = backgroundView;

    ApplyVisualConstraintsWithMetrics(
        @[ @"V:|[container]-(space)-[title]", @"H:|[title]|" ],
        @{@"container" : containerView, @"title" : _titleLabel},
        @{ @"space" : @(kSpaceIconTitle) });

    _imageBackgroundView = backgroundView;

#ifdef __IPHONE_13_4
    if (@available(iOS 13.4, *)) {
      if (base::FeatureList::IsEnabled(kPointerSupport)) {
        [self addInteraction:[[UIPointerInteraction alloc]
                                 initWithDelegate:self]];
      }
    }
#endif
  }
  return self;
}

// Returns the font size for the location label.
- (UIFont*)titleLabelFont {
  return PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleCaption1,
      self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryAccessibilityLarge);
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    self.titleLabel.font = [self titleLabelFont];
  }
}

#pragma mark - UIPointerInteractionDelegate

#ifdef __IPHONE_13_4

- (UIPointerRegion*)pointerInteraction:(UIPointerInteraction*)interaction
                      regionForRequest:(UIPointerRegionRequest*)request
                         defaultRegion:(UIPointerRegion*)defaultRegion
    API_AVAILABLE(ios(13.4)) {
  return defaultRegion;
}

- (UIPointerStyle*)pointerInteraction:(UIPointerInteraction*)interaction
                       styleForRegion:(UIPointerRegion*)region
    API_AVAILABLE(ios(13.4)) {
  UITargetedPreview* preview =
      [[UITargetedPreview alloc] initWithView:_imageContainerView];
  UIPointerHighlightEffect* effect =
      [UIPointerHighlightEffect effectWithPreview:preview];
  UIPointerShape* shape =
      [UIPointerShape shapeWithRoundedRect:_imageContainerView.frame
                              cornerRadius:8.0];
  return [UIPointerStyle styleWithEffect:effect shape:shape];
}

#endif

@end
