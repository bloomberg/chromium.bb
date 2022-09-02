// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/mediator_util.h"

#include "base/callback.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_category_wrapper.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
ContentSuggestionsSectionInformation* EmptySectionInfo(
    ContentSuggestionsSectionID sectionID) {
  ContentSuggestionsSectionInformation* sectionInfo = [
      [ContentSuggestionsSectionInformation alloc] initWithSectionID:sectionID];
  sectionInfo.title = nil;
  sectionInfo.footerTitle = nil;
  sectionInfo.showIfEmpty = NO;
  sectionInfo.layout = ContentSuggestionsSectionLayoutCustom;

  return sectionInfo;
}
}  // namespace


ContentSuggestionsSectionInformation* LogoSectionInformation() {
  ContentSuggestionsSectionInformation* sectionInfo =
      [[ContentSuggestionsSectionInformation alloc]
          initWithSectionID:ContentSuggestionsSectionLogo];
  sectionInfo.title = nil;
  sectionInfo.footerTitle = nil;
  sectionInfo.showIfEmpty = YES;
  sectionInfo.layout = ContentSuggestionsSectionLayoutCustom;

  return sectionInfo;
}

ContentSuggestionsSectionInformation* ReturnToRecentTabSectionInformation() {
  return EmptySectionInfo(ContentSuggestionsSectionReturnToRecentTab);
}

ContentSuggestionsSectionInformation* PromoSectionInformation() {
  return EmptySectionInfo(ContentSuggestionsSectionPromo);
}

ContentSuggestionsSectionInformation* MostVisitedSectionInformation() {
  return EmptySectionInfo(ContentSuggestionsSectionMostVisited);
}

ContentSuggestionsSectionInformation* SingleCellSectionInformation() {
  return EmptySectionInfo(ContentSuggestionsSectionSingleCell);
}

ContentSuggestionsMostVisitedItem* ConvertNTPTile(
    const ntp_tiles::NTPTile& tile,
    ContentSuggestionsSectionInformation* sectionInfo) {
  ContentSuggestionsMostVisitedItem* suggestion =
      [[ContentSuggestionsMostVisitedItem alloc] initWithType:0];

  suggestion.title = base::SysUTF16ToNSString(tile.title);
  suggestion.URL = tile.url;
  suggestion.source = tile.source;
  suggestion.titleSource = tile.title_source;
  suggestion.accessibilityTraits = UIAccessibilityTraitButton;

  suggestion.suggestionIdentifier = [[ContentSuggestionIdentifier alloc] init];
  suggestion.suggestionIdentifier.IDInSection = tile.url.spec();
  suggestion.suggestionIdentifier.sectionInfo = sectionInfo;

  return suggestion;
}

ContentSuggestionsMostVisitedActionItem* BookmarkActionItem() {
  return [[ContentSuggestionsMostVisitedActionItem alloc]
      initWithCollectionShortcutType:NTPCollectionShortcutTypeBookmark];
}

ContentSuggestionsMostVisitedActionItem* ReadingListActionItem() {
  return [[ContentSuggestionsMostVisitedActionItem alloc]
      initWithCollectionShortcutType:NTPCollectionShortcutTypeReadingList];
}

ContentSuggestionsMostVisitedActionItem* RecentTabsActionItem() {
  return [[ContentSuggestionsMostVisitedActionItem alloc]
      initWithCollectionShortcutType:NTPCollectionShortcutTypeRecentTabs];
}

ContentSuggestionsMostVisitedActionItem* HistoryActionItem() {
  return [[ContentSuggestionsMostVisitedActionItem alloc]
      initWithCollectionShortcutType:NTPCollectionShortcutTypeHistory];
}
