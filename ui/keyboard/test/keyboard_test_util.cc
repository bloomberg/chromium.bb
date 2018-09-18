// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/test/keyboard_test_util.h"

#include "base/run_loop.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_observer.h"

namespace {

class WindowVisibilityChangeWaiter : public aura::WindowObserver {
 public:
  explicit WindowVisibilityChangeWaiter(aura::Window* window, bool wait_until)
      : window_(window), wait_until_(wait_until) {
    window_->AddObserver(this);
  }
  ~WindowVisibilityChangeWaiter() override { window_->RemoveObserver(this); }

  void Wait() { run_loop_.Run(); }

 private:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (window_ == window && visible == wait_until_) {
      run_loop_.QuitWhenIdle();
    }
  }

  aura::Window* window_;
  base::RunLoop run_loop_;
  bool const wait_until_;

  DISALLOW_COPY_AND_ASSIGN(WindowVisibilityChangeWaiter);
};

class ControllerStateChangeWaiter
    : public keyboard::KeyboardControllerObserver {
 public:
  explicit ControllerStateChangeWaiter(keyboard::KeyboardControllerState state)
      : controller_(keyboard::KeyboardController::Get()), state_(state) {
    controller_->AddObserver(this);
  }
  ~ControllerStateChangeWaiter() override { controller_->RemoveObserver(this); }

  void Wait() { run_loop_.Run(); }

 private:
  void OnStateChanged(const keyboard::KeyboardControllerState state) override {
    if (state == state_) {
      run_loop_.QuitWhenIdle();
    }
  }

  base::RunLoop run_loop_;
  keyboard::KeyboardController* controller_;
  keyboard::KeyboardControllerState state_;

  DISALLOW_COPY_AND_ASSIGN(ControllerStateChangeWaiter);
};

bool WaitVisibilityChangesTo(bool visibility) {
  aura::Window* keyboard_window =
      keyboard::KeyboardController::Get()->GetKeyboardWindow();
  if (keyboard_window->IsVisible() == visibility)
    return true;
  WindowVisibilityChangeWaiter waiter(keyboard_window, visibility);
  waiter.Wait();
  return true;
}

}  // namespace

namespace keyboard {

bool WaitUntilShown() {
  return WaitVisibilityChangesTo(true);
}

bool WaitUntilHidden() {
  return WaitVisibilityChangesTo(false);
}

void WaitControllerStateChangesTo(KeyboardControllerState state) {
  ControllerStateChangeWaiter waiter(state);
  waiter.Wait();
}

bool IsKeyboardShowing() {
  auto* keyboard_controller = keyboard::KeyboardController::Get();
  DCHECK(keyboard_controller->enabled());

  // KeyboardController sets its state to SHOWN when it is about to show.
  return keyboard_controller->GetStateForTest() ==
         KeyboardControllerState::SHOWN;
}

gfx::Rect KeyboardBoundsFromRootBounds(const gfx::Rect& root_bounds,
                                       int keyboard_height) {
  return gfx::Rect(root_bounds.x(), root_bounds.bottom() - keyboard_height,
                   root_bounds.width(), keyboard_height);
}

TestKeyboardUI::TestKeyboardUI(ui::InputMethod* input_method)
    : input_method_(input_method) {}

TestKeyboardUI::~TestKeyboardUI() {
  // Destroy the window before the delegate.
  window_.reset();
}

aura::Window* TestKeyboardUI::GetKeyboardWindow() {
  if (!window_) {
    window_.reset(new aura::Window(&delegate_));
    window_->Init(ui::LAYER_NOT_DRAWN);
    window_->set_owned_by_parent(false);
  }
  return window_.get();
}

ui::InputMethod* TestKeyboardUI::GetInputMethod() {
  return input_method_;
}

bool TestKeyboardUI::HasKeyboardWindow() const {
  return !!window_;
}

}  // namespace keyboard
