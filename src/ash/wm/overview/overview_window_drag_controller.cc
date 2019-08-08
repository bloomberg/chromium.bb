// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_window_drag_controller.h"

#include <memory>

#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_util.h"
#include "base/numerics/ranges.h"
#include "ui/aura/window.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The amount of distance from the start of drag the item needs to be dragged
// vertically for it to be closed on release.
constexpr float kDragToCloseDistanceThresholdDp = 160.f;

// The minimum distance that will be considered as a drag event.
constexpr float kMinimumDragDistanceDp = 5.f;
// Items dragged to within |kDistanceFromEdgeDp| of the screen will get snapped
// even if they have not moved by |kMinimumDragDistanceDp|.
constexpr float kDistanceFromEdgeDp = 16.f;
// The minimum distance that an item must be moved before it is snapped. This
// prevents accidental snaps.
constexpr float kMinimumDragToSnapDistanceDp = 96.f;
// The minimum distance that an item must be moved before it is considered a
// drag event, if the drag starts in one of the snap regions.
constexpr float kMinimumDragDistanceAlreadyInSnapRegionDp = 48.f;

// Flings with less velocity than this will not close the dragged item.
constexpr float kFlingToCloseVelocityThreshold = 2000.f;
constexpr float kItemMinOpacity = 0.4f;

// The UMA histogram that records presentation time for window dragging
// operation in overview mode.
constexpr char kOverviewWindowDragHistogram[] =
    "Ash.Overview.WindowDrag.PresentationTime.TabletMode";
constexpr char kOverviewWindowDragMaxLatencyHistogram[] =
    "Ash.Overview.WindowDrag.PresentationTime.MaxLatency.TabletMode";

void UnpauseOcclusionTracker() {
  Shell::Get()->overview_controller()->UnpauseOcclusionTracker(
      kOcclusionPauseDurationForDragMs);
}

}  // namespace

OverviewWindowDragController::OverviewWindowDragController(
    OverviewSession* overview_session)
    : overview_session_(overview_session),
      split_view_controller_(Shell::Get()->split_view_controller()) {}

OverviewWindowDragController::~OverviewWindowDragController() = default;

void OverviewWindowDragController::InitiateDrag(
    OverviewItem* item,
    const gfx::PointF& location_in_screen) {
  item_ = item;
  initial_event_location_ = location_in_screen;
  initial_centerpoint_ = item_->target_bounds().CenterPoint();
  if (ShouldAllowSplitView()) {
    started_in_snap_region_ =
        GetSnapPosition(location_in_screen) != SplitViewController::NONE;
  }
  current_drag_behavior_ = DragBehavior::kUndefined;
  Shell::Get()->overview_controller()->PauseOcclusionTracker();
  DCHECK(!presentation_time_recorder_);

  presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
      item_->root_window()->layer()->GetCompositor(),
      kOverviewWindowDragHistogram, kOverviewWindowDragMaxLatencyHistogram);
}

