// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_cell.h"

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_constants.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/common/favicon/favicon_view.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsMostVisitedCell : MDCCollectionViewCell

@synthesize faviconView = _faviconView;
@synthesize titleLabel = _titleLabel;

#pragma mark - Public

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.textColor = [UIColor colorWithWhite:0 alpha:kTitleAlpha];
    _titleLabel.font = [UIFont systemFontOfSize:12];
    _titleLabel.textAlignment = NSTextAlignmentCenter;
    _titleLabel.preferredMaxLayoutWidth = [[self class] defaultSize].width;
    _titleLabel.numberOfLines = kLabelNumLines;

    _faviconView = [[FaviconViewNew alloc] init];
    _faviconView.font = [UIFont systemFontOfSize:22];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;

    [self.contentView addSubview:_titleLabel];

    UIView* containerView = nil;
    UIImageView* faviconContainer =
        [[UIImageView alloc] initWithFrame:self.bounds];
    faviconContainer.translatesAutoresizingMaskIntoConstraints = NO;
    faviconContainer.image = [UIImage imageNamed:@"ntp_most_visited_tile"];
    [self.contentView addSubview:faviconContainer];
    [faviconContainer addSubview:_faviconView];

    [NSLayoutConstraint activateConstraints:@[
      [faviconContainer.widthAnchor constraintEqualToConstant:kIconSize],
      [faviconContainer.heightAnchor
          constraintEqualToAnchor:faviconContainer.widthAnchor],
      [faviconContainer.centerXAnchor
          constraintEqualToAnchor:_titleLabel.centerXAnchor],
      [_faviconView.heightAnchor constraintEqualToConstant:32],
      [_faviconView.widthAnchor
          constraintEqualToAnchor:_faviconView.heightAnchor],
    ]];
    AddSameCenterConstraints(_faviconView, faviconContainer);
    containerView = faviconContainer;

    ApplyVisualConstraintsWithMetrics(
        @[ @"V:|[container]-(space)-[title]", @"H:|[title]|" ],
        @{@"container" : containerView, @"title" : _titleLabel},
        @{ @"space" : @(kSpaceIconTitle) });

    self.isAccessibilityElement = YES;
  }
  return self;
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];

  [UIView transitionWithView:self
                    duration:ios::material::kDuration8
                     options:UIViewAnimationOptionCurveEaseInOut
                  animations:^{
                    self.alpha = highlighted ? 0.5 : 1.0;
                  }
                  completion:nil];
}

+ (CGSize)defaultSize {
  return kCellSize;
}

- (CGSize)intrinsicContentSize {
  return [[self class] defaultSize];
}

@end
