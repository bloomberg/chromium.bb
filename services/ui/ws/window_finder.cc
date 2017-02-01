// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/window_finder.h"

#include "base/containers/adapters.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_delegate.h"
#include "services/ui/ws/window_coordinate_conversions.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/transform.h"

namespace ui {
namespace ws {
namespace {

bool IsLocationInNonclientArea(const ServerWindow* target,
                               const gfx::Point& location) {
  if (!target->parent() || target->bounds().size().IsEmpty())
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
  // The non-client area takes precedence over descendants, as otherwise the
  // user would likely not be able to hit the non-client area as it's common
  // for descendants to go into the non-client area.
  if (IsLocationInNonclientArea(window, location)) {
    deepest_window->window = window;
    deepest_window->in_non_client_area = true;
    return true;
  }
  const mojom::EventTargetingPolicy event_targeting_policy =
      window->event_targeting_policy();

  if (event_targeting_policy == ui::mojom::EventTargetingPolicy::NONE)
    return false;

  if (event_targeting_policy ==
          mojom::EventTargetingPolicy::TARGET_AND_DESCENDANTS ||
      event_targeting_policy == mojom::EventTargetingPolicy::DESCENDANTS_ONLY) {
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
  }

  if (event_targeting_policy == mojom::EventTargetingPolicy::DESCENDANTS_ONLY)
    return false;

  deepest_window->window = window;
  deepest_window->in_non_client_area = false;
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
