// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_constants.h"

#include "build/build_config.h"
#include "ui/gfx/color_palette.h"

namespace app_list {

const SkColor kContentsBackgroundColor = SkColorSetRGB(0xF2, 0xF2, 0xF2);
const SkColor kSearchBoxBackgroundDefault = SK_ColorWHITE;

const SkColor kSearchTextColor = SkColorSetRGB(0x33, 0x33, 0x33);

const SkColor kLabelBackgroundColor = SK_ColorTRANSPARENT;

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

const SkColor kFolderTitleColor = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kFolderTitleHintTextColor = SkColorSetRGB(0xA0, 0xA0, 0xA0);
// Color of the folder ink bubble.
const SkColor kFolderBubbleColor = SK_ColorWHITE;
// Color of the folder bubble shadow.
const SkColor kFolderShadowColor = SkColorSetRGB(0xBF, 0xBF, 0xBF);
const float kFolderBubbleRadius = 23;
const float kFolderBubbleOffsetY = 1;

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
const int kPreferredCols = 6;
const int kPreferredRows = 4;
const int kGridIconDimension = 48;

// The preferred app badge icon size.
const int kAppBadgeIconSize = 12;

// Preferred search result icon sizes.
const int kListIconSize = 24;
const int kListBadgeIconSize = 16;
const int kListBadgeIconOffsetX = 6;
const int kListBadgeIconOffsetY = 6;
const int kTileIconSize = 48;

const SkColor kIconColor = gfx::kChromeIconGrey;

// The number of apps shown in the start page app grid.
const size_t kNumStartPageTiles = 9;
const size_t kNumStartPageTilesFullscreen = 5;

// Maximum number of results to show in the launcher Search UI.
const size_t kMaxSearchResults = 6;

// Radius of the circle, in which if entered, show re-order preview.
const int kReorderDroppingCircleRadius = 35;

// The padding around the outside of the apps grid (sides).
const int kAppsGridPadding = 24;

// The padding around the outside of the search box (top and sides).
const int kSearchBoxPadding = 16;

// Max items allowed in a folder.
size_t kMaxFolderItems = 16;

// Number of the top items in a folder, which are shown inside the folder icon
// and animated when opening and closing a folder.
const size_t kNumFolderTopItems = 4;

// Maximum length of the folder name in chars.
const size_t kMaxFolderNameChars = 80;

// Font style for app item labels.
const ui::ResourceBundle::FontStyle kItemTextFontStyle =
    ui::ResourceBundle::SmallFont;

// The UMA histogram that logs which page gets opened by the user.
const char kPageOpenedHistogram[] = "Apps.AppListPageOpened";

// The UMA histogram that logs the type of search result opened.
const char kSearchResultOpenDisplayTypeHistogram[] =
    "Apps.AppListSearchResultOpenDisplayType";

// The UMA histogram that logs how long the search query was when a result was
// opened.
const char kSearchQueryLength[] = "Apps.AppListSearchQueryLength";

// The UMA histogram that logs the Manhattan distance from the origin of the
// search results to the selected result.
const char kSearchResultDistanceFromOrigin[] =
    "Apps.AppListSearchResultDistanceFromOrigin";

gfx::ShadowValue GetShadowForZHeight(int z_height) {
  if (z_height <= 0)
    return gfx::ShadowValue();

  switch (z_height) {
    case 1:
      return gfx::ShadowValue(gfx::Vector2d(0, 1), 4,
                              SkColorSetARGB(0x4C, 0, 0, 0));
    case 2:
      return gfx::ShadowValue(gfx::Vector2d(0, 2), 8,
                              SkColorSetARGB(0x33, 0, 0, 0));
    default:
      return gfx::ShadowValue(gfx::Vector2d(0, 8), 24,
                              SkColorSetARGB(0x3F, 0, 0, 0));
  }
}

const gfx::ShadowValues& IconStartShadows() {
  CR_DEFINE_STATIC_LOCAL(const gfx::ShadowValues, icon_shadows,
                         (1, gfx::ShadowValue(gfx::Vector2d(0, 1), 4,
                                              SkColorSetARGB(0x33, 0, 0, 0))));
  return icon_shadows;
}

const gfx::ShadowValues& IconEndShadows() {
  CR_DEFINE_STATIC_LOCAL(const gfx::ShadowValues, icon_shadows,
                         (1, gfx::ShadowValue(gfx::Vector2d(0, 4), 8,
                                              SkColorSetARGB(0x50, 0, 0, 0))));
  return icon_shadows;
}

}  // namespace app_list
