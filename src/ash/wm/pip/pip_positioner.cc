// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_positioner.h"

#include <algorithm>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/work_area_insets.h"
#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {
const int kPipWorkAreaInsetsDp = 8;
const float kPipDismissMovementProportion = 1.5f;

// A property key to store whether the a window should be ignored for PIP
// collision detection. For example, StatusBubble windows.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIgnoreForPipCollisionDetection, false)

enum { GRAVITY_LEFT, GRAVITY_RIGHT, GRAVITY_TOP, GRAVITY_BOTTOM };

// Returns the result of adjusting |bounds| according to |gravity| inside
// |region|.
gfx::Rect GetAdjustedBoundsByGravity(const gfx::Rect& bounds,
                                     const gfx::Rect& region,
                                     int gravity) {
  switch (gravity) {
    case GRAVITY_LEFT:
      return gfx::Rect(region.x(), bounds.y(), bounds.width(), bounds.height());
    case GRAVITY_RIGHT:
      return gfx::Rect(region.right() - bounds.width(), bounds.y(),
                       bounds.width(), bounds.height());
    case GRAVITY_TOP:
      return gfx::Rect(bounds.x(), region.y(), bounds.width(), bounds.height());
    case GRAVITY_BOTTOM:
      return gfx::Rect(bounds.x(), region.bottom() - bounds.height(),
                       bounds.width(), bounds.height());
    default:
      NOTREACHED();
  }
  return bounds;
}

// Returns the gravity that would make |bounds| fall to the closest edge of
// |region|. If |bounds| is outside of |region| then it will return the gravity
// as if |bounds| had fallen outside of |region|. See the below diagram for what
// the gravity regions look like for a point.
//  \  TOP /
//   \____/ R
// L |\  /| I
// E | \/ | G
// F | /\ | H
// T |/__\| T
//   /    \
//  /BOTTOM
int GetGravityToClosestEdge(const gfx::Rect& bounds, const gfx::Rect& region) {
  const gfx::Insets insets = region.InsetsFrom(bounds);
  int minimum_edge_dist = std::min(insets.left(), insets.right());
  minimum_edge_dist = std::min(minimum_edge_dist, insets.top());
  minimum_edge_dist = std::min(minimum_edge_dist, insets.bottom());

  if (insets.left() == minimum_edge_dist) {
    return GRAVITY_LEFT;
  } else if (insets.right() == minimum_edge_dist) {
    return GRAVITY_RIGHT;
  } else if (insets.top() == minimum_edge_dist) {
    return GRAVITY_TOP;
  } else {
    return GRAVITY_BOTTOM;
  }
}

gfx::Rect ComputeCollisionRectFromBounds(const gfx::Rect& bounds,
                                         const aura::Window* parent) {
  gfx::Rect collision_rect = bounds;
  ::wm::ConvertRectToScreen(parent, &collision_rect);
  collision_rect.Inset(-kPipWorkAreaInsetsDp, -kPipWorkAreaInsetsDp);
  return collision_rect;
}

bool ShouldIgnoreWindowForCollision(const aura::Window* window) {
  return window->GetProperty(kIgnoreForPipCollisionDetection);
}

std::vector<gfx::Rect> CollectCollisionRects(const display::Display& display) {
  std::vector<gfx::Rect> rects;
  auto* root_window = Shell::GetRootWindowForDisplayId(display.id());
  if (root_window) {
    // Check SettingsBubbleContainer windows.
    auto* settings_bubble_container =
        root_window->GetChildById(kShellWindowId_SettingBubbleContainer);
    for (auto* window : settings_bubble_container->children()) {
      if (!window->IsVisible() && !window->GetTargetBounds().IsEmpty())
        continue;
      if (ShouldIgnoreWindowForCollision(window))
        continue;
      // Use the target bounds in case an animation is in progress.
      rects.push_back(ComputeCollisionRectFromBounds(window->GetTargetBounds(),
                                                     window->parent()));
    }

    // Check auto-hide shelf, which isn't included normally in the work area:
    auto* shelf = Shelf::ForWindow(root_window);
    auto* shelf_window = shelf->GetWindow();
    if (shelf->IsVisible() && !ShouldIgnoreWindowForCollision(shelf_window))
      rects.push_back(ComputeCollisionRectFromBounds(
          shelf_window->GetTargetBounds(), shelf_window->parent()));
  }

  auto* keyboard_controller = keyboard::KeyboardController::Get();
  if (keyboard_controller->IsEnabled() &&
      keyboard_controller->GetActiveContainerType() ==
          keyboard::mojom::ContainerType::kFloating &&
      keyboard_controller->GetRootWindow() == root_window &&
      !keyboard_controller->visual_bounds_in_screen().IsEmpty() &&
      !ShouldIgnoreWindowForCollision(
          keyboard_controller->GetKeyboardWindow())) {
    // TODO(shend): visual_bounds_in_screen should return the bounds in screen
    // coordinates. See crbug.com/943446.
    rects.push_back(ComputeCollisionRectFromBounds(
        keyboard_controller->visual_bounds_in_screen(),
        /*parent=*/root_window));
  }

  return rects;
}

