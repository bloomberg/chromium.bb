// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_container_view.h"

#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/util/animation_util.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/material_timing.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Size of the leading image view.
const CGFloat kLeadingImageSize = 30;
// Offset from the leading edge to the image view (used when the image is
// shown).
const CGFloat kleadingImageViewEdgeOffset = 7;
// Offset from the leading edge to the textfield when no image is shown.
const CGFloat kTextFieldLeadingOffsetNoImage = 16;
// Space between the leading button and the textfield when a button is shown.
const CGFloat kTextFieldLeadingOffsetImage = 14;
// Space between the clear button and the edge of the omnibox.
const CGFloat kTextFieldClearButtonTrailingOffset = 4;

}  // namespace

#pragma mark - OmniboxContainerView

@interface OmniboxContainerView ()
// Constraints the leading textfield side to the leading of |self|.
// Active when the |leadingView| is nil or hidden.
@property(nonatomic, strong) NSLayoutConstraint* leadingTextfieldConstraint;
// The leading image view. Used for autocomplete icons.
@property(nonatomic, strong) UIImageView* leadingImageView;
// Redefined as readwrite.
@property(nonatomic, strong) OmniboxTextFieldIOS* textField;

@end

@implementation OmniboxContainerView
@synthesize textField = _textField;
@synthesize leadingImageView = _leadingImageView;
@synthesize leadingTextfieldConstraint = _leadingTextfieldConstraint;
@synthesize incognito = _incognito;

#pragma mark - Public methods

- (instancetype)initWithFrame:(CGRect)frame
                    textColor:(UIColor*)textColor
                textFieldTint:(UIColor*)textFieldTint
                     iconTint:(UIColor*)iconTint {
  self = [super initWithFrame:frame];
  if (self) {
    _textField = [[OmniboxTextFieldIOS alloc] initWithFrame:frame
                                                  textColor:textColor
                                                  tintColor:textFieldTint];
    [self addSubview:_textField];

    _leadingTextfieldConstraint = [_textField.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kTextFieldLeadingOffsetNoImage];

    [NSLayoutConstraint activateConstraints:@[
      [_textField.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kTextFieldClearButtonTrailingOffset],
      [_textField.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_textField.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      _leadingTextfieldConstraint,
    ]];

    _textField.translatesAutoresizingMaskIntoConstraints = NO;
    [_textField
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];

    [self setupLeadingImageViewWithTint:iconTint];
  }
  return self;
}

- (void)attachLayoutGuides {
  [NamedGuide guideWithName:kOmniboxTextFieldGuide view:self].constrainedView =
      self.textField;

  // The leading image view can be not present, in which case the guide
  // shouldn't be attached.
  if (self.leadingImageView.superview) {
    [NamedGuide guideWithName:kOmniboxLeadingImageGuide view:self]
        .constrainedView = self.leadingImageView;
  }
}

- (void)setLeadingImage:(UIImage*)image {
  [self.leadingImageView setImage:image];
}

- (void)setIncognito:(BOOL)incognito {
  _incognito = incognito;
  self.textField.incognito = incognito;
}

- (void)setLeadingImageAlpha:(CGFloat)alpha {
  self.leadingImageView.alpha = alpha;
}

#pragma mark - private

- (void)setupLeadingImageViewWithTint:(UIColor*)iconTint {
  _leadingImageView = [[UIImageView alloc] init];
  _leadingImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _leadingImageView.contentMode = UIViewContentModeCenter;

  // The image view is always shown. Its width should be constant.
  [NSLayoutConstraint activateConstraints:@[
    [_leadingImageView.widthAnchor constraintEqualToConstant:kLeadingImageSize],
    [_leadingImageView.heightAnchor
        constraintEqualToAnchor:_leadingImageView.widthAnchor],
  ]];

  _leadingImageView.tintColor = iconTint;
  [self addSubview:_leadingImageView];
  self.leadingTextfieldConstraint.active = NO;

  NSLayoutConstraint* leadingImageViewToTextField =
      [self.leadingImageView.trailingAnchor
          constraintEqualToAnchor:self.textField.leadingAnchor
                         constant:-kTextFieldLeadingOffsetImage];

  [NSLayoutConstraint activateConstraints:@[
    [_leadingImageView.centerYAnchor
        constraintEqualToAnchor:self.centerYAnchor],
    [self.leadingAnchor
        constraintEqualToAnchor:self.leadingImageView.leadingAnchor
                       constant:-kleadingImageViewEdgeOffset],
    leadingImageViewToTextField,
  ]];
}

@end
