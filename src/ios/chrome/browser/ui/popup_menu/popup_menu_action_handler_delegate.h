// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_ACTION_HANDLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_ACTION_HANDLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@protocol PopupMenuItem;
@class TableViewItem;

// Delegate for the PopupMenuActionHandler.
@protocol PopupMenuActionHandlerDelegate
// Adds the current page to the reading list.
- (void)readPageLater;
// Navigates to the page associated with |item|.
- (void)navigateToPageForItem:(TableViewItem<PopupMenuItem>*)item;
// Records open settings metric per profile type.
- (void)recordSettingsMetricsPerProfile;
// Records open downloads metric per profile type.
- (void)recordDownloadsMetricsPerProfile;
// Starts a reverse image search for the image currently in the pasteboard.
- (void)searchCopiedImage;
@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_ACTION_HANDLER_DELEGATE_H_
