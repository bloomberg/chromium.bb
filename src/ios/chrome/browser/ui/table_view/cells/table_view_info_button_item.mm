// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_info_button_item.h"

#import "ios/chrome/browser/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_info_button_item_delegate.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TableViewInfoButtonItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewInfoButtonCell class];
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(TableViewInfoButtonCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.text;
  if (self.textColor) {
    cell.textLabel.textColor = self.textColor;
  }
  if (self.detailText) {
    cell.detailTextLabel.text = self.detailText;
    if (self.detailTextColor) {
      cell.detailTextLabel.textColor = self.detailTextColor;
    }
    [cell updatePaddingForDetailText:YES];
  } else {
    [cell updatePaddingForDetailText:NO];
  }
  cell.statusTextLabel.text = self.statusText;
  if (self.accessibilityHint) {
    cell.customizedAccessibilityHint = self.accessibilityHint;
  }
  if (self.accessibilityDelegate) {
    cell.accessibilityCustomActions = [self createAccessibilityActions];
  }
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  // Update the icon image, if one is present.
  [cell setIconImage:self.image withTintColor:self.tintColor];

  // Updates if the cells UI button should be hidden.
  [cell hideUIButton:self.infoButtonIsHidden];
}

#pragma mark - Accessibility

// Creates custom accessibility actions.
- (NSArray*)createAccessibilityActions {
  UIAccessibilityCustomAction* tapAction = [[UIAccessibilityCustomAction alloc]
      initWithName:l10n_util::GetNSString(
                       IDS_IOS_TABLE_VIEW_INFO_BUTTON_ITEM_ACCESSIBILITY_TAP)
            target:self
          selector:@selector(handleTapOutsideInfoButtonForItem)];
  return @[ tapAction ];
}

// Handles accessibility action for tapping outside the info button.
- (void)handleTapOutsideInfoButtonForItem {
  [self.accessibilityDelegate handleTapOutsideInfoButtonForItem:self];
}

@end
