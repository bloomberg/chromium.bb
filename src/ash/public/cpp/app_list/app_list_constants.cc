// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_constants.h"

#include "build/build_config.h"
#include "ui/gfx/color_palette.h"

namespace app_list {

const SkColor kContentsBackgroundColor = SkColorSetRGB(0xF2, 0xF2, 0xF2);

const SkColor kLabelBackgroundColor = SK_ColorTRANSPARENT;

// Color of bottom separtor under folder title (12% white) in full screen mode.
const SkColor kBottomSeparatorColor = SkColorSetARGB(0x1F, 0xFF, 0xFF, 0xFF);

// The color of the separator used inside dialogs in the app list.
const SkColor kDialogSeparatorColor = SkColorSetRGB(0xD1, 0xD1, 0xD1);

// The mouse hover colour (3% black).
const SkColor kHighlightedColor = SkColorSetARGB(8, 0, 0, 0);
// The keyboard select color for grid views, which are on top of a black shield
// view for new design (12% white).
const SkColor kGridSelectedColor = SkColorSetARGB(0x1F, 0xFF, 0xFF, 0xFF);
// Selection color for answer card (3% black).
const SkColor kAnswerCardSelectedColor = SkColorSetARGB(0x08, 0x00, 0x00, 0x00);

const SkColor kPagerHoverColor = SkColorSetRGB(0xB4, 0xB4, 0xB4);
const SkColor kPagerNormalColor = SkColorSetRGB(0xE2, 0xE2, 0xE2);
const SkColor kPagerSelectedColor = SkColorSetRGB(0x46, 0x8F, 0xFC);

// The preferred height for horizontal pages. For page #01 in the apps grid, it
// includes the top/bottom 24px padding. For page #02 and all the followings,
// it includes top 24px padding and bottom 56px padding.
const int kHorizontalPagePreferredHeight = 623;

const SkColor kFolderTitleColor = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kFolderTitleHintTextColor = SkColorSetRGB(0xA0, 0xA0, 0xA0);
// Color of the folder bubble shadow.
const SkColor kFolderShadowColor = SkColorSetRGB(0xBF, 0xBF, 0xBF);
const float kFolderBubbleOpacity = 0.12f;

const SkColor kCardBackgroundColor = SkColorSetRGB(0xFA, 0xFA, 0xFC);

// Duration in milliseconds for page transition.
const int kPageTransitionDurationInMs = 250;

// Dampening value for PaginationModel's SlideAnimation.
const int kPageTransitionDurationDampening = 3;

// Duration in milliseconds for over scroll page transition.
const int kOverscrollPageTransitionDurationMs = 50;

// Duration in milliseconds for fading in the target page when opening
// or closing a folder, and the duration for the top folder icon animation
// for flying in or out the folder.
const int kFolderTransitionInDurationMs = 250;

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

// The number of apps shown in the start page app grid.
const int kNumStartPageTiles = 5;

// Maximum number of results to show in the launcher Search UI.
const size_t kMaxSearchResults = 6;

// Radius of the circle, in which if entered, show re-order preview.
const int kReorderDroppingCircleRadius = 35;

// The padding around the outside of the apps grid (sides).
const int kAppsGridPadding = 24;

// The left and right side padding of the apps grid. The
// space is used for page switcher and its padding on the right side. Left side
// should have the same space to keep the apps grid horizontally centered.
const int kAppsGridLeftRightPadding = 40;

// The left and right padding from the folder name bottom separator to the edge
// of the left or right edge of the left most and right most app item.
const int kBottomSeparatorLeftRightPadding = 24;

// The bottom padding from the bottom separator to the top of the app item.
const int kBottomSeparatorBottomPadding = 24;

// Bottom padding of search box in peeking state.
const int kSearchBoxPeekingBottomPadding = 12;

// Bottom padding of search box.
const int kSearchBoxBottomPadding = 24;

// Max pages allowed in a folder.
const size_t kMaxFolderPages = 3;

// Max items per page allowed in a folder.
const size_t kMaxFolderItemsPerPage = 16;

// Maximum length of the folder name in chars.
const size_t kMaxFolderNameChars = 28;

// Font style for app item labels.
const ui::ResourceBundle::FontStyle kItemTextFontStyle =
    ui::ResourceBundle::SmallFont;

// Range of the height of centerline above screen bottom that all apps should
// change opacity. NOTE: this is used to change page switcher's opacity as well.
const float kAllAppsOpacityStartPx = 8.0f;
const float kAllAppsOpacityEndPx = 144.0f;

// Delta applied to font size of all AppListSearchResult titles.
const int kSearchResultTitleTextSizeDelta = 2;

// Font style for AppListSearchResultTileItemViews that are not suggested apps.
const ui::ResourceBundle::FontStyle kSearchResultTitleFontStyle =
    ui::ResourceBundle::BaseFont;

// The UMA histogram that logs usage of suggested and regular apps.
const char kAppListAppLaunched[] = "Apps.AppListAppLaunched";

// The UMA histogram that logs usage of suggested and regular apps while the
// fullscreen launcher is enabled.
const char kAppListAppLaunchedFullscreen[] =
    "Apps.AppListAppLaunchedFullscreen";

// The UMA histogram that logs different ways to move an app in app list's apps
// grid.
const char kAppListAppMovingType[] = "Apps.AppListAppMovingType";

// The UMA histogram that logs the creation time of the AppListView.
const char kAppListCreationTimeHistogram[] = "Apps.AppListCreationTime";

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

// The UMA histogram that logs how many apps users have in folders.
const char kNumberOfAppsInFoldersHistogram[] =
    "Apps.AppsInFolders.FullscreenAppListEnabled";

// The UMA histogram that logs how many folders users have.
const char kNumberOfFoldersHistogram[] = "Apps.NumberOfFolders";

// The UMA histogram that logs how many pages users have in top level apps grid.
const char kNumberOfPagesHistogram[] = "Apps.NumberOfPages";

// The UMA histogram that logs how many pages with empty slots users have in top
// level apps grid.
const char kNumberOfPagesNotFullHistogram[] = "Apps.NumberOfPagesNotFull";

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

}  // namespace app_list
