// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/public/easy_resize_window_targeter.h"

#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rect.h"

namespace wm {

EasyResizeWindowTargeter::EasyResizeWindowTargeter(
    aura::Window* container,
    const gfx::Insets& mouse_extend,
    const gfx::Insets& touch_extend)
    : container_(container),
      mouse_extend_(mouse_extend),
      touch_extend_(touch_extend) {
}

EasyResizeWindowTargeter::~EasyResizeWindowTargeter() {
}

bool EasyResizeWindowTargeter::EventLocationInsideBounds(
    aura::Window* window,
    const ui::LocatedEvent& event) const {
  if (ShouldUseExtendedBounds(window)) {
    gfx::RectF bounds(window->bounds());
    gfx::Transform transform = window->layer()->transform();
    transform.TransformRect(&bounds);
    if (event.IsTouchEvent() || event.IsGestureEvent()) {
      bounds.Inset(touch_extend_);
    } else {
      bounds.Inset(mouse_extend_);
    }

    // Note that |event|'s location is in the |container_|'s coordinate system,
    // as is |bounds|.
    return bounds.Contains(event.location());
  }
  return WindowTargeter::EventLocationInsideBounds(window, event);
}

bool EasyResizeWindowTargeter::ShouldUseExtendedBounds(
    const aura::Window* window) const {
  // Use the extended bounds only for immediate child windows of |container_|.
  // Use the default targetter otherwise.
  if (window->parent() != container_)
    return false;

  aura::client::TransientWindowClient* transient_window_client =
      aura::client::GetTransientWindowClient();
  return !transient_window_client ||
      !transient_window_client->GetTransientParent(window) ||
      transient_window_client->GetTransientParent(window) == container_;
}

}  // namespace wm
