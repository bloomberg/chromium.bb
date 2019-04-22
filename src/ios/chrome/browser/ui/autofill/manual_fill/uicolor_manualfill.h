// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_UICOLOR_MANUALFILL_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_UICOLOR_MANUALFILL_H_

#import <UIKit/UIKit.h>

@interface UIColor (ManualFill)

// Color to set in interactable elements for manual fill (0.1, 0.45, 0.91 RGB).
@property(class, nonatomic, readonly) UIColor* cr_manualFillTintColor;

// Color for the text in manual fill chips.
@property(class, nonatomic, readonly) UIColor* cr_manualFillChipDarkTextColor;

// Color for the manual fill chips.
@property(class, nonatomic, readonly) UIColor* cr_manualFillChipColor;

// Color for the highlighted manual fill chips.
@property(class, nonatomic, readonly)
    UIColor* cr_manualFillHighlightedChipColor;

// Color for the line separators in manual fill (0.66, 0.66, 0.66 RGB).
@property(class, nonatomic, readonly) UIColor* cr_manualFillSeparatorColor;

// Color for the gray line separators in manual fill.
@property(class, nonatomic, readonly) UIColor* cr_manualFillGrayLineColor;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_UICOLOR_MANUALFILL_H_
