// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_constants.h"

namespace app_list {

const SkColor kContentsBackgroundColor = SkColorSetRGB(0xF5, 0xF5, 0xF5);
const SkColor kSearchBoxBackground = SK_ColorWHITE;
const SkColor kTopSeparatorColor = SkColorSetRGB(0xE5, 0xE5, 0xE5);

// 6% black over kContentsBackgroundColor
const SkColor kHighlightedColor = SkColorSetRGB(0xE6, 0xE6, 0xE6);
// 10% black over kContentsBackgroundColor
const SkColor kSelectedColor = SkColorSetRGB(0xDC, 0xDC, 0xDC);

const SkColor kPagerHoverColor = SkColorSetRGB(0xB4, 0xB4, 0xB4);
const SkColor kPagerNormalColor = SkColorSetRGB(0xE2, 0xE2, 0xE2);
const SkColor kPagerSelectedColor = SkColorSetRGB(0x46, 0x8F, 0xFC);

const SkColor kResultBorderColor = SkColorSetRGB(0xE5, 0xE5, 0xE5);
const SkColor kResultDefaultTextColor = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kResultDimmedTextColor = SkColorSetRGB(0x96, 0x96, 0x96);
const SkColor kResultURLTextColor = SkColorSetRGB(0x00, 0x99, 0x33);

const SkColor kGridTitleColor = SkColorSetRGB(0x5A, 0x5A, 0x5A);
const SkColor kGridTitleHoverColor = SkColorSetRGB(0x3C, 0x3C, 0x3C);

// Duration in milliseconds for page transition.
const int kPageTransitionDurationInMs = 180;

// Duration in milliseconds for over scroll page transition.
const int kOverscrollPageTransitionDurationMs = 50;

// Preferred number of columns and rows in apps grid.
const int kPreferredCols = 4;
const int kPreferredRows = 4;

// Font style for app item labels.
const ui::ResourceBundle::FontStyle kItemTextFontStyle =
    ui::ResourceBundle::SmallBoldFont;

}  // namespace app_list
