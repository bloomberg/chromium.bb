// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_constants.h"

namespace app_list {

const SkColor kContentsBackgroundColor = SkColorSetRGB(0xF2, 0xF2, 0xF2);
const SkColor kSearchBoxBackground = SK_ColorWHITE;

const SkColor kSearchTextColor = SkColorSetRGB(0x33, 0x33, 0x33);

// In Windows, transparent background color will cause ugly text rendering,
// therefore kContentsBackgroundColor should be used. See crbug.com/406989
#if defined(OS_CHROMEOS)
const SkColor kLabelBackgroundColor = SK_ColorTRANSPARENT;
#else
const SkColor kLabelBackgroundColor = kContentsBackgroundColor;
#endif

const SkColor kTopSeparatorColor = SkColorSetRGB(0xC0, 0xC0, 0xC0);
const SkColor kBottomSeparatorColor = SkColorSetRGB(0xC0, 0xC0, 0xC0);

// The color of the separator used inside dialogs in the app list.
const SkColor kDialogSeparatorColor = SkColorSetRGB(0xD1, 0xD1, 0xD1);

// The mouse hover colour (3% black).
const SkColor kHighlightedColor = SkColorSetARGB(8, 0, 0, 0);
// The keyboard select colour (6% black).
const SkColor kSelectedColor = SkColorSetARGB(15, 0, 0, 0);

const SkColor kPagerHoverColor = SkColorSetRGB(0xB4, 0xB4, 0xB4);
const SkColor kPagerNormalColor = SkColorSetRGB(0xE2, 0xE2, 0xE2);
const SkColor kPagerSelectedColor = SkColorSetRGB(0x46, 0x8F, 0xFC);

const SkColor kResultBorderColor = SkColorSetRGB(0xE5, 0xE5, 0xE5);
const SkColor kResultDefaultTextColor = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kResultDimmedTextColor = SkColorSetRGB(0x84, 0x84, 0x84);
const SkColor kResultURLTextColor = SkColorSetRGB(0x00, 0x99, 0x33);

const SkColor kGridTitleColor = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kGridTitleHoverColor = kGridTitleColor;

const SkColor kFolderTitleColor = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kFolderTitleHintTextColor = SkColorSetRGB(0xA0, 0xA0, 0xA0);
// Color of the folder ink bubble.
const SkColor kFolderBubbleColor = SK_ColorWHITE;
// Color of the folder bubble shadow.
const SkColor kFolderShadowColor = SkColorSetRGB(0xBF, 0xBF, 0xBF);
const float kFolderBubbleRadius = 23;
const float kFolderShadowRadius = 23.5;
const float kFolderShadowOffsetY = 1;

const SkColor kCardBackgroundColor = SK_ColorWHITE;

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

// The height of the custom page widget when collapsed on the start page.
const int kCustomPageCollapsedHeight = 78;

// Animation curve used for fading in the target page when opening or closing
// a folder.
const gfx::Tween::Type kFolderFadeInTweenType = gfx::Tween::EASE_IN_2;

// Animation curve used for fading out the target page when opening or closing
// a folder.
const gfx::Tween::Type kFolderFadeOutTweenType = gfx::Tween::FAST_OUT_LINEAR_IN;

// Preferred number of columns and rows in apps grid.
const int kPreferredCols = 4;
const int kPreferredRows = 4;
const int kGridIconDimension = 48;

// Preferred search result icon sizes.
const int kListIconSize = 24;
const int kTileIconSize = 48;

// Preferred number of columns and rows in the centered app list apps grid.
const int kCenteredPreferredCols = 6;
const int kCenteredPreferredRows = 3;

// Preferred number of columns and rows in the experimental app list apps grid.
const int kExperimentalPreferredCols = 6;
const int kExperimentalPreferredRows = 4;

// Radius of the circle, in which if entered, show re-order preview.
const int kReorderDroppingCircleRadius = 35;

// The padding around the outside of the experimental app list apps grid
// (sides).
const int kExperimentalAppsGridPadding = 24;

// The padding around the outside of the experimental app list search box (top
// and sides).
const int kExperimentalSearchBoxPadding = 16;

// Max items allowed in a folder.
size_t kMaxFolderItems = 16;

// Number of the top items in a folder, which are shown inside the folder icon
// and animated when opening and closing a folder.
const size_t kNumFolderTopItems = 4;

// Maximum length of the folder name in chars.
const size_t kMaxFolderNameChars = 40;

// Font style for app item labels.
const ui::ResourceBundle::FontStyle kItemTextFontStyle =
    ui::ResourceBundle::SmallFont;

// The UMA histogram that logs which page gets opened by the user.
const char kPageOpenedHistogram[] = "Apps.AppListPageOpened";

// The UMA histogram that logs the type of search result opened.
const char kSearchResultOpenDisplayTypeHistogram[] =
    "Apps.AppListSearchResultOpenDisplayType";

#if defined(OS_LINUX)
#if defined(GOOGLE_CHROME_BUILD)
const char kAppListWMClass[] = "chrome_app_list";
#else  // CHROMIUM_BUILD
const char kAppListWMClass[] = "chromium_app_list";
#endif
#endif

gfx::ShadowValue GetShadowForZHeight(int z_height) {
  if (z_height <= 0)
    return gfx::ShadowValue();

  switch (z_height) {
    case 1:
      return gfx::ShadowValue(gfx::Vector2d(0, 1), 2,
                              SkColorSetARGB(0x4C, 0, 0, 0));
    case 2:
      return gfx::ShadowValue(gfx::Vector2d(0, 2), 4,
                              SkColorSetARGB(0x33, 0, 0, 0));
    default:
      return gfx::ShadowValue(gfx::Vector2d(0, 8), 12,
                              SkColorSetARGB(0x3F, 0, 0, 0));
  }
}

}  // namespace app_list
