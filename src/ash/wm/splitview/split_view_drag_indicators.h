// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_DRAG_INDICATORS_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_DRAG_INDICATORS_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

// Enum which contains the possible states SplitViewDragIndicators can be in.
enum class IndicatorState {
  kNone,
  // Showing both left/top and right/bottom drag guidances.
  kDragArea,

  // Showing only left/top drag guidance.
  kDragAreaLeft,

  // Showing only right/bottom drag guidance.
  kDragAreaRight,

  // Showing both left/top and right/bottom cannot drag indicators.
  kCannotSnap,

  // Showing only left/top cannot drag indicator.
  kCannotSnapLeft,

  // Showing only right/bottom cannot drag indicator.
  kCannotSnapRight,

  // Showing a left/top preview area with the same bounds as left/top snapped
  // window.
  kPreviewAreaLeft,

  // Showing a right/bottom preview area with the same bounds as right/bottom
  // snapped window.
  kPreviewAreaRight
};

// Enum which contains the indicators that SplitViewDragIndicators can display.
// Converted to a bitmask to make testing easier.
enum class IndicatorType {
  kLeftHighlight = 1,
  kLeftText = 2,
  kRightHighlight = 4,
  kRightText = 8
};

// An overlay in overview mode which guides users while they are attempting to
// enter splitview. Displays text and highlights when dragging an overview
// window. Displays a highlight of where the window will end up when an overview
// window has entered a snap region.
class ASH_EXPORT SplitViewDragIndicators {
 public:
  static bool IsPreviewAreaState(IndicatorState indicator_state);
  static bool IsLeftIndicatorState(IndicatorState indicator_state);
  static bool IsRightIndicatorState(IndicatorState indicator_state);
  static bool IsCannotSnapState(IndicatorState indicator_state);

  // Calculates whether the  preview area should physically be on the left or
  // top of the screen.
  static bool IsPreviewAreaOnLeftTopOfScreen(IndicatorState indicator_state);

  SplitViewDragIndicators();
  ~SplitViewDragIndicators();

  // Sets visiblity. The correct indicators will become visible based on the
  // split view controllers state. If |event_location| is located on a different
  // root window than |widget_|, |widget_| will reparent.
  void SetIndicatorState(IndicatorState indicator_state,
                         const gfx::Point& event_location);

  // Called by owner of this class when split view mode ends, so that this class
  // can show the drag indicators which are usually hidden in split view mode.
  // https://crbug.com/946601
  void OnSplitViewModeEnded();

  // Called by owner of this class when display bounds changes are observed, so
  // that this class can relayout accordingly.
  void OnDisplayBoundsChanged();

  bool GetIndicatorTypeVisibilityForTesting(IndicatorType type) const;

  IndicatorState current_indicator_state() const {
    return current_indicator_state_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(SplitViewDragIndicatorsTest,
                           SplitViewDragIndicatorsWidgetReparenting);
  class RotatedImageLabelView;
  class SplitViewDragIndicatorsView;

  // The root content view of |widget_|.
  SplitViewDragIndicatorsView* indicators_view_ = nullptr;

  IndicatorState current_indicator_state_ = IndicatorState::kNone;

  // The SplitViewDragIndicators widget. It covers the entire root window
  // and displays regions and text indicating where users should drag windows
  // enter split view.
  std::unique_ptr<views::Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(SplitViewDragIndicators);
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_DRAG_INDICATORS_H_
