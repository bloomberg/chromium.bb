// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_VIEW_H_
#define IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/find_bar/find_bar_ui_element.h"

// Find bar view.
// It shows a textfield that hosts the search term, a label with results count
// in format of "1 of 13", and next/previous/close buttons.
@interface FindBarView : UIView<FindBarUIElement>

// Designated initializer. |darkAppearance| makes the background to dark color
// and changes font colors to lighter colors.
- (instancetype)initWithDarkAppearance:(BOOL)darkAppearance
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_VIEW_H_