void OverviewWindowDragController::Drag(const gfx::PointF& location_in_screen) {
  if (!did_move_) {
    gfx::Vector2dF distance = location_in_screen - initial_event_location_;
    // Do not start dragging if the distance from |location_in_screen| to
    // |initial_event_location_| is not greater than |kMinimumDragDistanceDp|.
    if (std::abs(distance.x()) < kMinimumDragDistanceDp &&
        std::abs(distance.y()) < kMinimumDragDistanceDp) {
      return;
    }

    if (std::abs(distance.x()) < std::abs(distance.y())) {
      current_drag_behavior_ = DragBehavior::kDragToClose;
      original_opacity_ = item_->GetOpacity();
      overview_session_->GetGridWithRootWindow(item_->root_window())
          ->StartNudge(item_);
      did_move_ = true;
    } else if (ShouldAllowSplitView()) {
      StartSplitViewDragMode(location_in_screen);
    }
  }

  // Update the state based on the drag behavior.
  if (current_drag_behavior_ == DragBehavior::kDragToClose) {
    // Update |item_|'s opacity based on its distance. |item_|'s x coordinate
    // should not change while in drag to close state.
    float val = std::abs(location_in_screen.y() - initial_event_location_.y()) /
                kDragToCloseDistanceThresholdDp;
    overview_session_->GetGridWithRootWindow(item_->root_window())
        ->UpdateNudge(item_, val);
    val = base::ClampToRange(val, 0.f, 1.f);
    float opacity = original_opacity_;
    if (opacity > kItemMinOpacity)
      opacity = original_opacity_ - val * (original_opacity_ - kItemMinOpacity);
    item_->SetOpacity(opacity);
  } else if (current_drag_behavior_ == DragBehavior::kDragToSnap) {
    UpdateDragIndicatorsAndOverviewGrid(location_in_screen);
  }

  if (presentation_time_recorder_)
    presentation_time_recorder_->RequestNext();

  // Update the dragged |item_|'s bounds accordingly. The distance from the new
  // location to the new centerpoint should be the same it was initially. Do not
  // update x bounds if dragging to close.
  gfx::RectF bounds(item_->target_bounds());
  const gfx::PointF centerpoint =
      location_in_screen - (initial_event_location_ - initial_centerpoint_);
  if (current_drag_behavior_ == DragBehavior::kDragToSnap)
    bounds.set_x(centerpoint.x() - bounds.width() / 2.f);
  bounds.set_y(centerpoint.y() - bounds.height() / 2.f);
  item_->SetBounds(bounds, OVERVIEW_ANIMATION_NONE);
}

void OverviewWindowDragController::CompleteDrag(
    const gfx::PointF& location_in_screen) {
  // Update the split view divider bar stuatus if necessary. The divider bar
  // should be placed above the dragged window after drag ends. Note here the
  // passed paramters |snap_position_| and |location_in_screen| won't be used in
  // this function for this case, but they are passed in as placeholders.
  if (ShouldAllowSplitView()) {
    split_view_controller_->OnWindowDragEnded(
        item_->GetWindow(), snap_position_,
        gfx::ToRoundedPoint(location_in_screen));
  }

  // Update window grid bounds and |snap_position_| in case the screen
  // orientation was changed.
  if (current_drag_behavior_ == DragBehavior::kDragToSnap) {
    UpdateDragIndicatorsAndOverviewGrid(location_in_screen);
    overview_session_->SetSplitViewDragIndicatorsIndicatorState(
        IndicatorState::kNone, gfx::Point());
  }

  if (!did_move_) {
    ActivateDraggedWindow();
  } else if (current_drag_behavior_ == DragBehavior::kDragToClose) {
    // If we are in drag to close mode close the window if it has been dragged
    // enough, otherwise reposition it and set its opacity back to its original
    // value.
    overview_session_->GetGridWithRootWindow(item_->root_window())->EndNudge();
    if (std::abs((location_in_screen - initial_event_location_).y()) >
        kDragToCloseDistanceThresholdDp) {
      item_->AnimateAndCloseWindow(
          (location_in_screen - initial_event_location_).y() < 0);
    } else {
      item_->SetOpacity(original_opacity_);
      overview_session_->PositionWindows(/*animate=*/true);
    }
  } else if (current_drag_behavior_ == DragBehavior::kDragToSnap) {
    overview_session_->RemoveDropTargetForDraggingFromOverview(item_);
    // If the window was dragged around but should not be snapped, move it back
    // to overview window grid.
    if (!ShouldUpdateDragIndicatorsOrSnap(location_in_screen) ||
        snap_position_ == SplitViewController::NONE) {
      item_->set_should_restack_on_animation_end(true);
      overview_session_->PositionWindows(/*animate=*/true);
    } else {
      SnapWindow(snap_position_);
    }
  }
  did_move_ = false;
  item_ = nullptr;
  current_drag_behavior_ = DragBehavior::kNoDrag;
  UnpauseOcclusionTracker();
  presentation_time_recorder_.reset();
}

