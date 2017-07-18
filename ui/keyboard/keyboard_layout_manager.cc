// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_layout_manager.h"

#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
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

  // Request to change the bounds of child window (AKA the virtual keyboard
  // window) should change the container window first. Then the child window is
  // resized and covers the container window. Note the child's bound is only set
  // in OnWindowResized.
  gfx::Rect new_bounds = requested_bounds;
  if (controller_->keyboard_mode() == FULL_WIDTH) {
    const gfx::Rect& window_bounds =
        controller_->GetContainerWindow()->GetRootWindow()->bounds();
    new_bounds.set_y(window_bounds.height() - new_bounds.height());
    // If shelf is positioned on the left side of screen, x is not 0. In
    // FULL_WIDTH mode, the virtual keyboard should always align with the left
    // edge of the screen. So manually set x to 0 here.
    new_bounds.set_x(0);
    new_bounds.set_width(window_bounds.width());
  }
  // Keyboard bounds should only be reset when it actually changes. Otherwise
  // it interrupts the initial animation of showing the keyboard. Described in
  // crbug.com/356753.
  gfx::Rect old_bounds = contents_window_->GetTargetBounds();
  aura::Window::ConvertRectToTarget(
      contents_window_, contents_window_->GetRootWindow(), &old_bounds);
  if (new_bounds == old_bounds)
    return;

  ui::LayerAnimator* animator =
      controller_->GetContainerWindow()->layer()->GetAnimator();
  // Stops previous animation if a window resize is requested during animation.
  if (animator->is_animating())
    animator->StopAnimating();

  controller_->GetContainerWindow()->SetBounds(new_bounds);
  SetChildBoundsDirect(contents_window_, gfx::Rect(new_bounds.size()));

  const bool container_had_size = old_bounds.height() != 0;
  const bool child_has_size = child->bounds().height() != 0;

  if (!container_had_size && child_has_size && controller_->show_on_resize()) {
    // The window height is set to 0 initially or before switch to an IME in a
    // different extension. Virtual keyboard window may wait for this bounds
    // change to correctly animate in.
    if (controller_->keyboard_locked()) {
      // Do not move the keyboard to another display after switch to an IME in a
      // different extension.
      const int64_t display_id =
          display::Screen::GetScreen()
              ->GetDisplayNearestWindow(controller_->GetContainerWindow())
              .id();
      controller_->ShowKeyboardInDisplay(display_id);
    } else {
      controller_->ShowKeyboard(false /* lock */);
    }
  } else {
    if (!container_had_size && child_has_size &&
        !controller_->keyboard_visible()) {
      // When the child is layed out, the controller is not shown, but showing
      // is not desired, this is indicative that the pre-load has completed.
      controller_->NotifyContentsLoadingComplete();
    }

    if (controller_->keyboard_mode() == FULL_WIDTH) {
      // We need to send out this notification only if keyboard is visible since
      // the contents window is resized even if keyboard is hidden.
      if (controller_->keyboard_visible())
        controller_->NotifyContentsBoundsChanging(new_bounds);
    } else if (controller_->keyboard_mode() == FLOATING) {
      controller_->NotifyContentsBoundsChanging(gfx::Rect());
    }
  }
}

}  // namespace keyboard
