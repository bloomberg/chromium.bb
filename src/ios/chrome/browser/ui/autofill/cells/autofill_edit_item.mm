// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"

#import "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;

// Minimum gap between the label and the text field.
const CGFloat kLabelAndFieldGap = 5;
}  // namespace

@interface AutofillEditCell ()
// Updates the cell's fonts and colors for the given |cellStyle|.
- (void)updateForStyle:(CollectionViewCellStyle)cellStyle;
@end

@implementation AutofillEditItem

@synthesize cellStyle = _cellStyle;
@synthesize textFieldName = _textFieldName;
@synthesize textFieldValue = _textFieldValue;
@synthesize identifyingIcon = _identifyingIcon;
@synthesize inputView = _inputView;
@synthesize textFieldEnabled = _textFieldEnabled;
@synthesize autofillUIType = _autofillUIType;
@synthesize required = _required;
@synthesize returnKeyType = _returnKeyType;
@synthesize keyboardType = _keyboardType;
@synthesize autoCapitalizationType = _autoCapitalizationType;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [AutofillEditCell class];
    _cellStyle = CollectionViewCellStyle::kMaterial;
    _returnKeyType = UIReturnKeyNext;
    _keyboardType = UIKeyboardTypeDefault;
    _autoCapitalizationType = UITextAutocapitalizationTypeWords;
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureCell:(AutofillEditCell*)cell {
  [super configureCell:cell];

  // Update fonts and colors before changing anything else.
  [cell updateForStyle:self.cellStyle];

  NSString* textLabelFormat = self.required ? @"%@*" : @"%@";
  cell.textLabel.text =
      [NSString stringWithFormat:textLabelFormat, self.textFieldName];
  cell.textField.text = self.textFieldValue;
  if (self.textFieldName.length) {
    cell.textField.accessibilityIdentifier =
        [NSString stringWithFormat:@"%@_textField", self.textFieldName];
  }
  cell.textField.enabled = self.textFieldEnabled;
  cell.textField.textColor = self.textFieldEnabled
                                 ? [[MDCPalette cr_bluePalette] tint500]
                                 : [[MDCPalette greyPalette] tint500];
  [cell.textField addTarget:self
                     action:@selector(textFieldChanged:)
           forControlEvents:UIControlEventEditingChanged];
  cell.textField.inputView = self.inputView;
  cell.textField.returnKeyType = self.returnKeyType;
  cell.textField.keyboardType = self.keyboardType;
  cell.textField.autocapitalizationType = self.autoCapitalizationType;
  cell.identifyingIconView.image = self.identifyingIcon;
}

#pragma mark - Actions

- (void)textFieldChanged:(UITextField*)textField {
  self.textFieldValue = textField.text;
}

@end

@implementation AutofillEditCell {
  NSLayoutConstraint* _iconHeightConstraint;
  NSLayoutConstraint* _iconWidthConstraint;
  NSLayoutConstraint* _textFieldTrailingConstraint;
}

@synthesize textField = _textField;
@synthesize textLabel = _textLabel;
@synthesize identifyingIconView = _identifyingIconView;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;
    self.allowsCellInteractionsWhileEditing = YES;
    UIView* contentView = self.contentView;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [_textLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                  forAxis:UILayoutConstraintAxisHorizontal];
    [contentView addSubview:_textLabel];

    _textField = [[UITextField alloc] init];
    _textField.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_textField];

    _textField.autocorrectionType = UITextAutocorrectionTypeNo;
    _textField.clearButtonMode = UITextFieldViewModeWhileEditing;
    _textField.contentVerticalAlignment =
        UIControlContentVerticalAlignmentCenter;
    _textField.textAlignment =
        UseRTLLayout() ? NSTextAlignmentLeft : NSTextAlignmentRight;

    // Card type icon.
    _identifyingIconView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _identifyingIconView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_identifyingIconView];

    // Set up the icons size constraints. They are activated here and updated in
    // layoutSubviews.
    _iconHeightConstraint =
        [_identifyingIconView.heightAnchor constraintEqualToConstant:0];
    _iconWidthConstraint =
        [_identifyingIconView.widthAnchor constraintEqualToConstant:0];

    _textFieldTrailingConstraint = [_textField.trailingAnchor
        constraintEqualToAnchor:_identifyingIconView.leadingAnchor];

    // Set up the constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kHorizontalPadding],
      _textFieldTrailingConstraint,
      [_textField.firstBaselineAnchor
          constraintEqualToAnchor:_textLabel.firstBaselineAnchor],
      [_textField.leadingAnchor
          constraintEqualToAnchor:_textLabel.trailingAnchor
                         constant:kLabelAndFieldGap],
      [_identifyingIconView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalPadding],
      [_identifyingIconView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      _iconHeightConstraint,
      _iconWidthConstraint,
    ]];
    AddOptionalVerticalPadding(contentView, _textLabel, kVerticalPadding);
  }
  return self;
}

- (void)updateForStyle:(CollectionViewCellStyle)cellStyle {
  if (cellStyle == CollectionViewCellStyle::kUIKit &&
      experimental_flags::IsSettingsUIRebootEnabled()) {
    self.textLabel.font = [UIFont systemFontOfSize:kUIKitMainFontSize];
    self.textLabel.textColor = UIColorFromRGB(kUIKitMainTextColor);
    self.textField.font = [UIFont systemFontOfSize:kUIKitMainFontSize];
    self.textField.textColor = [UIColor grayColor];
  } else {
    self.textLabel.font = [[MDCTypography fontLoader] mediumFontOfSize:14];
    self.textLabel.textColor = [[MDCPalette greyPalette] tint900];
    self.textField.font = [[MDCTypography fontLoader] lightFontOfSize:16];
    self.textField.textColor = [[MDCPalette greyPalette] tint500];
  }
}

#pragma mark - UIView

- (void)layoutSubviews {
  if (self.identifyingIconView.image) {
    _textFieldTrailingConstraint.constant = -kLabelAndFieldGap;

    // Set the size constraints of the icon view to the dimensions of the image.
    _iconHeightConstraint.constant = self.identifyingIconView.image.size.height;
    _iconWidthConstraint.constant = self.identifyingIconView.image.size.width;
  } else {
    _textFieldTrailingConstraint.constant = 0;

    _iconHeightConstraint.constant = 0;
    _iconWidthConstraint.constant = 0;
  }

  [super layoutSubviews];
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.textField.text = nil;
  self.textField.returnKeyType = UIReturnKeyNext;
  self.textField.keyboardType = UIKeyboardTypeDefault;
  self.textField.autocapitalizationType = UITextAutocapitalizationTypeWords;
  self.textField.autocorrectionType = UITextAutocorrectionTypeNo;
  self.textField.clearButtonMode = UITextFieldViewModeWhileEditing;
  self.textField.accessibilityIdentifier = nil;
  self.textField.enabled = NO;
  self.textField.delegate = nil;
  [self.textField removeTarget:nil
                        action:nil
              forControlEvents:UIControlEventAllEvents];
  self.identifyingIconView.image = nil;
}

#pragma mark - Accessibility

- (NSString*)accessibilityLabel {
  return [NSString
      stringWithFormat:@"%@, %@", self.textLabel.text, self.textField.text];
}

@end