// Finds the candidate points |center| could be moved to. One of these points
// is guaranteed to be the minimum distance to move |center| to make it not
// intersect any of the rectangles in |collision_rects|.
std::vector<gfx::Point> CollectCandidatePoints(
    const gfx::Point& center,
    const std::vector<gfx::Rect>& collision_rects) {
  std::vector<gfx::Point> candidate_points;
  candidate_points.reserve(4 * collision_rects.size() * collision_rects.size() +
                           4 * collision_rects.size() + 1);
  // There are four cases for how the PIP window will move.
  // Case #1: Touches 0 obstacles. This corresponds to not moving.
  // Case #2: Touches 1 obstacle.
  //   The PIP window will be touching one edge of the obstacle.
  // Case #3: Touches 2 obstacles.
  //   The PIP window will be touching one horizontal and one vertical edge
  //   from two different obstacles.
  // Case #4: Touches more than 2 obstacles. This is handled in case #3.

  // Case #2.
  // Rects include the left and top edges, so subtract 1 for those.
  // Prioritize horizontal movement before vertical movement.
  bool intersects = false;
  for (const auto& rectA : collision_rects) {
    intersects = intersects || rectA.Contains(center);
    candidate_points.emplace_back(rectA.x() - 1, center.y());
    candidate_points.emplace_back(rectA.right(), center.y());
    candidate_points.emplace_back(center.x(), rectA.y() - 1);
    candidate_points.emplace_back(center.x(), rectA.bottom());
  }
  // Case #1: Touching zero obstacles, so don't move the PIP window.
  if (!intersects)
    return {};

  // Case #3: Add candidate points corresponding to each pair of horizontal
  // and vertical edges.
  for (const auto& rectA : collision_rects) {
    for (const auto& rectB : collision_rects) {
      // Continuing early here isn't necessary but makes fewer candidate points.
      if (&rectA == &rectB)
        continue;
      candidate_points.emplace_back(rectA.x() - 1, rectB.y() - 1);
      candidate_points.emplace_back(rectA.x() - 1, rectB.bottom());
      candidate_points.emplace_back(rectA.right(), rectB.y() - 1);
      candidate_points.emplace_back(rectA.right(), rectB.bottom());
    }
  }
  return candidate_points;
}

// Finds the candidate point with the shortest distance to |center| that is
// inside |work_area| and does not intersect any gfx::Rect in |rects|.
gfx::Point ComputeBestCandidatePoint(const gfx::Point& center,
                                     const gfx::Rect& work_area,
                                     const std::vector<gfx::Rect>& rects) {
  auto candidate_points = CollectCandidatePoints(center, rects);
  int64_t best_dist = -1;
  gfx::Point best_point = center;
  for (const auto& point : candidate_points) {
    if (!work_area.Contains(point))
      continue;
    bool viable = true;
    for (const auto& rect : rects) {
      if (rect.Contains(point)) {
        viable = false;
        break;
      }
    }
    if (!viable)
      continue;
    int64_t dist = (point - center).LengthSquared();
    if (dist < best_dist || best_dist == -1) {
      best_dist = dist;
      best_point = point;
    }
  }
  return best_point;
}

}  // namespace

gfx::Rect PipPositioner::GetMovementArea(const display::Display& display) {
  gfx::Rect work_area =
      WorkAreaInsets::ForWindow(Shell::GetRootWindowForDisplayId(display.id()))
          ->user_work_area_bounds();

  work_area.Inset(kPipWorkAreaInsetsDp, kPipWorkAreaInsetsDp);
  return work_area;
}

gfx::Rect PipPositioner::GetBoundsForDrag(const display::Display& display,
                                          const gfx::Rect& bounds_in_screen) {
  gfx::Rect drag_bounds = bounds_in_screen;
  drag_bounds.AdjustToFit(GetMovementArea(display));
  drag_bounds = AvoidObstacles(display, drag_bounds);
  return drag_bounds;
}

