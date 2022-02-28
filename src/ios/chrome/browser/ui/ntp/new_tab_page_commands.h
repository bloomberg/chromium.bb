// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COMMANDS_H_

// Commands to communicate back to the NewTabPageCoordinator
@protocol NewTabPageCommands

// Updates the NTP to take into account a new feed, or a change in feed
// visibility.
- (void)updateNTPForFeed;

// Called when the Discover Feed layout needs updating. e.g. An inner view like
// ContentSuggestions height might have changed and the Feed needs to update its
// layout to reflect this.
- (void)updateDiscoverFeedLayout;

// Called when the NTP's content offset needs to be set to return to the top of
// the page.
- (void)setContentOffsetToTop;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COMMANDS_H_
