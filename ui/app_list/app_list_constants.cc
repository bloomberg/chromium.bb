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

const SkColor kBottomSeparatorColor = SkColorSetRGB(0xC0, 0xC0, 0xC0);
// Color of bottom separtor under folder title (12% white) in full screen mode.
const SkColor kBottomSeparatorColorFullScreen =
    SkColorSetARGB(0x1F, 0xFF, 0xFF, 0xFF);

// The color of the separator used inside dialogs in the app list.
const SkColor kDialogSeparatorColor = SkColorSetRGB(0xD1, 0xD1, 0xD1);

// The mouse hover colour (3% black).
const SkColor kHighlightedColor = SkColorSetARGB(8, 0, 0, 0);
// The keyboard select colour (6% black).
const SkColor kSelectedColor = SkColorSetARGB(15, 0, 0, 0);
// The keyboard select color for grid views, which are on top of a black shield
// view for new design (12% white).
const SkColor kGridSelectedColor = SkColorSetARGB(0x1F, 0xFF, 0xFF, 0xFF);
// Selection color for answer card (3% black).
const SkColor kAnswerCardSelectedColor = SkColorSetARGB(0x08, 0x00, 0x00, 0x00);

const SkColor kPagerHoverColor = SkColorSetRGB(0xB4, 0xB4, 0xB4);
const SkColor kPagerNormalColor = SkColorSetRGB(0xE2, 0xE2, 0xE2);
const SkColor kPagerSelectedColor = SkColorSetRGB(0x46, 0x8F, 0xFC);

const SkColor kResultBorderColor = SkColorSetRGB(0xE5, 0xE5, 0xE5);
const SkColor kResultDefaultTextColor = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kResultDimmedTextColor = SkColorSetRGB(0x84, 0x84, 0x84);
const SkColor kResultURLTextColor = SkColorSetRGB(0x00, 0x99, 0x33);

const SkColor kGridTitleColor = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kGridTitleColorFullscreen = SK_ColorWHITE;

const int kGridTileWidth = 96;
const int kGridTileHeight = 99;
const int kGridTileSpacing = 24;
const int kGridIconTopPadding = 24;
const int kGridTitleSpacing = 10;
const int kGridTitleHorizontalPadding = 8;
const int kGridSelectedSize = 64;
const int kGridSelectedCornerRadius = 8;

const SkColor kFolderTitleColor = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kFolderTitleHintTextColor = SkColorSetRGB(0xA0, 0xA0, 0xA0);
// Color of the folder ink bubble.
const SkColor kFolderBubbleColor = SK_ColorWHITE;
// Color of folder bubble color (12% white) in full screen mode.
const SkColor kFolderBubbleColorFullScreen =
    SkColorSetARGB(0x1F, 0xFF, 0xFF, 0xFF);
// Color of the folder bubble shadow.
const SkColor kFolderShadowColor = SkColorSetRGB(0xBF, 0xBF, 0xBF);
const float kFolderBubbleOpacity = 0.12f;
const float kFolderBubbleRadius = 23;
const float kFolderBubbleOffsetY = 1;

const SkColor kCardBackgroundColor = SK_ColorWHITE;
const SkColor kCardBackgroundColorFullscreen = SkColorSetRGB(0xFA, 0xFA, 0xFC);

// Duration in milliseconds for page transition.
const int kPageTransitionDurationInMs = 250;

// Dampening value for PaginationModel's SlideAnimation.
const int kPageTransitionDurationDampening = 3;

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
const int kPreferredColsFullscreen = 5;
const int kPreferredRows = 4;
const int kPreferredRowsFullscreen = 5;
const int kGridIconDimension = 48;

// The preferred app badge icon size.
const int kAppBadgeIconSize = 12;
// The preferred badge background(circle) radius.
const int kBadgeBackgroundRadius = 10;

// Preferred search result icon sizes.
const int kListIconSize = 24;
const int kListIconSizeFullscreen = 18;
const int kListBadgeIconSize = 16;
const int kListBadgeIconSizeFullscreen = 14;
const int kListBadgeIconOffsetX = 6;
const int kListBadgeIconOffsetY = 6;
const int kTileIconSize = 48;

const SkColor kIconColor = gfx::kChromeIconGrey;

// The drag and drop app icon should get scaled by this factor.
const float kDragDropAppIconScale = 1.2f;

// The drag and drop icon scaling up or down animation transition duration.
const int kDragDropAppIconScaleTransitionInMs = 20;

// The number of apps shown in the start page app grid.
const size_t kNumStartPageTiles = 9;
const size_t kNumStartPageTilesFullscreen = 5;

// Maximum number of results to show in the launcher Search UI.
const size_t kMaxSearchResults = 6;

