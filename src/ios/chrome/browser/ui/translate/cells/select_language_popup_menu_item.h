// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TRANSLATE_CELLS_SELECT_LANGUAGE_POPUP_MENU_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TRANSLATE_CELLS_SELECT_LANGUAGE_POPUP_MENU_ITEM_H_

#import "ios/chrome/browser/ui/translate/cells/translate_popup_menu_item.h"

// Item used for the translate infobar's language selection popup menu.
@interface SelectLanguagePopupMenuItem : TranslatePopupMenuItem

// Language code for the item.
@property(nonatomic, strong) NSString* languageCode;

@end

#endif  // IOS_CHROME_BROWSER_UI_TRANSLATE_CELLS_SELECT_LANGUAGE_POPUP_MENU_ITEM_H_
