// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/font_size_tab_helper.h"

#import <UIKit/UIKit.h>

#include "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(FontSizeTabHelper);

FontSizeTabHelper::~FontSizeTabHelper() {
  // Remove observer in destructor because |this| is captured by the usingBlock
  // in calling [NSNotificationCenter.defaultCenter
  // addObserverForName:object:queue:usingBlock] in constructor.
  [NSNotificationCenter.defaultCenter
      removeObserver:content_size_did_change_observer_];
}

FontSizeTabHelper::FontSizeTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state->AddObserver(this);
  content_size_did_change_observer_ = [NSNotificationCenter.defaultCenter
      addObserverForName:UIContentSizeCategoryDidChangeNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* _Nonnull note) {
                SetPageFontSize(GetSystemSuggestedFontSize());
              }];
}

void FontSizeTabHelper::SetPageFontSize(int size) {
  if (web_state_->ContentIsHTML()) {
    NSString* js = [NSString
        stringWithFormat:@"__gCrWeb.accessibility.adjustFontSize(%d)", size];
    web_state_->ExecuteJavaScript(base::SysNSStringToUTF16(js));
  }
}

int FontSizeTabHelper::GetSystemSuggestedFontSize() const {
  // Scaling numbers are calculated by [UIFont
  // preferredFontForTextStyle:UIFontTextStyleBody].pointSize, which are [14,
  // 15, 16, 17(default), 19, 21, 23, 28, 33, 40, 47, 53].
  static NSDictionary* font_size_map = @{
    UIContentSizeCategoryUnspecified : @100,
    UIContentSizeCategoryExtraSmall : @82,
    UIContentSizeCategorySmall : @88,
    UIContentSizeCategoryMedium : @94,
    UIContentSizeCategoryLarge : @100,  // system default
    UIContentSizeCategoryExtraLarge : @112,
    UIContentSizeCategoryExtraExtraLarge : @124,
    UIContentSizeCategoryExtraExtraExtraLarge : @135,
    UIContentSizeCategoryAccessibilityMedium : @165,
    UIContentSizeCategoryAccessibilityLarge : @194,
    UIContentSizeCategoryAccessibilityExtraLarge : @235,
    UIContentSizeCategoryAccessibilityExtraExtraLarge : @276,
    UIContentSizeCategoryAccessibilityExtraExtraExtraLarge : @312,
  };
  UIContentSizeCategory category =
      UIApplication.sharedApplication.preferredContentSizeCategory;
  NSNumber* font_size = font_size_map[category];
  UMA_HISTOGRAM_BOOLEAN("Accessibility.iOS.NewLargerTextCategory", !font_size);
  return font_size ? font_size.intValue : 100;
}

void FontSizeTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}

void FontSizeTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  DCHECK_EQ(web_state, web_state_);
  int size = GetSystemSuggestedFontSize();
  if (size != 100)
    SetPageFontSize(size);
}