// Top padding of expand arrow.
const size_t kExpandArrowTopPadding = 29;

// Radius of the circle, in which if entered, show re-order preview.
const int kReorderDroppingCircleRadius = 35;

// The padding around the outside of the apps grid (sides).
const int kAppsGridPadding = 24;

// The left and right side padding of the apps grid in fullscreen mode. The
// space is used for page switcher and its padding on the right side. Left side
// should have the same space to keep the apps grid horizontally centered.
const int kAppsGridLeftRightPaddingFullscreen = 40;

// The left and right padding from the folder name bottom separator to the edge
// of the left or right edge of the left most and right most app item.
const int kBottomSeparatorLeftRightPaddingFullScreen = 24;

// The bottom padding from the bottom separator to the top of the app item.
const int kBottomSeparatorBottomPaddingFullScreen = 24;

// The padding around the outside of the search box (top and sides).
const int kSearchBoxPadding = 16;
const int kSearchBoxTopPadding = 24;

// Bottom padding of search box in peeking state.
const int kSearchBoxPeekingBottomPadding = 12;

// Bottom padding of search box in fullscreen state.
const int kSearchBoxFullscreenBottomPadding = 24;

// The background border corner radius of the search box in fullscreen mode.
const int kSearchBoxBorderCornerRadiusFullscreen = 24;

// Preferred height of search box.
const int kSearchBoxPreferredHeight = 48;

// The height of the peeking app list from the bottom of the screen.
const int kPeekingAppListHeight = 320;

// The height/width of the shelf from the bottom/side of the screen.
const int kShelfSize = 48;

// Max items allowed in a folder.
const size_t kMaxFolderItems = 16;
// Max items allowed in a folder for fullscreen app list folder v1.
const size_t kMaxFolderItemsFullscreen = 20;

// Number of the top items in a folder, which are shown inside the folder icon
// and animated when opening and closing a folder.
const size_t kNumFolderTopItems = 4;

// Maximum length of the folder name in chars.
const size_t kMaxFolderNameChars = 80;

// Font style for app item labels.
const ui::ResourceBundle::FontStyle kItemTextFontStyle =
    ui::ResourceBundle::SmallFont;

// The UMA histogram that logs usage of suggested and regular apps.
const char kAppListAppLaunched[] = "Apps.AppListAppLaunched";

// The UMA histogram that logs usage of suggested and regular apps while the
// fullscreen launcher is enabled.
const char kAppListAppLaunchedFullscreen[] =
    "Apps.AppListAppLaunchedFullscreen";

// The UMA histogram that logs usage of state transitions in the new
// app list UI.
const char kAppListStateTransitionSourceHistogram[] =
    "Apps.AppListStateTransitionSource";

// The UMA histogram that logs the source of vertical page switcher usage in the
// app list.
const char kAppListPageSwitcherSourceHistogram[] =
    "Apps.AppListPageSwitcherSource";

// The UMA histogram that logs usage of the original and redesigned folders.
const char kAppListFolderOpenedHistogram[] = "Apps.AppListFolderOpened";

// The UMA histogram that logs how the app list transitions from peeking to
// fullscreen.
const char kAppListPeekingToFullscreenHistogram[] =
    "Apps.AppListPeekingToFullscreenSource";

// The UMA histogram that logs how the app list is shown.
const char kAppListToggleMethodHistogram[] = "Apps.AppListShowSource";

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

// The height of tiles in search result.
const int kSearchTileHeight = 90;

// The size of the search icon in the search box.
const int kSearchIconSize = 24;

// Default color used when wallpaper customized color is not available for
// searchbox, #000 at 87% opacity.
const SkColor kDefaultSearchboxColor =
    SkColorSetARGBMacro(0xDE, 0x00, 0x00, 0x00);

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

const gfx::FontList& FullscreenAppListAppTitleFont() {
  // The max line height of app titles while the fullscreen launcher is enabled,
  // which is determined by the sizes of app tile views, its paddings, and the
  // icon.
  constexpr int kFullscreenAppTitleMaxLineHeight = 16;

  // The font for app titles while the fullscreen launcher is enabled. We're
  // getting the largest font that doesn't exceed
  // |kFullscreenAppTitleMaxLineHeight|.
  // Note: we resize the font to 1px larger, otherwise it looks too small.
  static const gfx::FontList kFullscreenAppListAppTitleFont =
      ui::ResourceBundle::GetSharedInstance()
          .GetFontList(ui::ResourceBundle::LargeFont)
          .DeriveWithHeightUpperBound(kFullscreenAppTitleMaxLineHeight)
          .DeriveWithSizeDelta(1);
  return kFullscreenAppListAppTitleFont;
}

}  // namespace app_list
