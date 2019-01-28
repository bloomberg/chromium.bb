// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_screen.h"

#include "ui/display/display.h"
#include "ui/display/display_finder.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

namespace ui {

WaylandScreen::WaylandScreen(WaylandConnection* connection)
    : connection_(connection), weak_factory_(this) {
  DCHECK(connection_);
}

WaylandScreen::~WaylandScreen() = default;

void WaylandScreen::OnOutputAdded(uint32_t output_id) {
  display::Display new_display(output_id);
  display_list_.AddDisplay(std::move(new_display),
                           display::DisplayList::Type::NOT_PRIMARY);
}

void WaylandScreen::OnOutputRemoved(uint32_t output_id) {
  display::Display primary_display = GetPrimaryDisplay();
  if (primary_display.id() == output_id) {
    // First, set a new primary display as required by the |display_list_|. It's
    // safe to set any of the displays to be a primary one. Once the output is
    // completely removed, Wayland updates geometry of other displays. And a
    // display, which became the one to be nearest to the origin will become a
    // primary one.
    for (const auto& display : display_list_.displays()) {
      if (display.id() != output_id) {
        display_list_.AddOrUpdateDisplay(display,
                                         display::DisplayList::Type::PRIMARY);
        break;
      }
    }
  }
  display_list_.RemoveDisplay(output_id);
}

void WaylandScreen::OnOutputMetricsChanged(uint32_t output_id,
                                           const gfx::Rect& new_bounds,
                                           float device_pixel_ratio) {
  display::Display changed_display(output_id);
  changed_display.set_device_scale_factor(device_pixel_ratio);
  changed_display.set_bounds(new_bounds);
  changed_display.set_work_area(new_bounds);

  bool is_primary = false;
  display::Display display_nearest_origin =
      GetDisplayNearestPoint(gfx::Point(0, 0));
  // If bounds of the nearest to origin display are empty, it must have been the
  // very first and the same display added before.
  if (display_nearest_origin.bounds().IsEmpty()) {
    DCHECK_EQ(display_nearest_origin.id(), changed_display.id());
    is_primary = true;
  } else if (changed_display.bounds().origin() <
             display_nearest_origin.bounds().origin()) {
    // If changed display is nearer to the origin than the previous display,
    // that one must become a primary display.
    is_primary = true;
  } else if (changed_display.bounds().OffsetFromOrigin() ==
             display_nearest_origin.bounds().OffsetFromOrigin()) {
    // If changed display has the same origin as the nearest to origin display,
    // |changed_display| must become a primary one or it has already been the
    // primary one. If a user changed positions of two displays (the second at
    // x,x was set to 0,0), the second change will modify geometry of the
    // display, which used to be the one nearest to the origin.
    is_primary = true;
  }

  display_list_.UpdateDisplay(
      changed_display, is_primary ? display::DisplayList::Type::PRIMARY
                                  : display::DisplayList::Type::NOT_PRIMARY);
}

base::WeakPtr<WaylandScreen> WaylandScreen::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

const std::vector<display::Display>& WaylandScreen::GetAllDisplays() const {
  return display_list_.displays();
}

display::Display WaylandScreen::GetPrimaryDisplay() const {
  auto iter = display_list_.GetPrimaryDisplayIterator();
  DCHECK(iter != display_list_.displays().end());
  return *iter;
}

display::Display WaylandScreen::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  // TODO(msisov): implement wl_surface_listener::enter and
  // wl_surface_listener::leave for a wl_surface to know what surface the window
  // is located on.
  //
  // https://crbug.com/890271
  NOTIMPLEMENTED_LOG_ONCE();
  return GetPrimaryDisplay();
}

gfx::Point WaylandScreen::GetCursorScreenPoint() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Point();
}

gfx::AcceleratedWidget WaylandScreen::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  // It is safe to check only for focused windows and test if they contain the
  // point or not.
  auto* window = connection_->GetCurrentFocusedWindow();
  if (window && window->GetBounds().Contains(point))
    return window->GetWidget();
  return gfx::kNullAcceleratedWidget;
}

display::Display WaylandScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return *FindDisplayNearestPoint(display_list_.displays(), point);
}

display::Display WaylandScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  const display::Display* display_matching =
      display::FindDisplayWithBiggestIntersection(display_list_.displays(),
                                                  match_rect);
  if (!display_matching)
    return display::Display();
  return *display_matching;
}

void WaylandScreen::AddObserver(display::DisplayObserver* observer) {
  display_list_.AddObserver(observer);
}

void WaylandScreen::RemoveObserver(display::DisplayObserver* observer) {
  display_list_.RemoveObserver(observer);
}

}  // namespace ui
