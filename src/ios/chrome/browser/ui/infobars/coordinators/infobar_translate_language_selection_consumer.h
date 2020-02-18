// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_TRANSLATE_LANGUAGE_SELECTION_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_TRANSLATE_LANGUAGE_SELECTION_CONSUMER_H_

@class TableViewTextItem;

@protocol InfobarTranslateLanguageSelectionConsumer <NSObject>

// Tells the consumer about the language |items| to be displayed.
- (void)setTranslateLanguageItems:(NSArray<TableViewTextItem*>*)items;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_TRANSLATE_LANGUAGE_SELECTION_CONSUMER_H_
