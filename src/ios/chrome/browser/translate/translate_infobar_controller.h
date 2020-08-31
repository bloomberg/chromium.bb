// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBAR_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBAR_CONTROLLER_H_

#include "ios/chrome/browser/infobars/infobar_controller.h"

@protocol LanguageSelectionHandler;
@protocol TranslateNotificationHandler;
@protocol TranslateOptionSelectionHandler;
namespace translate {
class TranslateInfoBarDelegate;
}

// Acts as a UIViewController for TranslateInfoBarView. Creates the infobar view
// and handles user actions.
@interface TranslateInfoBarController : InfoBarController

- (instancetype)initWithInfoBarDelegate:
    (translate::TranslateInfoBarDelegate*)infoBarDelegate
    NS_DESIGNATED_INITIALIZER;

@property(nonatomic, weak) id<LanguageSelectionHandler>
    languageSelectionHandler;

@property(nonatomic, weak) id<TranslateOptionSelectionHandler>
    translateOptionSelectionHandler;

@property(nonatomic, weak) id<TranslateNotificationHandler>
    translateNotificationHandler;

@end

#endif  // IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBAR_CONTROLLER_H_