gfx::Rect PipPositioner::GetRestingPosition(const display::Display& display,
                                            const gfx::Rect& bounds_in_screen) {
  gfx::Rect resting_bounds = bounds_in_screen;
  gfx::Rect area = GetMovementArea(display);
  resting_bounds.AdjustToFit(area);

  const int gravity = GetGravityToClosestEdge(resting_bounds, area);
  resting_bounds = GetAdjustedBoundsByGravity(resting_bounds, area, gravity);
  return AvoidObstacles(display, resting_bounds);
}

gfx::Rect PipPositioner::GetDismissedPosition(
    const display::Display& display,
    const gfx::Rect& bounds_in_screen) {
  gfx::Rect work_area = GetMovementArea(display);
  const int gravity = GetGravityToClosestEdge(bounds_in_screen, work_area);
  // Allow the bounds to move at most |kPipDismissMovementProportion| of the
  // length of the bounds in the direction of movement.
  gfx::Rect bounds_movement_area = bounds_in_screen;
  bounds_movement_area.Inset(
      -bounds_in_screen.width() * kPipDismissMovementProportion,
      -bounds_in_screen.height() * kPipDismissMovementProportion);
  gfx::Rect dismissed_bounds = GetAdjustedBoundsByGravity(
      bounds_in_screen, bounds_movement_area, gravity);

  // If the PIP window isn't close enough to the edge of the screen, don't slide
  // it out.
  return work_area.Intersects(dismissed_bounds) ? bounds_in_screen
                                                : dismissed_bounds;
}

gfx::Rect PipPositioner::GetPositionAfterMovementAreaChange(
    wm::WindowState* window_state) {
  // Restore to previous bounds if we have them. This lets us move the PIP
  // window back to its original bounds after transient movement area changes,
  // like the keyboard popping up and pushing the PIP window up.
  gfx::Rect bounds_in_screen = window_state->window()->GetBoundsInScreen();
  // If the client changes the window size, don't try to resize it back for
  // restore.
  if (window_state->HasRestoreBounds()) {
    bounds_in_screen.set_origin(
        window_state->GetRestoreBoundsInScreen().origin());
  }
  return GetRestingPosition(window_state->GetDisplay(), bounds_in_screen);
}

void PipPositioner::MarkWindowAsIgnoredForCollisionDetection(
    aura::Window* window) {
  window->SetProperty(kIgnoreForPipCollisionDetection, true);
}

gfx::Rect PipPositioner::AvoidObstacles(const display::Display& display,
                                        const gfx::Rect& bounds_in_screen) {
  gfx::Rect work_area = GetMovementArea(display);
  auto rects = CollectCollisionRects(display);
  // The worst case for this should be: floating keyboard + one system tray +
  // the volume shifter, which is 3 windows.
  DCHECK(rects.size() <= 15) << "PipPositioner::AvoidObstacles is N^3 and "
                                "should be optimized if there are a lot of "
                                "windows. Please see crrev.com/c/1221427 for a "
                                "description of an N^2 algorithm.";
  return AvoidObstaclesInternal(work_area, rects, bounds_in_screen);
}

gfx::Rect PipPositioner::AvoidObstaclesInternal(
    const gfx::Rect& work_area,
    const std::vector<gfx::Rect>& rects,
    const gfx::Rect& bounds_in_screen) {
  gfx::Rect inset_work_area = work_area;

  // For even sized bounds, there is no 'center' integer point, so we need
  // to adjust the obstacles and work area to account for this.
  inset_work_area.Inset(
      bounds_in_screen.width() / 2, bounds_in_screen.height() / 2,
      (bounds_in_screen.width() - 1) / 2, (bounds_in_screen.height() - 1) / 2);
  std::vector<gfx::Rect> inset_rects(rects);
  for (auto& rect : inset_rects) {
    // Reduce the collision resolution problem from rectangles-rectangle
    // resolution to rectangles-point resolution, by expanding each obstacle
    // by |bounds_in_screen| size.
    rect.Inset(-(bounds_in_screen.width() - 1) / 2,
               -(bounds_in_screen.height() - 1) / 2,
               -bounds_in_screen.width() / 2, -bounds_in_screen.height() / 2);
  }

  gfx::Point moved_center = ComputeBestCandidatePoint(
      bounds_in_screen.CenterPoint(), inset_work_area, inset_rects);
  gfx::Rect moved_bounds = bounds_in_screen;
  moved_bounds.Offset(moved_center - bounds_in_screen.CenterPoint());
  return moved_bounds;
}

}  // namespace ash
