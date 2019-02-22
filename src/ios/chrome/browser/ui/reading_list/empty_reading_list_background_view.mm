// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/empty_reading_list_background_view.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/reading_list/empty_reading_list_message_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Images name.
NSString* const kEmptyReadingListBackgroundIcon = @"reading_list_empty_state";

// Background view constants.
const CGFloat kImageHeight = 44;
const CGFloat kImageWidth = 60;
const CGFloat kPercentageFromTopForPosition = 0.4;
const CGFloat kTextHorizontalMinimumMargin = 32;
const CGFloat kTextImageSpacing = 10;
const CGFloat kTextMaximalWidth = 255;
}  // namespace

@interface EmptyReadingListBackgroundView ()

// Sets the constraints for this view, positionning the |imageView| in the X
// center and at 40% of the top. The |label| is positionned below.
- (void)setConstraintsToImageView:(UIImageView*)imageView
                         andLabel:(UILabel*)label;

@end

@implementation EmptyReadingListBackgroundView

#pragma mark - Public

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {

    // Attach to the label.
    UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
    label.attributedText = GetReadingListEmptyMessage();
    label.numberOfLines = 0;
    label.textAlignment = NSTextAlignmentCenter;
    label.accessibilityLabel = GetReadingListEmptyMessageA11yLabel();
    label.accessibilityIdentifier =
        [EmptyReadingListBackgroundView accessibilityIdentifier];
    [label setTranslatesAutoresizingMaskIntoConstraints:NO];
    [self addSubview:label];

    UIImageView* imageView = [[UIImageView alloc] init];
    imageView.image = [UIImage imageNamed:kEmptyReadingListBackgroundIcon];
    [imageView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [self addSubview:imageView];

    [self setConstraintsToImageView:imageView andLabel:label];
  }
  return self;
}

+ (NSString*)accessibilityIdentifier {
  return @"ReadingListBackgroundViewIdentifier";
}

#pragma mark - Private

- (void)setConstraintsToImageView:(UIImageView*)imageView
                         andLabel:(UILabel*)label {
  [NSLayoutConstraint activateConstraints:@[
    [imageView.heightAnchor constraintEqualToConstant:kImageHeight],
    [imageView.widthAnchor constraintEqualToConstant:kImageWidth],
    [self.centerXAnchor constraintEqualToAnchor:label.centerXAnchor],
    [self.centerXAnchor constraintEqualToAnchor:imageView.centerXAnchor],
    [label.topAnchor constraintEqualToAnchor:imageView.bottomAnchor
                                    constant:kTextImageSpacing],
    [label.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.trailingAnchor
                                 constant:-kTextHorizontalMinimumMargin],
    [label.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.leadingAnchor
                                    constant:kTextHorizontalMinimumMargin]
  ]];

  NSLayoutConstraint* widthConstraint =
      [label.widthAnchor constraintEqualToConstant:kTextMaximalWidth];
  widthConstraint.priority = UILayoutPriorityDefaultHigh;
  widthConstraint.active = YES;

  // Position the top of the image at 40% from the top.
  NSLayoutConstraint* verticalAlignment =
      [NSLayoutConstraint constraintWithItem:imageView
                                   attribute:NSLayoutAttributeTop
                                   relatedBy:NSLayoutRelationEqual
                                      toItem:self
                                   attribute:NSLayoutAttributeBottom
                                  multiplier:kPercentageFromTopForPosition
                                    constant:0];
  [self addConstraints:@[ verticalAlignment ]];
}

@end