void OverviewWindowDragController::StartSplitViewDragMode(
    const gfx::PointF& location_in_screen) {
  DCHECK(ShouldAllowSplitView());

  overview_session_->AddDropTargetForDraggingFromOverview(item_);
  item_->ScaleUpSelectedItem(
      OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW);

  did_move_ = true;
  current_drag_behavior_ = DragBehavior::kDragToSnap;
  overview_session_->SetSplitViewDragIndicatorsIndicatorState(
      CanSnapInSplitview(item_->GetWindow()) ? IndicatorState::kDragArea
                                             : IndicatorState::kCannotSnap,
      gfx::ToRoundedPoint(location_in_screen));

  // Update the split view divider bar status if necessary. If splitview is
  // active when dragging the overview window, the split divider bar should be
  // placed below the dragged window during dragging.
  if (ShouldAllowSplitView())
    split_view_controller_->OnWindowDragStarted(item_->GetWindow());
}

void OverviewWindowDragController::Fling(const gfx::PointF& location_in_screen,
                                         float velocity_x,
                                         float velocity_y) {
  if (current_drag_behavior_ == DragBehavior::kDragToClose ||
      current_drag_behavior_ == DragBehavior::kUndefined) {
    if (std::abs(velocity_y) > kFlingToCloseVelocityThreshold) {
      if (ShouldAllowSplitView()) {
        split_view_controller_->OnWindowDragEnded(
            item_->GetWindow(), snap_position_,
            gfx::ToRoundedPoint(location_in_screen));
      }

      item_->AnimateAndCloseWindow(
          (location_in_screen - initial_event_location_).y() < 0);
      did_move_ = false;
      item_ = nullptr;
      current_drag_behavior_ = DragBehavior::kNoDrag;
      UnpauseOcclusionTracker();
      return;
    }
  }

  // If the fling velocity was not high enough, or flings should be ignored,
  // treat it as a scroll end event.
  CompleteDrag(location_in_screen);
}

void OverviewWindowDragController::ActivateDraggedWindow() {
  // If no drag was initiated (e.g., a click/tap on the overview window),
  // activate the window. If the split view is active and has a left window,
  // snap the current window to right. If the split view is active and has a
  // right window, snap the current window to left. If split view is active
  // and the selected window cannot be snapped, exit splitview and activate
  // the selected window, and also exit the overview.
  SplitViewController::State split_state = split_view_controller_->state();
  if (!ShouldAllowSplitView() || split_state == SplitViewController::NO_SNAP) {
    overview_session_->SelectWindow(item_);
  } else if (CanSnapInSplitview(item_->GetWindow())) {
    SnapWindow(split_state == SplitViewController::LEFT_SNAPPED
                   ? SplitViewController::RIGHT
                   : SplitViewController::LEFT);
  } else {
    split_view_controller_->EndSplitView();
    overview_session_->SelectWindow(item_);
    split_view_controller_->ShowAppCannotSnapToast();
  }
  current_drag_behavior_ = DragBehavior::kNoDrag;
  UnpauseOcclusionTracker();
}

void OverviewWindowDragController::ResetGesture() {
  if (current_drag_behavior_ == DragBehavior::kDragToSnap) {
    overview_session_->RemoveDropTargetForDraggingFromOverview(item_);
    overview_session_->SetSplitViewDragIndicatorsIndicatorState(
        IndicatorState::kNone, gfx::Point());
  }
  overview_session_->PositionWindows(/*animate=*/true);
  // This function gets called after a long press release, which bypasses
  // CompleteDrag but stops dragging as well, so reset |item_|.
  item_ = nullptr;
  current_drag_behavior_ = DragBehavior::kNoDrag;
  UnpauseOcclusionTracker();
}

void OverviewWindowDragController::ResetOverviewSession() {
  overview_session_ = nullptr;
}

