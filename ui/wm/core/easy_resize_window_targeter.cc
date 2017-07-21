// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/easy_resize_window_targeter.h"

#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rect.h"

namespace wm {

EasyResizeWindowTargeter::EasyResizeWindowTargeter(
    aura::Window* container,
    const gfx::Insets& mouse_extend,
    const gfx::Insets& touch_extend)
    : container_(container) {
  DCHECK(container_);
  SetInsets(mouse_extend, touch_extend);
}

EasyResizeWindowTargeter::~EasyResizeWindowTargeter() {}

void EasyResizeWindowTargeter::SetInsets(const gfx::Insets& mouse_extend,
                                         const gfx::Insets& touch_extend) {
  if (mouse_extend == mouse_extend_ && touch_extend_ == touch_extend)
    return;

  mouse_extend_ = mouse_extend;
  touch_extend_ = touch_extend;
  if (aura::Env::GetInstance()->mode() != aura::Env::Mode::MUS)
    return;

  aura::WindowPortMus::Get(container_)
      ->SetExtendedHitRegionForChildren(mouse_extend, touch_extend);
}

bool EasyResizeWindowTargeter::EventLocationInsideBounds(
    aura::Window* window,
    const ui::LocatedEvent& event) const {
  if (ShouldUseExtendedBounds(window)) {
    // Note that |event|'s location is in |window|'s parent's coordinate system,
    // so convert it to |window|'s coordinate system first.
    gfx::Point point = event.location();
    if (window->parent())
      aura::Window::ConvertPointToTarget(window->parent(), window, &point);

    gfx::Rect bounds(window->bounds().size());
    if (event.IsTouchEvent() || event.IsGestureEvent())
      bounds.Inset(touch_extend_);
    else
      bounds.Inset(mouse_extend_);

    return bounds.Contains(point);
  }
  return WindowTargeter::EventLocationInsideBounds(window, event);
}

bool EasyResizeWindowTargeter::ShouldUseExtendedBounds(
    const aura::Window* window) const {
  // Use the extended bounds only for immediate child windows of |container_|.
  // Use the default targeter otherwise.
  if (window->parent() != container_)
    return false;

  // Only resizable windows benefit from the extended hit-test region.
  if ((window->GetProperty(aura::client::kResizeBehaviorKey) &
       ui::mojom::kResizeBehaviorCanResize) == 0) {
    return false;
  }

  // For transient children use extended bounds if a transient parent or if
  // transient parent's parent is a top level window in |container_|.
  aura::client::TransientWindowClient* transient_window_client =
      aura::client::GetTransientWindowClient();
  const aura::Window* transient_parent =
      transient_window_client
          ? transient_window_client->GetTransientParent(window)
          : nullptr;
  return !transient_parent || transient_parent == container_ ||
         transient_parent->parent() == container_;
}

}  // namespace wm
