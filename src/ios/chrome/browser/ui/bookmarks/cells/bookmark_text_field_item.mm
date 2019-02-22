// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_text_field_item.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/ui/text_field_styling.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - BookmarkTextFieldItem

@implementation BookmarkTextFieldItem

@synthesize text = _text;
@synthesize placeholder = _placeholder;
@synthesize delegate = _delegate;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  self.cellClass = [BookmarkTextFieldCell class];
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(UITableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];

  BookmarkTextFieldCell* cell =
      base::mac::ObjCCastStrict<BookmarkTextFieldCell>(tableCell);
  cell.textField.text = self.text;
  cell.titleLabel.text = self.placeholder;
  cell.textField.placeholder = self.placeholder;
  cell.textField.tag = self.type;
  [cell.textField addTarget:self
                     action:@selector(textFieldDidChange:)
           forControlEvents:UIControlEventEditingChanged];
  cell.textField.delegate = self.delegate;
  cell.textField.accessibilityLabel = self.text;
  cell.textField.accessibilityIdentifier =
      [NSString stringWithFormat:@"%@_textField", self.accessibilityIdentifier];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
}

#pragma mark UIControlEventEditingChanged

- (void)textFieldDidChange:(UITextField*)textField {
  DCHECK_EQ(textField.tag, self.type);
  self.text = textField.text;
  [self.delegate textDidChangeForItem:self];
}

@end

#pragma mark - BookmarkTextFieldCell

@implementation BookmarkTextFieldCell
@synthesize textField = _textField;
@synthesize titleLabel = _titleLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (!self)
    return nil;

  // Label.
  self.titleLabel = [[UILabel alloc] init];
  self.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  self.titleLabel.adjustsFontForContentSizeCategory = YES;
  [self.titleLabel setContentHuggingPriority:UILayoutPriorityRequired
                                     forAxis:UILayoutConstraintAxisHorizontal];
  [self.titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [self.titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];

  // Textfield.
  self.textField = [[UITextField alloc] init];
  self.textField.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  self.textField.adjustsFontForContentSizeCategory = YES;
  self.textField.textColor = [BookmarkTextFieldCell textColorForEditing:NO];
  self.textField.clearButtonMode = UITextFieldViewModeWhileEditing;
  self.textField.textAlignment = NSTextAlignmentRight;
  [self.textField setContentHuggingPriority:UILayoutPriorityDefaultLow
                                    forAxis:UILayoutConstraintAxisHorizontal];
  [self.textField
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];

  // Horizontal StackView.
  UIStackView* horizontalStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ self.titleLabel, self.textField ]];
  horizontalStack.axis = UILayoutConstraintAxisHorizontal;
  horizontalStack.spacing = kBookmarkCellViewSpacing;
  horizontalStack.distribution = UIStackViewDistributionFill;
  horizontalStack.alignment = UIStackViewAlignmentCenter;
  [horizontalStack
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:horizontalStack];

  // Set up constraints.
  [NSLayoutConstraint activateConstraints:@[
    [horizontalStack.topAnchor
        constraintEqualToAnchor:self.contentView.topAnchor
                       constant:kBookmarkCellVerticalInset],
    [horizontalStack.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor
                       constant:-kBookmarkCellVerticalInset],
    [horizontalStack.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kBookmarkCellHorizontalLeadingInset],
    [horizontalStack.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor
                       constant:-kBookmarkCellHorizontalTrailingInset],
  ]];

  return self;
}

+ (UIColor*)textColorForEditing:(BOOL)editing {
  return editing ? [UIColor blackColor] : [UIColor lightGrayColor];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.textField resignFirstResponder];
  [self.textField removeTarget:nil
                        action:NULL
              forControlEvents:UIControlEventAllEvents];
  self.textField.delegate = nil;
  self.textField.text = nil;
}

@end
