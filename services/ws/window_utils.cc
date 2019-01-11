// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/window_utils.h"

#include "services/ws/proxy_window.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace ws {

bool IsLocationInNonClientArea(const aura::Window* window,
                               const gfx::Point& location) {
  const ProxyWindow* proxy_window = ProxyWindow::GetMayBeNull(window);
  if (!proxy_window || !proxy_window->IsTopLevel())
    return false;

  // Locations inside bounds but within the resize insets count as non-client
  // area. Locations outside the bounds, assume it's in extended hit test area,
  // which is non-client area.
  ui::WindowShowState window_state =
      window->GetProperty(aura::client::kShowStateKey);
  if ((window->GetProperty(aura::client::kResizeBehaviorKey) &
       ws::mojom::kResizeBehaviorCanResize) &&
      (window_state != ui::WindowShowState::SHOW_STATE_MAXIMIZED) &&
      (window_state != ui::WindowShowState::SHOW_STATE_FULLSCREEN)) {
    int resize_handle_size =
        window->GetProperty(aura::client::kResizeHandleInset);
    gfx::Rect non_handle_area(window->bounds().size());
    non_handle_area.Inset(gfx::Insets(resize_handle_size));
    if (!non_handle_area.Contains(location))
      return true;
  }

  gfx::Rect client_area(window->bounds().size());
  client_area.Inset(proxy_window->client_area());
  if (client_area.Contains(location))
    return false;

  for (const auto& rect : proxy_window->additional_client_areas()) {
    if (rect.Contains(location))
      return false;
  }
  return true;
}

}  // namespace ws
