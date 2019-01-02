// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_cell.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsMostVisitedActionItem

@synthesize action = _action;
@synthesize count = _count;
@synthesize metricsRecorded = _metricsRecorded;
@synthesize suggestionIdentifier = _suggestionIdentifier;
@synthesize title = _title;
@synthesize accessibilityLabel = _accessibilityLabel;

- (instancetype)initWithAction:(ContentSuggestionsMostVisitedAction)action {
  self = [super initWithType:0];
  if (self) {
    _action = action;
    self.cellClass = [ContentSuggestionsMostVisitedActionCell class];
    self.title = [self titleForAction:_action];
  }
  return self;
}

#pragma mark - Accessors

- (void)setTitle:(NSString*)title {
  if ([_title isEqualToString:title])
    return;
  _title = title;
  [self updateAccessibilityLabel];
}

- (void)setCount:(NSInteger)count {
  if (_count == count)
    return;
  _count = count;
  [self updateAccessibilityLabel];
}

#pragma mark - AccessibilityCustomAction

- (void)configureCell:(ContentSuggestionsMostVisitedActionCell*)cell {
  [super configureCell:cell];
  cell.accessibilityCustomActions = nil;
  cell.titleLabel.text = self.title;
  cell.accessibilityLabel =
      self.accessibilityLabel.length ? self.accessibilityLabel : self.title;
  cell.iconView.image = [self imageForAction:_action];
  if (self.count != 0) {
    cell.countLabel.text = [@(self.count) stringValue];
    cell.countContainer.hidden = NO;
  } else {
    cell.countContainer.hidden = YES;
  }
}

#pragma mark - ContentSuggestionsItem

- (CGFloat)cellHeightForWidth:(CGFloat)width {
  return [ContentSuggestionsMostVisitedActionCell defaultSize].height;
}

#pragma mark - Private

// Returns the title to use for a cell with |action|.
- (NSString*)titleForAction:(ContentSuggestionsMostVisitedAction)action {
  switch (action) {
    case ContentSuggestionsMostVisitedActionBookmark:
      return l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_BOOKMARKS);
    case ContentSuggestionsMostVisitedActionReadingList:
      return l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_READING_LIST);
    case ContentSuggestionsMostVisitedActionRecentTabs:
      return l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_RECENT_TABS);
    case ContentSuggestionsMostVisitedActionHistory:
      return l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_HISTORY);
  }
}

// Returns the image to use for a cell with |action|.
- (UIImage*)imageForAction:(ContentSuggestionsMostVisitedAction)action {
  switch (action) {
    case ContentSuggestionsMostVisitedActionBookmark:
      return [self imageGettingTintedNamed:@"ntp_bookmarks_icon"];
    case ContentSuggestionsMostVisitedActionReadingList:
      return [self imageGettingTintedNamed:@"ntp_readinglist_icon"];
    case ContentSuggestionsMostVisitedActionRecentTabs:
      return [self imageGettingTintedNamed:@"ntp_recent_icon"];
    case ContentSuggestionsMostVisitedActionHistory:
      return [self imageGettingTintedNamed:@"ntp_history_icon"];
  }
}

// Returns the image named |imageName| with a rendering mode "AlwaysTemplate".
- (UIImage*)imageGettingTintedNamed:(NSString*)imageName {
  return [[UIImage imageNamed:imageName]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

// Updates self.accessibilityLabel based on the current property values.
- (void)updateAccessibilityLabel {
  // Resetting self.accessibilityLabel to nil will prompt self.title to be used
  // as the default label.  This default value should be used if:
  // - the cell is not for Reading List,
  // - there are no unread articles in the reading list.
  if (self.action != ContentSuggestionsMostVisitedActionReadingList ||
      self.count <= 0) {
    self.accessibilityLabel = nil;
    return;
  }

  BOOL hasMultipleArticles = self.count > 1;
  int messageID =
      hasMultipleArticles
          ? IDS_IOS_CONTENT_SUGGESTIONS_READING_LIST_ACCESSIBILITY_LABEL
          : IDS_IOS_CONTENT_SUGGESTIONS_READING_LIST_ACCESSIBILITY_LABEL_ONE_UNREAD;
  if (hasMultipleArticles) {
    self.accessibilityLabel = l10n_util::GetNSStringF(
        messageID, base::SysNSStringToUTF16([@(self.count) stringValue]));
  } else {
    self.accessibilityLabel = l10n_util::GetNSString(messageID);
  }
  DCHECK(self.accessibilityLabel.length);
}

@end
