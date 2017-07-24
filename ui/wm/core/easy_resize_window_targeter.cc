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

bool EasyResizeWindowTargeter::GetHitTestRects(aura::Window* window,
                                               gfx::Rect* rect_mouse,
                                               gfx::Rect* rect_touch) const {
  if (!ShouldUseExtendedBounds(window))
    return WindowTargeter::GetHitTestRects(window, rect_mouse, rect_touch);

  DCHECK(rect_mouse);
  DCHECK(rect_touch);
  *rect_mouse = *rect_touch = gfx::Rect(window->bounds());
  rect_mouse->Inset(mouse_extend_);
  rect_touch->Inset(touch_extend_);
  return true;
}

bool EasyResizeWindowTargeter::EventLocationInsideBounds(
    aura::Window* target,
    const ui::LocatedEvent& event) const {
  return WindowTargeter::EventLocationInsideBounds(target, event);
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
