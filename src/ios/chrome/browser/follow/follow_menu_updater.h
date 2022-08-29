// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_FOLLOW_MENU_UPDATER_H_
#define IOS_CHROME_BROWSER_FOLLOW_FOLLOW_MENU_UPDATER_H_

@class FollowWebPageURLs;

// Protocol defining a updater for follow menu item.
@protocol FollowMenuUpdater

// Updates the follow menu item with follow |webPageURLs|, |status|,
// |domainName| and |enabled|.
- (void)updateFollowMenuItemWithFollowWebPageURLs:
            (FollowWebPageURLs*)webPageURLs
                                           status:(BOOL)status
                                       domainName:(NSString*)domainName
                                          enabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_FOLLOW_FOLLOW_MENU_UPDATER_H_
