// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_controller.h"

#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/gfx/rect.h"
#include "ui/keyboard/keyboard_controller_proxy.h"

namespace {

// LayoutManager for the virtual keyboard container.  Manages a single window
// (the virtual keyboard) and keeps it positioned at the bottom of the
// container window.
class KeyboardLayoutManager : public aura::LayoutManager {
 public:
  KeyboardLayoutManager(aura::Window* owner, aura::Window* keyboard)
    : owner_(owner), keyboard_(keyboard) {}

  // Overridden from aura::LayoutManager
  virtual void OnWindowResized() OVERRIDE {
    gfx::Rect owner_bounds = owner_->bounds();
    gfx::Rect keyboard_bounds = gfx::Rect(
        owner_bounds.x(),
        owner_bounds.y() + owner_bounds.height() * 0.7,
        owner_bounds.width(),
        owner_bounds.height() * 0.3);
    SetChildBoundsDirect(keyboard_, keyboard_bounds);
  }
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE {
    CHECK(child == keyboard_);
  }
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE {}
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE {}
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE {}
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE {
    // Drop these: the size should only be set in OnWindowResized.
  }

 private:
  aura::Window* owner_;
  aura::Window* keyboard_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardLayoutManager);
};

}  // namespace

namespace keyboard {

KeyboardController::KeyboardController(KeyboardControllerProxy* proxy)
    : proxy_(proxy), container_(NULL) {
  CHECK(proxy);
}

KeyboardController::~KeyboardController() {}

aura::Window* KeyboardController::GetContainerWindow() {
  if (!container_) {
    container_ = new aura::Window(NULL);
    container_->SetName("KeyboardContainer");
    container_->Init(ui::LAYER_NOT_DRAWN);

    aura::Window* keyboard = proxy_->GetKeyboardWindow();
    keyboard->Show();

    container_->SetLayoutManager(
        new KeyboardLayoutManager(container_, keyboard));
    container_->AddChild(keyboard);
  }
  return container_;
}

}  // namespace keyboard
