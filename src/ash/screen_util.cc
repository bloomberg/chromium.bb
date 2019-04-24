// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/screen_util.h"

#include "ash/display/display_configuration_controller.h"
#include "ash/display/mirror_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace screen_util {

gfx::Rect GetMaximizedWindowBoundsInParent(aura::Window* window) {
  if (Shelf::ForWindow(window)->shelf_widget())
    return GetDisplayWorkAreaBoundsInParent(window);
  return GetDisplayBoundsInParent(window);
}

gfx::Rect GetDisplayBoundsInParent(aura::Window* window) {
  gfx::Rect result =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).bounds();
  ::wm::ConvertRectFromScreen(window->parent(), &result);
  return result;
}

gfx::Rect GetFullscreenWindowBoundsInParent(aura::Window* window) {
  gfx::Rect result = GetDisplayBoundsInParent(window);

  Shelf* shelf = Shelf::ForWindow(window);
  ShelfLayoutManager* shelf_layout_manager =
      shelf ? shelf->shelf_layout_manager() : nullptr;
  if (shelf_layout_manager) {
    result.Inset(0,
                 shelf_layout_manager->accessibility_panel_height() +
                     shelf_layout_manager->docked_magnifier_height(),
                 0, 0);
  }

  return result;
}

gfx::Rect GetDisplayWorkAreaBoundsInParent(aura::Window* window) {
  gfx::Rect result =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).work_area();
  ::wm::ConvertRectFromScreen(window->parent(), &result);
  return result;
}

gfx::Rect GetDisplayWorkAreaBoundsInParentForLockScreen(aura::Window* window) {
  gfx::Rect bounds = Shelf::ForWindow(window)->GetUserWorkAreaBounds();
  ::wm::ConvertRectFromScreen(window->parent(), &bounds);
  return bounds;
}

gfx::Rect GetDisplayWorkAreaBoundsInParentForDefaultContainer(
    aura::Window* window) {
  aura::Window* root_window = window->GetRootWindow();
  return GetDisplayWorkAreaBoundsInParent(
      root_window->GetChildById(kShellWindowId_DefaultContainer));
}

gfx::Rect GetDisplayWorkAreaBoundsInScreenForDefaultContainer(
    aura::Window* window) {
  gfx::Rect bounds =
      GetDisplayWorkAreaBoundsInParentForDefaultContainer(window);
  ::wm::ConvertRectToScreen(window->GetRootWindow(), &bounds);
  return bounds;
}

gfx::Rect GetDisplayBoundsWithShelf(aura::Window* window) {
  if (!Shell::Get()->display_manager()->IsInUnifiedMode())
    return window->GetRootWindow()->bounds();

  // In Unified Mode, the display that should contain the shelf depends on the
  // current shelf alignment.
  const display::Display shelf_display =
      Shell::Get()
          ->display_configuration_controller()
          ->GetPrimaryMirroringDisplayForUnifiedDesktop();
  DCHECK_NE(shelf_display.id(), display::kInvalidDisplayId);
  gfx::RectF shelf_display_screen_bounds(shelf_display.bounds());

  // Transform the bounds back to the unified host's coordinates.
  auto inverse_unified_transform =
      window->GetRootWindow()->GetHost()->GetInverseRootTransform();
  inverse_unified_transform.TransformRect(&shelf_display_screen_bounds);

  return gfx::ToEnclosingRect(shelf_display_screen_bounds);
}

gfx::Rect SnapBoundsToDisplayEdge(const gfx::Rect& bounds,
                                  const aura::Window* window) {
  const aura::WindowTreeHost* host = window->GetHost();
  if (!host)
    return bounds;

  const float dsf = host->device_scale_factor();
  const gfx::Rect display_bounds_in_pixel = host->GetBoundsInPixels();
  const gfx::Rect display_bounds_in_dip = window->GetRootWindow()->bounds();
  const gfx::Rect bounds_in_pixel = gfx::ScaleToEnclosedRect(bounds, dsf);

  // Adjusts |bounds| such that the scaled enclosed bounds are atleast as big as
  // the scaled enclosing unadjusted bounds.
  gfx::Rect snapped_bounds = bounds;
  if ((display_bounds_in_dip.width() == bounds.width() &&
       bounds_in_pixel.width() != display_bounds_in_pixel.width()) ||
      (bounds.right() == display_bounds_in_dip.width() &&
       bounds_in_pixel.right() != display_bounds_in_pixel.width())) {
    snapped_bounds.Inset(0, 0, -1, 0);
    DCHECK_GE(gfx::ScaleToEnclosedRect(snapped_bounds, dsf).right(),
              gfx::ScaleToEnclosingRect(bounds, dsf).right());
  }
  if ((display_bounds_in_dip.height() == bounds.height() &&
       bounds_in_pixel.height() != display_bounds_in_pixel.height()) ||
      (bounds.bottom() == display_bounds_in_dip.height() &&
       bounds_in_pixel.bottom() != display_bounds_in_pixel.height())) {
    snapped_bounds.Inset(0, 0, 0, -1);
    DCHECK_GE(gfx::ScaleToEnclosedRect(snapped_bounds, dsf).bottom(),
              gfx::ScaleToEnclosingRect(bounds, dsf).bottom());
  }

  return snapped_bounds;
}

}  // namespace screen_util

}  // namespace ash
