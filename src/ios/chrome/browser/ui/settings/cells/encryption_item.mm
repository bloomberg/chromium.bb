// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/encryption_item.h"

#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation EncryptionItem

@synthesize accessoryType = _accessoryType;
@synthesize text = _text;
@synthesize enabled = _enabled;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [EncryptionCell class];
    self.enabled = YES;
    self.accessibilityTraits |= UIAccessibilityTraitButton;
  }
  return self;
}

- (void)configureCell:(EncryptionCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.accessoryType = self.accessoryType;
  cell.textLabel.text = self.text;
  cell.textLabel.textColor =
      self.enabled ? [UIColor blackColor]
                   : UIColorFromRGB(kTableViewSecondaryLabelLightGrayTextColor);
}

@end

@implementation EncryptionCell

@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.numberOfLines = 0;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    [self.contentView addSubview:_textLabel];

    // Set up the constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_textLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
    ]];
    AddOptionalVerticalPadding(self.contentView, _textLabel,
                               kTableViewLargeVerticalSpacing);
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.accessoryType = UITableViewCellAccessoryNone;
  self.textLabel.text = nil;
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  return self.textLabel.text;
}

@end
