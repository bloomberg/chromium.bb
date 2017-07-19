// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_layout_manager.h"

#include "ui/compositor/layer_animator.h"
#include "ui/display/screen.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"

namespace keyboard {

// Overridden from aura::LayoutManager
void KeyboardLayoutManager::OnWindowResized() {
  if (contents_window_) {
    gfx::Rect container_bounds = controller_->GetContainerWindow()->bounds();
    // Always align container window and keyboard window.
    if (controller_->keyboard_mode() == FULL_WIDTH) {
      SetChildBounds(contents_window_, gfx::Rect(container_bounds.size()));
    } else {
      SetChildBoundsDirect(contents_window_,
                           gfx::Rect(container_bounds.size()));
    }
  }
}

void KeyboardLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  DCHECK(!contents_window_);
  contents_window_ = child;
  if (controller_->keyboard_mode() == FULL_WIDTH) {
    controller_->GetContainerWindow()->SetBounds(gfx::Rect());
  } else if (controller_->keyboard_mode() == FLOATING) {
    controller_->GetContainerWindow()->SetBounds(child->bounds());
    SetChildBoundsDirect(contents_window_, gfx::Rect(child->bounds().size()));
  }
}

void KeyboardLayoutManager::SetChildBounds(aura::Window* child,
                                           const gfx::Rect& requested_bounds) {
  DCHECK(child == contents_window_);

  TRACE_EVENT0("vk", "KeyboardLayoutSetChildBounds");

  // Request to change the bounds of the contents window
  // should change the container window first. Then the contents window is
  // resized and covers the container window. Note the contents' bound is only
  // set in OnWindowResized.

  const aura::Window* root_window =
      controller_->GetContainerWindow()->GetRootWindow();
  gfx::Rect new_bounds = requested_bounds;
  if (controller_->keyboard_mode() == FULL_WIDTH) {
    // Honors only the height of the request bounds
    const gfx::Rect& window_bounds = root_window->bounds();
    new_bounds.set_y(window_bounds.height() - requested_bounds.height());
    // If shelf is positioned on the left side of screen, x is not 0. In
    // FULL_WIDTH mode, the virtual keyboard should always align with the left
    // edge of the screen. So manually set x to 0 here.
    new_bounds.set_x(0);
    new_bounds.set_width(window_bounds.width());
  }
  // Containar bounds should only be reset when the contents window bounds
  // actually change. Otherwise it interrupts the initial animation of showing
  // the keyboard. Described in crbug.com/356753.
  gfx::Rect old_bounds = contents_window_->GetTargetBounds();
  aura::Window::ConvertRectToTarget(contents_window_, root_window, &old_bounds);
  if (new_bounds == old_bounds)
    return;

  SetChildBoundsDirect(contents_window_, gfx::Rect(new_bounds.size()));
  const bool contents_loaded =
      old_bounds.height() == 0 && new_bounds.height() > 0;

  controller_->SetContainerBounds(new_bounds, contents_loaded);
}

}  // namespace keyboard
