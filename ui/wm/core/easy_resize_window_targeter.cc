// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/easy_resize_window_targeter.h"

#include <algorithm>

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
namespace {

// Returns an insets whose values are all negative or 0. Any positive value is
// forced to 0.
gfx::Insets InsetsWithOnlyNegativeValues(const gfx::Insets& insets) {
  if (insets.top() > 0 || insets.left() > 0 || insets.right() > 0 ||
      insets.bottom() > 0) {
    // See TODO at call site.
    NOTIMPLEMENTED_LOG_ONCE();
  }
  return gfx::Insets(std::min(0, insets.top()), std::min(0, insets.left()),
                     std::min(0, insets.bottom()), std::min(0, insets.right()));
}

}  // namespace

EasyResizeWindowTargeter::EasyResizeWindowTargeter(
    aura::Window* container,
    const gfx::Insets& mouse_extend,
    const gfx::Insets& touch_extend)
    : container_(container) {
  DCHECK(container_);
  SetInsets(mouse_extend, touch_extend);
}

EasyResizeWindowTargeter::~EasyResizeWindowTargeter() {}

void EasyResizeWindowTargeter::OnSetInsets(
    const gfx::Insets& last_mouse_extend,
    const gfx::Insets& last_touch_extend) {
  if (aura::Env::GetInstance()->mode() != aura::Env::Mode::MUS)
    return;

  // Mus only accepts 0 or negative values, force all values to fit that.
  // TODO: figure out how best to deal with positive values, see
  // http://crbug.com/775223
  const gfx::Insets effective_last_mouse_extend =
      InsetsWithOnlyNegativeValues(last_mouse_extend);
  const gfx::Insets effective_last_touch_extend =
      InsetsWithOnlyNegativeValues(last_touch_extend);
  const gfx::Insets effective_mouse_extend =
      InsetsWithOnlyNegativeValues(mouse_extend());
  const gfx::Insets effective_touch_extend =
      InsetsWithOnlyNegativeValues(touch_extend());
  if (effective_last_touch_extend == effective_touch_extend &&
      effective_last_mouse_extend == effective_mouse_extend) {
    return;
  }

  aura::WindowPortMus::Get(container_)
      ->SetExtendedHitRegionForChildren(effective_mouse_extend,
                                        effective_touch_extend);
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
