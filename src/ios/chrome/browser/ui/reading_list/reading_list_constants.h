// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Accessibility identifier for reading list view.
extern NSString* const kReadingListViewID;

// Accessibility identifier for the badge icon.
extern NSString* const kTableViewURLCellFaviconBadgeViewID;

// Accessibility identifiers for reading list toolbar buttons.
extern NSString* const kReadingListToolbarEditButtonID;
extern NSString* const kReadingListToolbarDeleteButtonID;
extern NSString* const kReadingListToolbarDeleteAllReadButtonID;
extern NSString* const kReadingListToolbarCancelButtonID;
extern NSString* const kReadingListToolbarMarkButtonID;

// NSUserDefault key to save last time a Messages prompt was shown.
extern NSString* const kLastTimeUserShownReadingListMessages;
extern NSString* const kLastReadingListEntryAddedFromMessages;
extern NSString* const kShouldAnimateReadingListNTPUnreadCountBadge;
extern NSString* const kShouldAnimateReadingListOverflowMenuUnreadCountBadge;
extern CGFloat const kReadingListUnreadCountBadgeAnimationDuration;

// ChromeBrowserState pref key to never show the Reading List Message prompt.
extern const char kPrefReadingListMessagesNeverShow[];

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_CONSTANTS_H_
