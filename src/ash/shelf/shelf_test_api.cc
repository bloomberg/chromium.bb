// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_test_api.h"

#include "ash/public/cpp/shelf_ui_info.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ui/compositor/layer_animator.h"

namespace {

ash::Shelf* GetShelf() {
  return ash::Shell::Get()->GetPrimaryRootWindowController()->shelf();
}

ash::ShelfWidget* GetShelfWidget() {
  return ash::Shell::GetRootWindowControllerWithDisplayId(
             display::Screen::GetScreen()->GetPrimaryDisplay().id())
      ->shelf()
      ->shelf_widget();
}

ash::HotseatWidget* GetHotseatWidget() {
  return GetShelfWidget()->hotseat_widget();
}

ash::ScrollableShelfView* GetScrollableShelfView() {
  return GetHotseatWidget()->scrollable_shelf_view();
}

}  // namespace

namespace ash {

ShelfTestApi::ShelfTestApi() = default;
ShelfTestApi::~ShelfTestApi() = default;

bool ShelfTestApi::IsVisible() {
  return GetShelf()->shelf_layout_manager()->IsVisible();
}

bool ShelfTestApi::IsAlignmentBottomLocked() {
  return GetShelf()->alignment() == ShelfAlignment::kBottomLocked;
}

views::View* ShelfTestApi::GetHomeButton() {
  return GetShelfWidget()->navigation_widget()->GetHomeButton();
}

bool ShelfTestApi::HasLoginShelfGestureHandler() const {
  return GetShelfWidget()->login_shelf_gesture_controller_for_testing();
}

ScrollableShelfInfo ShelfTestApi::GetScrollableShelfInfoForState(
    const ShelfState& state) {
  const auto* scrollable_shelf_view = GetScrollableShelfView();

  ScrollableShelfInfo info;
  info.main_axis_offset =
      scrollable_shelf_view->CalculateMainAxisScrollDistance();
  info.page_offset = scrollable_shelf_view->CalculatePageScrollingOffsetInAbs(
      scrollable_shelf_view->layout_strategy_);
  info.left_arrow_bounds =
      scrollable_shelf_view->left_arrow()->GetBoundsInScreen();
  info.right_arrow_bounds =
      scrollable_shelf_view->right_arrow()->GetBoundsInScreen();
  info.is_animating = scrollable_shelf_view->during_scroll_animation_;
  info.is_overflow = (scrollable_shelf_view->layout_strategy_ !=
                      ScrollableShelfView::kNotShowArrowButtons);
  info.is_shelf_widget_animating =
      GetShelfWidget()->GetLayer()->GetAnimator()->is_animating();

  const ShelfView* const shelf_view = scrollable_shelf_view->shelf_view_;
  for (int i = shelf_view->first_visible_index();
       i <= shelf_view->last_visible_index(); ++i) {
    info.icons_bounds_in_screen.push_back(
        shelf_view->view_model()->view_at(i)->GetBoundsInScreen());
  }

  // Calculates the target offset only when |scroll_distance| is specified.
  if (state.scroll_distance != 0.f) {
    const float target_offset =
        scrollable_shelf_view->CalculateTargetOffsetAfterScroll(
            info.main_axis_offset, state.scroll_distance);
    info.target_main_axis_offset = target_offset;
  }

  return info;
}

HotseatInfo ShelfTestApi::GetHotseatInfo() {
  HotseatInfo info;
  auto* hotseat_widget = GetHotseatWidget();
  info.is_animating =
      hotseat_widget->GetNativeView()->layer()->GetAnimator()->is_animating();
  info.hotseat_state = hotseat_widget->state();

  const gfx::Rect shelf_widget_bounds =
      GetShelf()->shelf_widget()->GetWindowBoundsInScreen();
  info.swipe_up.swipe_start_location = shelf_widget_bounds.CenterPoint();

  // The swipe distance is small enough to avoid the window drag from shelf.
  const int swipe_distance = hotseat_widget->GetHotseatFullDragAmount() / 2;

  gfx::Point swipe_end_location = info.swipe_up.swipe_start_location;
  swipe_end_location.set_y(swipe_end_location.y() - swipe_distance);
  info.swipe_up.swipe_end_location = swipe_end_location;

  return info;
}

}  // namespace ash