void OverviewWindowDragController::UpdateDragIndicatorsAndOverviewGrid(
    const gfx::PointF& location_in_screen) {
  DCHECK(ShouldAllowSplitView());
  if (!ShouldUpdateDragIndicatorsOrSnap(location_in_screen))
    return;

  // Attempt to update the drag indicators and move the window grid only if the
  // window is snappable.
  if (!CanSnapInSplitview(item_->GetWindow())) {
    snap_position_ = SplitViewController::NONE;
    return;
  }

  snap_position_ = GetSnapPosition(location_in_screen);
  IndicatorState indicator_state;
  switch (snap_position_) {
    case SplitViewController::NONE:
      indicator_state = CanSnapInSplitview(item_->GetWindow())
                            ? IndicatorState::kDragArea
                            : IndicatorState::kCannotSnap;
      break;
    case SplitViewController::LEFT:
      indicator_state = IndicatorState::kPreviewAreaLeft;
      break;
    case SplitViewController::RIGHT:
      indicator_state = IndicatorState::kPreviewAreaRight;
      break;
  }
  overview_session_->RearrangeDuringDrag(item_->GetWindow(), location_in_screen,
                                         indicator_state);
  overview_session_->SetSplitViewDragIndicatorsIndicatorState(indicator_state,
                                                              gfx::Point());
}

bool OverviewWindowDragController::ShouldUpdateDragIndicatorsOrSnap(
    const gfx::PointF& event_location) {
  auto snap_position = GetSnapPosition(event_location);
  const bool inverted = !IsCurrentScreenOrientationPrimary();
  // Note: in some orientations SplitViewController::LEFT is not physically on
  // the left/top.
  const bool on_the_left_or_top =
      (!inverted && snap_position == SplitViewController::LEFT) ||
      (inverted && snap_position == SplitViewController::RIGHT);

  // Snap the window if it is less than |kDistanceFromEdgeDp| from the edge.
  const bool landscape = IsCurrentScreenOrientationLandscape();
  gfx::Rect area(
      screen_util::GetDisplayWorkAreaBoundsInParent(item_->GetWindow()));
  ::wm::ConvertRectToScreen(item_->GetWindow()->GetRootWindow(), &area);
  area.Inset(kDistanceFromEdgeDp, kDistanceFromEdgeDp);
  const gfx::Point event_location_i = gfx::ToRoundedPoint(event_location);
  if ((landscape && (event_location_i.x() < area.x() ||
                     event_location_i.x() > area.right())) ||
      (!landscape && (event_location_i.y() < area.y() ||
                      event_location_i.y() > area.bottom()))) {
    return true;
  }

  // The drag indicators can update or the item can snap even if the drag events
  // are in the snap region, if the event has travelled past the threshold in
  // the direction of the attempted snap region.
  const gfx::Vector2dF distance = event_location - initial_event_location_;
  // Check the x-axis distance for landscape, y-axis distance for portrait.
  const float distance_scalar = landscape ? distance.x() : distance.y();

  // If not started in a snap region, snap if the item has been dragged
  // |kMinimumDragDistanceDp|. This prevents accidental snaps.
  if (!started_in_snap_region_ &&
      std::abs(distance_scalar) > kMinimumDragToSnapDistanceDp) {
    return true;
  }

  if (snap_position == SplitViewController::NONE) {
    // If the event started in a snap region, but has since moved out set
    // |started_in_snap_region_| to false. |event_location| is guarenteed to not
    // be in a snap region so that the drag indicators are shown correctly and
    // the snap mechanism works normally for the rest of the drag.
    started_in_snap_region_ = false;
    return true;
  }

  // If the snap region is physically on the left/top side of the device, check
  // that |distance_scalar| is less than
  // -|kMinimumDragDistanceAlreadyInSnapRegionDp|. If the snap region is
  // physically on the right/bottom side of the device, check that
  // |distance_scalar| is greater than
  // |kMinimumDragDistanceAlreadyInSnapRegionDp|.
  return on_the_left_or_top
             ? distance_scalar <= -kMinimumDragDistanceAlreadyInSnapRegionDp
             : distance_scalar >= kMinimumDragDistanceAlreadyInSnapRegionDp;
}

