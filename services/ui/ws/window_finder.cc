// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/window_finder.h"

#include "base/containers/adapters.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_compositor_frame_sink_manager.h"
#include "services/ui/ws/server_window_delegate.h"
#include "services/ui/ws/window_coordinate_conversions.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/transform.h"

namespace ui {
namespace ws {
namespace {

bool IsValidWindowForEvents(ServerWindow* window) {
  ServerWindowCompositorFrameSinkManager* compositor_frame_sink_manager =
      window->compositor_frame_sink_manager();
  // Valid windows have at least one of the two surface types. Only an underlay
  // is valid as we assume the window manager will likely get the event in this
  // case.
  return compositor_frame_sink_manager &&
         compositor_frame_sink_manager->HasCompositorFrameSink();
}

bool IsLocationInNonclientArea(const ServerWindow* target,
                               const gfx::Point& location) {
  if (!target->parent())
    return false;

  gfx::Rect client_area(target->bounds().size());
  client_area.Inset(target->client_area());
  if (client_area.Contains(location))
    return false;

  for (const auto& rect : target->additional_client_areas()) {
    if (rect.Contains(location))
      return false;
  }

  return true;
}

bool FindDeepestVisibleWindowForEventsImpl(ServerWindow* window,
                                           const gfx::Point& location,
                                           DeepestWindow* deepest_window) {
  if (!window->can_accept_events()) {
    if (!IsLocationInNonclientArea(window, location) ||
        !IsValidWindowForEvents(window)) {
      return false;
    }
    deepest_window->window = window;
    deepest_window->in_non_client_area = true;
    return true;
  }

  const ServerWindow::Windows& children = window->children();
  for (ServerWindow* child : base::Reversed(children)) {
    if (!child->visible())
      continue;

    // TODO(sky): support transform.
    gfx::Point location_in_child(location.x() - child->bounds().x(),
                                 location.y() - child->bounds().y());
    gfx::Rect child_bounds(child->bounds().size());
    child_bounds.Inset(-child->extended_hit_test_region().left(),
                       -child->extended_hit_test_region().top(),
                       -child->extended_hit_test_region().right(),
                       -child->extended_hit_test_region().bottom());
    if (!child_bounds.Contains(location_in_child) ||
        (child->hit_test_mask() &&
         !child->hit_test_mask()->Contains(location_in_child))) {
      continue;
    }

    if (FindDeepestVisibleWindowForEventsImpl(child, location_in_child,
                                              deepest_window)) {
      return true;
    }
  }

  if (!IsValidWindowForEvents(window))
    return false;

  deepest_window->window = window;
  deepest_window->in_non_client_area =
      IsLocationInNonclientArea(window, location);
  return true;
}

}  // namespace

DeepestWindow FindDeepestVisibleWindowForEvents(ServerWindow* root_window,
                                                const gfx::Point& location) {
  DeepestWindow result;
  FindDeepestVisibleWindowForEventsImpl(root_window, location, &result);
  return result;
}

gfx::Transform GetTransformToWindow(ServerWindow* window) {
  gfx::Transform transform;
  ServerWindow* current = window;
  while (current->parent()) {
    transform.Translate(-current->bounds().x(), -current->bounds().y());
    current = current->parent();
  }
  return transform;
}

}  // namespace ws
}  // namespace ui
