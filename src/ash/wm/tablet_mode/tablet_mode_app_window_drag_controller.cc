// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_app_window_drag_controller.h"

#include "ash/shell.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/tablet_mode/tablet_mode_window_drag_delegate.h"
#include "ash/wm/window_state.h"
#include "ui/base/hit_test.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// Return the location of |event| in screen coordinates.
gfx::Point GetEventLocationInScreen(const ui::GestureEvent* event) {
  gfx::Point location_in_screen(event->location());
  ::wm::ConvertPointToScreen(static_cast<aura::Window*>(event->target()),
                             &location_in_screen);
  return location_in_screen;
}

// The drag delegate for app windows. It not only includes the logic in
// TabletModeWindowDragDelegate, but also has special logic for app windows.
class TabletModeAppWindowDragDelegate : public TabletModeWindowDragDelegate {
 public:
  TabletModeAppWindowDragDelegate() = default;
  ~TabletModeAppWindowDragDelegate() override = default;

 private:
  // TabletModeWindowDragDelegate:
  void PrepareForDraggedWindow(const gfx::Point& location_in_screen) override {
    wm::GetWindowState(dragged_window_)
        ->CreateDragDetails(location_in_screen, HTCLIENT,
                            ::wm::WINDOW_MOVE_SOURCE_TOUCH);
  }
  void UpdateForDraggedWindow(const gfx::Point& location_in_screen) override {}
  void EndingForDraggedWindow(
      wm::WmToplevelWindowEventHandler::DragResult result,
      const gfx::Point& location_in_screen) override {
    wm::GetWindowState(dragged_window_)->DeleteDragDetails();
  }

  DISALLOW_COPY_AND_ASSIGN(TabletModeAppWindowDragDelegate);
};

}  // namespace

TabletModeAppWindowDragController::TabletModeAppWindowDragController()
    : drag_delegate_(std::make_unique<TabletModeAppWindowDragDelegate>()) {
  display::Screen::GetScreen()->AddObserver(this);
}

TabletModeAppWindowDragController::~TabletModeAppWindowDragController() {
  display::Screen::GetScreen()->RemoveObserver(this);
}

bool TabletModeAppWindowDragController::DragWindowFromTop(
    ui::GestureEvent* event) {
  previous_location_in_screen_ = GetEventLocationInScreen(event);
  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN)
    return StartWindowDrag(event);

  if (!drag_delegate_->dragged_window())
    return false;

  if (event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    UpdateWindowDrag(event);
    return true;
  }

  if (event->type() == ui::ET_GESTURE_SCROLL_END ||
      event->type() == ui::ET_SCROLL_FLING_START) {
    EndWindowDrag(event, wm::WmToplevelWindowEventHandler::DragResult::SUCCESS);
    return true;
  }

  EndWindowDrag(event, wm::WmToplevelWindowEventHandler::DragResult::REVERT);
  return false;
}

bool TabletModeAppWindowDragController::StartWindowDrag(
    ui::GestureEvent* event) {
  views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
      static_cast<aura::Window*>(event->target()));
  if (!widget)
    return false;

  drag_delegate_->StartWindowDrag(widget->GetNativeWindow(),
                                  GetEventLocationInScreen(event));
  return true;
}

void TabletModeAppWindowDragController::UpdateWindowDrag(
    ui::GestureEvent* event) {
  // Update the dragged window's tranform during dragging.
  drag_delegate_->ContinueWindowDrag(
      GetEventLocationInScreen(event),
      TabletModeWindowDragDelegate::UpdateDraggedWindowType::UPDATE_TRANSFORM);
}

bool TabletModeAppWindowDragController::ShouldFlingIntoOverview(
    ui::GestureEvent* event) {
  if (event->type() != ui::ET_SCROLL_FLING_START)
    return false;

  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  const gfx::Point location_in_screen = GetEventLocationInScreen(event);
  const IndicatorState indicator_state =
      drag_delegate_->GetIndicatorState(location_in_screen);
  const bool is_landscape =
      split_view_controller->IsCurrentScreenOrientationLandscape();
  const float velocity = is_landscape ? event->details().velocity_x()
                                      : event->details().velocity_y();

  // Drop the window into overview if fling with large enough velocity to the
  // opposite snap position when preview area is shown.
  if (split_view_controller->IsCurrentScreenOrientationPrimary()) {
    if (indicator_state == IndicatorState::kPreviewAreaLeft)
      return velocity > kFlingToOverviewFromSnappingAreaThreshold;
    else if (indicator_state == IndicatorState::kPreviewAreaRight)
      return -velocity > kFlingToOverviewFromSnappingAreaThreshold;
  } else {
    if (indicator_state == IndicatorState::kPreviewAreaLeft)
      return -velocity > kFlingToOverviewFromSnappingAreaThreshold;
    else if (indicator_state == IndicatorState::kPreviewAreaRight)
      return velocity > kFlingToOverviewFromSnappingAreaThreshold;
  }

  const SplitViewController::State snap_state = split_view_controller->state();
  const int end_position =
      is_landscape ? location_in_screen.x() : location_in_screen.y();
  // Fling the window when splitview is active. Since each snapping area in
  // splitview has a corresponding snap position. Fling the window to the
  // opposite position of the area's snap position with large enough velocity
  // should drop the window into overview grid.
  if (snap_state == SplitViewController::LEFT_SNAPPED ||
      snap_state == SplitViewController::RIGHT_SNAPPED) {
    return end_position > split_view_controller->divider_position()
               ? -velocity > kFlingToOverviewFromSnappingAreaThreshold
               : velocity > kFlingToOverviewFromSnappingAreaThreshold;
  }

  // Consider only the velocity_y if splitview is not active and preview area is
  // not shown.
  return event->details().velocity_y() > kFlingToOverviewThreshold;
}

void TabletModeAppWindowDragController::EndWindowDrag(
    ui::GestureEvent* event,
    wm::WmToplevelWindowEventHandler::DragResult result) {
  if (ShouldFlingIntoOverview(event)) {
    DCHECK(Shell::Get()->window_selector_controller()->IsSelecting());
    Shell::Get()->window_selector_controller()->window_selector()->AddItem(
        drag_delegate_->dragged_window(), /*reposition=*/true,
        /*animate=*/false);
  }
  drag_delegate_->EndWindowDrag(result, GetEventLocationInScreen(event));
}

void TabletModeAppWindowDragController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (!drag_delegate_->dragged_window() || !(metrics & DISPLAY_METRIC_ROTATION))
    return;

  display::Display current_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          drag_delegate_->dragged_window());
  if (display.id() != current_display.id())
    return;

  drag_delegate_->EndWindowDrag(
      wm::WmToplevelWindowEventHandler::DragResult::REVERT,
      previous_location_in_screen_);
}

}  // namespace ash
