// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_constants.h"

namespace app_list {

const SkColor kContentsBackgroundColor = SkColorSetRGB(0xFB, 0xFB, 0xFB);
const SkColor kSearchBoxBackground = SK_ColorWHITE;
const SkColor kTopSeparatorColor = SkColorSetRGB(0xE5, 0xE5, 0xE5);

// 6% black over kContentsBackgroundColor
const SkColor kHighlightedColor = SkColorSetRGB(0xE6, 0xE6, 0xE6);
// 10% black over kContentsBackgroundColor
const SkColor kSelectedColor = SkColorSetRGB(0xF1, 0xF1, 0xF1);

const SkColor kPagerHoverColor = SkColorSetRGB(0xB4, 0xB4, 0xB4);
const SkColor kPagerNormalColor = SkColorSetRGB(0xE2, 0xE2, 0xE2);
const SkColor kPagerSelectedColor = SkColorSetRGB(0x46, 0x8F, 0xFC);

const SkColor kResultBorderColor = SkColorSetRGB(0xE5, 0xE5, 0xE5);
const SkColor kResultDefaultTextColor = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kResultDimmedTextColor = SkColorSetRGB(0x96, 0x96, 0x96);
const SkColor kResultURLTextColor = SkColorSetRGB(0x00, 0x99, 0x33);

const SkColor kGridTitleColor = SkColorSetRGB(0x5A, 0x5A, 0x5A);
const SkColor kGridTitleHoverColor = SkColorSetRGB(0x3C, 0x3C, 0x3C);

// Color of the folder ink bubble.
const SkColor kFolderBubbleColor = SkColorSetRGB(0xD7, 0xD7, 0xD7);

// Duration in milliseconds for page transition.
const int kPageTransitionDurationInMs = 180;

// Duration in milliseconds for over scroll page transition.
const int kOverscrollPageTransitionDurationMs = 50;

// Duration in milliseconds for fading in the target page when opening
// or closing a folder, and the duration for the top folder icon animation
// for flying in or out the folder.
const int kFolderTransitionInDurationMs = 400;

// Duration in milliseconds for fading out the old page when opening or closing
// a folder.
const int kFolderTransitionOutDurationMs = 30;

// Animation curve used for fading in the target page when opening or closing
// a folder.
const gfx::Tween::Type kFolderFadeInTweenType = gfx::Tween::EASE_IN_2;

// Animation curve used for fading out the target page when opening or closing
// a folder.
const gfx::Tween::Type kFolderFadeOutTweenType = gfx::Tween::EASE_IN_3;

// Preferred number of columns and rows in apps grid.
const int kPreferredCols = 4;
const int kPreferredRows = 4;
const int kPreferredIconDimension = 48;

// Preferred number of columns and rows in the experimental app list apps grid.
const int kExperimentalPreferredCols = 6;
const int kExperimentalPreferredRows = 3;

// Number of the top items in a folder, which are shown inside the folder icon
// and animated when opening and closing a folder.
const size_t kNumFolderTopItems = 4;

// Font style for app item labels.
const ui::ResourceBundle::FontStyle kItemTextFontStyle =
    ui::ResourceBundle::SmallBoldFont;

#if defined(OS_LINUX)
#if defined(GOOGLE_CHROME_BUILD)
const char kAppListWMClass[] = "chrome_app_list";
#else  // CHROMIUM_BUILD
const char kAppListWMClass[] = "chromium_app_list";
#endif
#endif

}  // namespace app_list
