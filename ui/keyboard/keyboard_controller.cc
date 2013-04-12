// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_controller.h"

#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/keyboard/keyboard_controller_proxy.h"

namespace {

gfx::Rect KeyboardBoundsFromWindowBounds(const gfx::Rect& window_bounds) {
  const float kKeyboardHeightRatio = 0.3f;
  return gfx::Rect(
      window_bounds.x(),
      window_bounds.y() + window_bounds.height() * (1 - kKeyboardHeightRatio),
      window_bounds.width(),
      window_bounds.height() * kKeyboardHeightRatio);
}

// LayoutManager for the virtual keyboard container.  Manages a single window
// (the virtual keyboard) and keeps it positioned at the bottom of the
// container window.
class KeyboardLayoutManager : public aura::LayoutManager {
 public:
  KeyboardLayoutManager(aura::Window* owner, aura::Window* keyboard)
    : owner_(owner), keyboard_(keyboard) {}

  // Overridden from aura::LayoutManager
  virtual void OnWindowResized() OVERRIDE {
    SetChildBoundsDirect(keyboard_,
                         KeyboardBoundsFromWindowBounds(owner_->bounds()));
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

// The KeyboardWindowDelegate makes sure the keyboard-window does not get focus.
// This is necessary to make sure that the synthetic key-events reach the target
// window.
// The delegate deletes itself when the window is destroyed.
class KeyboardWindowDelegate : public aura::WindowDelegate {
 public:
  KeyboardWindowDelegate() {}
  virtual ~KeyboardWindowDelegate() {}

 private:
  // Overridden from aura::WindowDelegate:
  virtual gfx::Size GetMinimumSize() const OVERRIDE { return gfx::Size(); }
  virtual gfx::Size GetMaximumSize() const OVERRIDE { return gfx::Size(); }
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE {
    bounds_ = new_bounds;
  }
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) OVERRIDE {
    return gfx::kNullCursor;
  }
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return HTNOWHERE;
  }
  virtual bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) OVERRIDE {
    return true;
  }
  virtual bool CanFocus() OVERRIDE { return false; }
  virtual void OnCaptureLost() OVERRIDE {}
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {}
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {}
  virtual void OnWindowDestroying() OVERRIDE {}
  virtual void OnWindowDestroyed() OVERRIDE { delete this; }
  virtual void OnWindowTargetVisibilityChanged(bool visible) OVERRIDE {}
  virtual bool HasHitTestMask() const OVERRIDE { return true; }
  virtual void GetHitTestMask(gfx::Path* mask) const OVERRIDE {
    gfx::Rect keyboard_bounds = KeyboardBoundsFromWindowBounds(bounds_);
    mask->addRect(RectToSkRect(keyboard_bounds));
  }
  virtual scoped_refptr<ui::Texture> CopyTexture() OVERRIDE { return NULL; }

  gfx::Rect bounds_;
  DISALLOW_COPY_AND_ASSIGN(KeyboardWindowDelegate);
};

}  // namespace

namespace keyboard {

KeyboardController::KeyboardController(KeyboardControllerProxy* proxy)
    : proxy_(proxy), container_(NULL) {
  CHECK(proxy);
  proxy_->GetInputMethod()->AddObserver(this);
}

KeyboardController::~KeyboardController() {
  if (container_)
    container_->RemoveObserver(this);
  proxy_->GetInputMethod()->RemoveObserver(this);
}

aura::Window* KeyboardController::GetContainerWindow() {
  if (!container_) {
    container_ = new aura::Window(new KeyboardWindowDelegate());
    container_->SetName("KeyboardContainer");
    container_->Init(ui::LAYER_NOT_DRAWN);
    container_->AddObserver(this);

    aura::Window* keyboard = proxy_->GetKeyboardWindow();
    keyboard->Show();

    container_->SetLayoutManager(
        new KeyboardLayoutManager(container_, keyboard));
    container_->AddChild(keyboard);
  }
  return container_;
}

void KeyboardController::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  if (!container_)
    return;

  if (!client || client->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE) {
    container_->Hide();
  } else {
    container_->parent()->StackChildAtTop(container_);
    container_->Show();
  }

  // TODO(bryeung): whenever the TextInputClient changes we need to notify the
  // keyboard (with the TextInputType) so that it can reset it's state (e.g.
  // abandon compositions in progress)
}

void KeyboardController::OnWindowParentChanged(aura::Window* window,
                                               aura::Window* parent) {
  OnTextInputStateChanged(proxy_->GetInputMethod()->GetTextInputClient());
}

void KeyboardController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(container_, window);
  container_ = NULL;
}

}  // namespace keyboard
