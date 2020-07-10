// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_CELLS_POPUP_MENU_ITEM_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_CELLS_POPUP_MENU_ITEM_H_

#import <UIKit/UIKit.h>

// Identifier for the action associated with a popup menu item.
typedef NS_ENUM(NSInteger, PopupMenuAction) {
  PopupMenuActionReload,
  PopupMenuActionStop,
  PopupMenuActionOpenDownloads,
  PopupMenuActionOpenNewTab,
  PopupMenuActionOpenNewIncognitoTab,
  PopupMenuActionReadLater,
  PopupMenuActionPageBookmark,
  PopupMenuActionTranslate,
  PopupMenuActionFindInPage,
  PopupMenuActionRequestDesktop,
  PopupMenuActionRequestMobile,
  PopupMenuActionSiteInformation,
  PopupMenuActionReportIssue,
  PopupMenuActionHelp,
  PopupMenuActionTextZoom,
#if !defined(NDEBUG)
  PopupMenuActionViewSource,
#endif  // !defined(NDEBUG)
  PopupMenuActionBookmarks,
  PopupMenuActionReadingList,
  PopupMenuActionRecentTabs,
  PopupMenuActionHistory,
  PopupMenuActionSettings,
  PopupMenuActionCloseTab,
  PopupMenuActionNavigate,
  PopupMenuActionPasteAndGo,
  PopupMenuActionVoiceSearch,
  // TODO(crbug.com/974751): Check if this is still used.
  PopupMenuActionSearch,
  // TODO(crbug.com/974751): Check if this is still used.
  PopupMenuActionIncognitoSearch,
  PopupMenuActionQRCodeSearch,
  PopupMenuActionSearchCopiedImage,
  // Language selection popup menu
  PopupMenuActionSelectLanguage,
  // Translate option selection popup menu
  PopupMenuActionChangeTargetLanguage,
  PopupMenuActionAlwaysTranslateSourceLanguage,
  PopupMenuActionNeverTranslateSourceLanguage,
  PopupMenuActionNeverTranslateSite,
  PopupMenuActionChangeSourceLanguage,
  // Badge overflow popup menu
  PopupMenuActionShowSavePasswordOptions,
  PopupMenuActionShowUpdatePasswordOptions,
  PopupMenuActionShowSaveCardOptions,
  PopupMenuActionShowTranslateOptions,
};

// Protocol defining a popup item.
@protocol PopupMenuItem

// Action identifier for the popup item.
@property(nonatomic, assign) PopupMenuAction actionIdentifier;

// Returns the size needed to display the cell associated with this item.
- (CGSize)cellSizeForWidth:(CGFloat)width;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_CELLS_POPUP_MENU_ITEM_H_
