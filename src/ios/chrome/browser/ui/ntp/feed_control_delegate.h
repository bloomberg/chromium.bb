// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_CONTROL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_CONTROL_DELEGATE_H_

#import "ios/chrome/browser/discover_feed/feed_constants.h"

// Delegate for controlling the presented feed.
@protocol FeedControlDelegate

// Handles operations after a new feed has been selected. e.g. Displays the
// feed, updates states, etc.
- (void)handleFeedSelected:(FeedType)feedType;

// Handles the sorting being selected for the Following feed.
- (void)handleSortTypeForFollowingFeed:(FollowingFeedSortType)sortType;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_CONTROL_DELEGATE_H_
