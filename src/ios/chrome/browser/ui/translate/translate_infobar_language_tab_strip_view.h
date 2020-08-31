// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_LANGUAGE_TAB_STRIP_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_LANGUAGE_TAB_STRIP_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/translate/translate_infobar_language_tab_view.h"

@protocol TranslateInfobarLanguageTabStripViewDelegate;

// A view containing a scrollable tab strip featuring the source and the target
// language tabs. When a language tab is tapped, it is scrolled into view, if
// necessary.
@interface TranslateInfobarLanguageTabStripView : UIView

// Source language name.
@property(nonatomic, copy) NSString* sourceLanguage;

// Target language name.
@property(nonatomic, copy) NSString* targetLanguage;

// State of the source language tab.
@property(nonatomic)
    TranslateInfobarLanguageTabViewState sourceLanguageTabState;

// State of the target language tab.
@property(nonatomic)
    TranslateInfobarLanguageTabViewState targetLanguageTabState;

// Delegate object that gets notified if user taps the source or the target
// language tabs.
@property(nonatomic, weak) id<TranslateInfobarLanguageTabStripViewDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_LANGUAGE_TAB_STRIP_VIEW_H_