SplitViewController::SnapPosition OverviewWindowDragController::GetSnapPosition(
    const gfx::PointF& location_in_screen) const {
  DCHECK(item_);
  DCHECK(ShouldAllowSplitView());
  gfx::Rect area(
      screen_util::GetDisplayWorkAreaBoundsInParent(item_->GetWindow()));
  ::wm::ConvertRectToScreen(item_->GetWindow()->GetRootWindow(), &area);

  const bool is_landscape = IsCurrentScreenOrientationLandscape();
  const bool is_primary = IsCurrentScreenOrientationPrimary();

  // If split view mode is active at the moment, and dragging an overview window
  // to snap it to a position that already has a snapped window in place, we
  // should show the preview window as soon as the window past the split divider
  // bar.
  if (split_view_controller_->IsSplitViewModeActive()) {
    const int position = gfx::ToRoundedInt(
        is_landscape ? location_in_screen.x() : location_in_screen.y());
    SplitViewController::SnapPosition default_snap_position =
        split_view_controller_->default_snap_position();
    // If we're trying to snap to a position that already has a snapped window:
    const bool re_snap =
        is_primary == (position < split_view_controller_->divider_position() ==
                       (default_snap_position == SplitViewController::LEFT));
    if (re_snap)
      return default_snap_position;
  }

  if (is_landscape) {
    // The window can be snapped if it reaches close enough to the screen
    // edge of the screen (on primary axis). The edge insets are a fixed ratio
    // of the screen plus some padding. This matches the drag indicators ui.
    const int screen_edge_inset_for_drag =
        area.width() * kHighlightScreenPrimaryAxisRatio +
        kHighlightScreenEdgePaddingDp;
    area.Inset(screen_edge_inset_for_drag, 0);
    if (location_in_screen.x() <= area.x()) {
      return is_primary ? SplitViewController::LEFT
                        : SplitViewController::RIGHT;
    }
    if (location_in_screen.x() >= area.right() - 1) {
      return is_primary ? SplitViewController::RIGHT
                        : SplitViewController::LEFT;
    }
    return SplitViewController::NONE;
  } else {
    const int screen_edge_inset_for_drag =
        area.height() * kHighlightScreenPrimaryAxisRatio +
        kHighlightScreenEdgePaddingDp;
    area.Inset(0, screen_edge_inset_for_drag);
    if (location_in_screen.y() <= area.y()) {
      return is_primary ? SplitViewController::LEFT
                        : SplitViewController::RIGHT;
    }
    if (location_in_screen.y() >= area.bottom() - 1) {
      return is_primary ? SplitViewController::RIGHT
                        : SplitViewController::LEFT;
    }
    return SplitViewController::NONE;
  }
}

gfx::Rect OverviewWindowDragController::GetGridBounds(
    SplitViewController::SnapPosition snap_position) {
  aura::Window* pending_snapped_window = item_->GetWindow();
  switch (snap_position) {
    case SplitViewController::NONE:
      return gfx::Rect(
          screen_util::GetDisplayWorkAreaBoundsInParentForActiveDeskContainer(
              pending_snapped_window));
    case SplitViewController::LEFT:
      return split_view_controller_->GetSnappedWindowBoundsInScreen(
          pending_snapped_window, SplitViewController::RIGHT);
    case SplitViewController::RIGHT:
      return split_view_controller_->GetSnappedWindowBoundsInScreen(
          pending_snapped_window, SplitViewController::LEFT);
  }

  NOTREACHED();
  return gfx::Rect();
}

void OverviewWindowDragController::SnapWindow(
    SplitViewController::SnapPosition snap_position) {
  DCHECK_NE(snap_position, SplitViewController::NONE);

  // |item_| will be deleted after SplitViewController::SnapWindow().
  split_view_controller_->SnapWindow(item_->GetWindow(), snap_position);
  item_ = nullptr;
}

}  // namespace ash
