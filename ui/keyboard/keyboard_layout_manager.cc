// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_layout_manager.h"

#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_proxy.h"
#include "ui/keyboard/keyboard_util.h"

namespace keyboard {

// Overridden from aura::LayoutManager
void KeyboardLayoutManager::OnWindowResized() {
  if (keyboard_ && !controller_->proxy()->resizing_from_contents())
    ResizeKeyboardToDefault(keyboard_);
}

void KeyboardLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  DCHECK(!keyboard_);
  keyboard_ = child;
  ResizeKeyboardToDefault(keyboard_);
}

void KeyboardLayoutManager::SetChildBounds(aura::Window* child,
                                           const gfx::Rect& requested_bounds) {
  // SetChildBounds can be invoked by resizing from the container or by
  // resizing from the contents (through window.resizeTo call in JS).
  // The flag resizing_from_contents() is used to determine the source of the
  // resize.
  if (controller_->proxy()->resizing_from_contents()) {
    controller_->NotifyKeyboardBoundsChanging(requested_bounds);
    SetChildBoundsDirect(child, requested_bounds);
  }
}

void KeyboardLayoutManager::ResizeKeyboardToDefault(aura::Window* child) {
  gfx::Rect keyboard_bounds = DefaultKeyboardBoundsFromWindowBounds(
      controller_->GetContainerWindow()->bounds());
  SetChildBoundsDirect(child, keyboard_bounds);
}

}  // namespace keyboard
