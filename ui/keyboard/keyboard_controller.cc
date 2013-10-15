// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_controller.h"

#include "base/bind.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/keyboard/keyboard_controller_proxy.h"
#include "ui/keyboard/keyboard_util.h"

namespace {

const int kHideKeyboardDelayMs = 100;

gfx::Rect KeyboardBoundsFromWindowBounds(const gfx::Rect& window_bounds) {
  const float kKeyboardHeightRatio = 0.3f;
  return gfx::Rect(
      window_bounds.x(),
      window_bounds.y() + window_bounds.height() * (1 - kKeyboardHeightRatio),
      window_bounds.width(),
      window_bounds.height() * kKeyboardHeightRatio);
}

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
  virtual void DidRecreateLayer(ui::Layer* old_layer,
                                ui::Layer* new_layer) OVERRIDE {}

  gfx::Rect bounds_;
  DISALLOW_COPY_AND_ASSIGN(KeyboardWindowDelegate);
};

}  // namespace

namespace keyboard {

// LayoutManager for the virtual keyboard container.  Manages a single window
// (the virtual keyboard) and keeps it positioned at the bottom of the
// owner window.
class KeyboardLayoutManager : public aura::LayoutManager {
 public:
  KeyboardLayoutManager(aura::Window* container)
      : container_(container), keyboard_(NULL) {
    CHECK(container_);
  }

  // Overridden from aura::LayoutManager
  virtual void OnWindowResized() OVERRIDE {
    if (!keyboard_)
      return;
    SetChildBoundsDirect(keyboard_,
                         KeyboardBoundsFromWindowBounds(container_->bounds()));
  }
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE {
    DCHECK(!keyboard_);
    keyboard_ = child;
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
  aura::Window* container_;
  aura::Window* keyboard_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardLayoutManager);
};

KeyboardController::KeyboardController(KeyboardControllerProxy* proxy)
    : proxy_(proxy),
      input_method_(NULL),
      keyboard_visible_(false),
      weak_factory_(this) {
  CHECK(proxy);
  input_method_ = proxy_->GetInputMethod();
  input_method_->AddObserver(this);
}

KeyboardController::~KeyboardController() {
  if (container_.get())
    container_->RemoveObserver(this);
  if (input_method_)
    input_method_->RemoveObserver(this);
}

aura::Window* KeyboardController::GetContainerWindow() {
  if (!container_.get()) {
    container_.reset(new aura::Window(new KeyboardWindowDelegate()));
    container_->SetName("KeyboardContainer");
    container_->set_owned_by_parent(false);
    container_->Init(ui::LAYER_NOT_DRAWN);
    container_->AddObserver(this);
    container_->SetLayoutManager(new KeyboardLayoutManager(container_.get()));
  }
  return container_.get();
}

void KeyboardController::HideKeyboard(HideReason reason) {
  keyboard_visible_ = false;

  keyboard::LogKeyboardControlEvent(
      reason == HIDE_REASON_AUTOMATIC ?
          keyboard::KEYBOARD_CONTROL_HIDE_AUTO :
          keyboard::KEYBOARD_CONTROL_HIDE_USER);

  FOR_EACH_OBSERVER(KeyboardControllerObserver,
                    observer_list_,
                    OnKeyboardBoundsChanging(gfx::Rect()));

  proxy_->HideKeyboardContainer(container_.get());
}

void KeyboardController::AddObserver(KeyboardControllerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void KeyboardController::RemoveObserver(KeyboardControllerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void KeyboardController::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  if (params.new_parent && params.target == container_.get())
    OnTextInputStateChanged(proxy_->GetInputMethod()->GetTextInputClient());
}

void KeyboardController::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  if (!container_.get())
    return;

  bool was_showing = keyboard_visible_;
  bool should_show = was_showing;
  ui::TextInputType type =
      client ? client->GetTextInputType() : ui::TEXT_INPUT_TYPE_NONE;
  if (type == ui::TEXT_INPUT_TYPE_NONE) {
    should_show = false;
  } else {
    if (container_->children().empty()) {
      aura::Window* keyboard = proxy_->GetKeyboardWindow();
      keyboard->Show();
      container_->AddChild(keyboard);
      container_->layout_manager()->OnWindowResized();
    }
    proxy_->SetUpdateInputType(type);
    container_->parent()->StackChildAtTop(container_.get());
    should_show = true;
  }

  if (was_showing != should_show) {
    if (should_show) {
      keyboard_visible_ = true;

      // If the controller is in the process of hiding the keyboard, do not log
      // the stat here since the keyboard will not actually be shown.
      if (!WillHideKeyboard())
        keyboard::LogKeyboardControlEvent(keyboard::KEYBOARD_CONTROL_SHOW);

      weak_factory_.InvalidateWeakPtrs();
      if (container_->IsVisible())
        return;

      FOR_EACH_OBSERVER(
          KeyboardControllerObserver,
          observer_list_,
          OnKeyboardBoundsChanging(container_->children()[0]->bounds()));
      proxy_->ShowKeyboardContainer(container_.get());
    } else {
      // Set the visibility state here so that any queries for visibility
      // before the timer fires returns the correct future value.
      keyboard_visible_ = false;
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&KeyboardController::HideKeyboard,
                     weak_factory_.GetWeakPtr(), HIDE_REASON_AUTOMATIC),
          base::TimeDelta::FromMilliseconds(kHideKeyboardDelayMs));
    }
  }
  // TODO(bryeung): whenever the TextInputClient changes we need to notify the
  // keyboard (with the TextInputType) so that it can reset it's state (e.g.
  // abandon compositions in progress)
}

void KeyboardController::OnInputMethodDestroyed(
    const ui::InputMethod* input_method) {
  DCHECK_EQ(input_method_, input_method);
  input_method_ = NULL;
}

bool KeyboardController::WillHideKeyboard() const {
  return weak_factory_.HasWeakPtrs();
}

}  // namespace keyboard
