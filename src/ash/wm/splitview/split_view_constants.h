// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_CONSTANTS_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_CONSTANTS_H_

#include "ash/ash_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// The ratio between a highlight view's primary axis, and the screens
// primary axis.
ASH_EXPORT constexpr float kHighlightScreenPrimaryAxisRatio = 0.10;

// The padding between a highlight view and the edge of the screen.
ASH_EXPORT constexpr float kHighlightScreenEdgePaddingDp = 8;

// The amount of inset to be applied on a split view label. Here horizontal and
// vertical apply to the orientation before rotation (if there is rotation).
constexpr int kSplitviewLabelHorizontalInsetDp = 16;
constexpr int kSplitviewLabelVerticalInsetDp = 8;

// The preferred height of a split view label.
constexpr int kSplitviewLabelPreferredHeightDp = 36;

// The amount of round applied to the corners of a split view label.
constexpr int kSplitviewLabelRoundRectRadiusDp = 18;

// The thickness of the divider when it is not being dragged.
constexpr int kSplitviewDividerShortSideLength = 8;

// The thickness of the divider during dragging.
constexpr int kSplitviewDividerEnlargedShortSideLength = 16;

// The time duration for the window transformation animations.
constexpr auto kSplitviewWindowTransformDuration = base::Milliseconds(250);

// The time duration for the divider animations when dragging starts and ends.
constexpr auto kSplitviewDividerSelectionStatusChangeDuration =
    base::Milliseconds(250);

// The time duration for the divider spawning animation.
constexpr auto kSplitviewDividerSpawnDuration = base::Milliseconds(100);

// The delay before the divider spawning animation.
constexpr auto kSplitviewDividerSpawnDelay = base::Milliseconds(183);

// The thickness of the divider's handler.
constexpr int kSplitviewWhiteBarShortSideLength = 2;

// The length of the divider's handler.
constexpr int kSplitviewWhiteBarLongSideLength = 16;

// The corner radius of the divider's handler.
constexpr int kSplitviewWhiteBarCornerRadius = 1;

// The radius of the circular handler when the divider is being dragged.
constexpr int kSplitviewWhiteBarRadius = 4;

// The length of the divider's handler when it spawns.
constexpr int kSplitviewWhiteBarSpawnLongSideLength = 2;

// The distance from the divider to where its handler spawns.
constexpr int kSplitviewWhiteBarSpawnUnsignedOffset = 2;

// The opacity of the drag-to-snap or cannot-snap drag indicator.
constexpr float kHighlightOpacity = 0.3f;
constexpr float kDarkLightHighlightOpacity = 0.4f;
constexpr float kDarkLightHighlightCannotSnapOpacity = 0.2f;

// The opacity of the split view snap preview area.
constexpr float kPreviewAreaHighlightOpacity = 0.18f;
constexpr float kDarkLightPreviewAreaHighlightOpacity = 0.2f;

// In portrait mode split view, if the caret in the bottom window is less than
// `kMinCaretKeyboardDist` dip above the upper bounds of the virtual keyboard,
// then we push up the bottom window above the virtual keyboard to avoid the
// input field being occluded by the virtual keyboard. The upper bounds of the
// bottom window after being pushed up cannot exceeds 1 -
// `kMinDividerPositionRatio` of screen height.
constexpr int kMinCaretKeyboardDist = 16;
constexpr float kMinDividerPositionRatio = 0.15f;

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_CONSTANTS_H_
