// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_CONSTANTS_H_
#define UI_APP_LIST_APP_LIST_CONSTANTS_H_

#include <stddef.h>

#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/app_list/app_list_export.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/shadow_value.h"

namespace app_list {

APP_LIST_EXPORT extern const SkColor kContentsBackgroundColor;
APP_LIST_EXPORT extern const SkColor kSearchBoxBackgroundDefault;

APP_LIST_EXPORT extern const SkColor kSearchTextColor;

APP_LIST_EXPORT extern const SkColor kLabelBackgroundColor;
APP_LIST_EXPORT extern const SkColor kBottomSeparatorColor;
APP_LIST_EXPORT extern const SkColor kBottomSeparatorColorFullScreen;

APP_LIST_EXPORT extern const SkColor kDialogSeparatorColor;

APP_LIST_EXPORT extern const SkColor kHighlightedColor;
APP_LIST_EXPORT extern const SkColor kSelectedColor;
APP_LIST_EXPORT extern const SkColor kGridSelectedColor;
APP_LIST_EXPORT extern const SkColor kAnswerCardSelectedColor;

APP_LIST_EXPORT extern const SkColor kPagerHoverColor;
APP_LIST_EXPORT extern const SkColor kPagerNormalColor;
APP_LIST_EXPORT extern const SkColor kPagerSelectedColor;

APP_LIST_EXPORT extern const SkColor kResultBorderColor;
APP_LIST_EXPORT extern const SkColor kResultDefaultTextColor;
APP_LIST_EXPORT extern const SkColor kResultDimmedTextColor;
APP_LIST_EXPORT extern const SkColor kResultURLTextColor;

APP_LIST_EXPORT extern const SkColor kGridTitleColor;
APP_LIST_EXPORT extern const SkColor kGridTitleColorFullscreen;

APP_LIST_EXPORT extern const int kGridTileWidth;
APP_LIST_EXPORT extern const int kGridTileHeight;
APP_LIST_EXPORT extern const int kGridTileSpacing;
APP_LIST_EXPORT extern const int kGridIconTopPadding;
APP_LIST_EXPORT extern const int kGridTitleSpacing;
APP_LIST_EXPORT extern const int kGridTitleHorizontalPadding;
APP_LIST_EXPORT extern const int kGridSelectedSize;
APP_LIST_EXPORT extern const int kGridSelectedCornerRadius;

APP_LIST_EXPORT extern const SkColor kFolderTitleColor;
APP_LIST_EXPORT extern const SkColor kFolderTitleHintTextColor;
APP_LIST_EXPORT extern const SkColor kFolderBubbleColor;
APP_LIST_EXPORT extern const SkColor kFolderBubbleColorFullScreen;
APP_LIST_EXPORT extern const SkColor kFolderShadowColor;
APP_LIST_EXPORT extern const float kFolderBubbleOpacity;
APP_LIST_EXPORT extern const float kFolderBubbleRadius;
APP_LIST_EXPORT extern const float kFolderBubbleOffsetY;

APP_LIST_EXPORT extern const SkColor kCardBackgroundColor;
APP_LIST_EXPORT extern const SkColor kCardBackgroundColorFullscreen;

APP_LIST_EXPORT extern const int kPageTransitionDurationInMs;
APP_LIST_EXPORT extern const int kPageTransitionDurationDampening;
APP_LIST_EXPORT extern const int kOverscrollPageTransitionDurationMs;
APP_LIST_EXPORT extern const int kFolderTransitionInDurationMs;
APP_LIST_EXPORT extern const int kFolderTransitionOutDurationMs;
APP_LIST_EXPORT extern const int kCustomPageCollapsedHeight;
APP_LIST_EXPORT extern const gfx::Tween::Type kFolderFadeInTweenType;
APP_LIST_EXPORT extern const gfx::Tween::Type kFolderFadeOutTweenType;

APP_LIST_EXPORT extern const int kPreferredCols;
APP_LIST_EXPORT extern const int kPreferredColsFullscreen;
APP_LIST_EXPORT extern const int kPreferredRows;
APP_LIST_EXPORT extern const int kPreferredRowsFullscreen;
APP_LIST_EXPORT extern const int kGridIconDimension;

APP_LIST_EXPORT extern const int kAppBadgeIconSize;
APP_LIST_EXPORT extern const int kBadgeBackgroundRadius;

APP_LIST_EXPORT extern const int kListIconSize;
APP_LIST_EXPORT extern const int kListIconSizeFullscreen;
APP_LIST_EXPORT extern const int kListBadgeIconSize;
APP_LIST_EXPORT extern const int kListBadgeIconSizeFullscreen;
APP_LIST_EXPORT extern const int kListBadgeIconOffsetX;
APP_LIST_EXPORT extern const int kListBadgeIconOffsetY;
APP_LIST_EXPORT extern const int kTileIconSize;

APP_LIST_EXPORT extern const SkColor kIconColor;

APP_LIST_EXPORT extern const float kDragDropAppIconScale;
APP_LIST_EXPORT extern const int kDragDropAppIconScaleTransitionInMs;

APP_LIST_EXPORT extern const size_t kNumStartPageTiles;
APP_LIST_EXPORT extern const size_t kNumStartPageTilesFullscreen;
APP_LIST_EXPORT extern const size_t kMaxSearchResults;

APP_LIST_EXPORT extern const size_t kExpandArrowTopPadding;

APP_LIST_EXPORT extern const int kReorderDroppingCircleRadius;

APP_LIST_EXPORT extern const int kAppsGridPadding;
APP_LIST_EXPORT extern const int kAppsGridLeftRightPaddingFullscreen;
APP_LIST_EXPORT extern const int kBottomSeparatorLeftRightPaddingFullScreen;
APP_LIST_EXPORT extern const int kBottomSeparatorBottomPaddingFullScreen;
APP_LIST_EXPORT extern const int kSearchBoxPadding;
APP_LIST_EXPORT extern const int kSearchBoxTopPadding;
APP_LIST_EXPORT extern const int kSearchBoxPeekingBottomPadding;
APP_LIST_EXPORT extern const int kSearchBoxFullscreenBottomPadding;
APP_LIST_EXPORT extern const int kSearchBoxBorderCornerRadiusFullscreen;
APP_LIST_EXPORT extern const int kSearchBoxPreferredHeight;

APP_LIST_EXPORT extern const int kPeekingAppListHeight;
APP_LIST_EXPORT extern const int kShelfSize;

APP_LIST_EXPORT extern const size_t kMaxFolderItems;
APP_LIST_EXPORT extern const size_t kMaxFolderItemsFullscreen;
APP_LIST_EXPORT extern const size_t kNumFolderTopItems;
APP_LIST_EXPORT extern const size_t kMaxFolderNameChars;

APP_LIST_EXPORT extern const ui::ResourceBundle::FontStyle kItemTextFontStyle;

// The different ways that the app list can transition from PEEKING to
// FULLSCREEN_ALL_APPS. These values are written to logs.  New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
enum AppListPeekingToFullscreenSource {
  kSwipe = 0,
  kExpandArrow = 1,
  kMousepadScroll = 2,
  kMousewheelScroll = 3,
  kMaxPeekingToFullscreen = 4,
};

// The different ways the app list can be shown. These values are written to
// logs.  New enum values can be added, but existing enums must never be
// renumbered or deleted and reused.
enum AppListShowSource {
  kSearchKey = 0,
  kShelfButton = 1,
  kSwipeFromShelf = 2,
  kMaxAppListToggleMethod = 3,
};

// The two versions of folders. These values are written to logs.  New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
enum AppListFolderOpened {
  kOldFolders = 0,
  kFullscreenAppListFolders = 1,
  kMaxFolderOpened = 2,
};

// The valid AppListState transitions. These values are written to logs.  New
// enum values can be added, but existing enums must never be renumbered or
// deleted and reused. If adding a state transition, add it to the switch
// statement in AppListView::GetAppListStateTransitionSource.
enum AppListStateTransitionSource {
  kFullscreenAllAppsToClosed = 0,
  kFullscreenAllAppsToFullscreenSearch = 1,
  kFullscreenAllAppsToPeeking = 2,
  kFullscreenSearchToClosed = 3,
  kFullscreenSearchToFullscreenAllApps = 4,
  kHalfToClosed = 5,
  KHalfToFullscreenSearch = 6,
  kHalfToPeeking = 7,
  kPeekingToClosed = 8,
  kPeekingToFullscreenAllApps = 9,
  kPeekingToHalf = 10,
  kMaxAppListStateTransition = 11,
};

// The different ways to change pages in the app list's app grid. These values
// are written to logs.  New enum values can be added, but existing enums must
// never be renumbered or deleted and reused.
enum AppListPageSwitcherSource {
  kTouchPageIndicator = 0,
  kClickPageIndicator = 1,
  kSwipeAppGrid = 2,
  kFlingAppGrid = 3,
  kMouseWheelScroll = 4,
  kMousePadScroll = 5,
  kDragAppToBorder = 6,
  kMaxAppListPageSwitcherSource = 7,
};

APP_LIST_EXPORT extern const char kAppListAppLaunched[];
APP_LIST_EXPORT extern const char kAppListAppLaunchedFullscreen[];
APP_LIST_EXPORT extern const char kAppListStateTransitionSourceHistogram[];
APP_LIST_EXPORT extern const char kAppListPageSwitcherSourceHistogram[];
APP_LIST_EXPORT extern const char kAppListFolderOpenedHistogram[];
APP_LIST_EXPORT extern const char kAppListPeekingToFullscreenHistogram[];
APP_LIST_EXPORT extern const char kAppListToggleMethodHistogram[];
APP_LIST_EXPORT extern const char kPageOpenedHistogram[];

APP_LIST_EXPORT extern const char kSearchResultOpenDisplayTypeHistogram[];
APP_LIST_EXPORT extern const char kSearchQueryLength[];
APP_LIST_EXPORT extern const char kSearchResultDistanceFromOrigin[];

APP_LIST_EXPORT extern const int kSearchTileHeight;

APP_LIST_EXPORT extern const int kSearchIconSize;
APP_LIST_EXPORT extern const SkColor kDefaultSearchboxColor;

// Returns the shadow values for a view at |z_height|.
APP_LIST_EXPORT gfx::ShadowValue GetShadowForZHeight(int z_height);

APP_LIST_EXPORT const gfx::ShadowValues& IconStartShadows();
APP_LIST_EXPORT const gfx::ShadowValues& IconEndShadows();

APP_LIST_EXPORT const gfx::FontList& FullscreenAppListAppTitleFont();

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_CONSTANTS_H_
